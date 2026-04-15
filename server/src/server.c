/*
TODO:
    - Game simulation in the tick function
*/
#define _DEFAULT_SOURCE
#include "../include/server.h"
#include "../../shared/include/protocol.h"
#include "../../shared/include/net_utils.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct {
    server_t* server;
    int client_fd;
    uint8_t slot_id; // player_id
} client_thread_args_t;

static int get_listen_socket(int port)
{
    int fd=socket(AF_INET,SOCK_STREAM,0);
    if (fd<0) {
        perror("Error creating socket for listening...");
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons((uint16_t)port);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);

    if (bind(fd,(struct sockaddr*)&addr,sizeof(addr))<0) {
        perror("Error binding socket..");
        close(fd);
        return -1;
    }

    if (listen(fd,16)<0) {
        perror("Error listening to socket...");
        close(fd);
        return -1;
    }

    return fd;
}

static bool server_is_running(server_t* server)
{
    bool running;

    pthread_mutex_lock(&server->state.mutex);
    running=server->state.running;
    pthread_mutex_unlock(&server->state.mutex);

    return running;
}

int init_server(server_t* server,int port)
{
    if (!server) return -1;

    memset(server,0,sizeof(*server));
    server->listen_fd=-1;
    server->port=port;

    if (game_state_init(&server->state)!=0) return -1;

    server->listen_fd=get_listen_socket(port);
    if (server->listen_fd<0) {
        game_state_destroy(&server->state);
        return -1;
    }

    return 0;
}

void shutdown_server(server_t* server)
{
    if (!server) return;

    pthread_mutex_lock(&server->state.mutex);
    server->state.running=false;
    for (uint8_t i=0;i<MAX_PLAYERS;i++)
        if (server->state.players[i].socket_fd>=0)
            shutdown(server->state.players[i].socket_fd,SHUT_RDWR);
    pthread_mutex_unlock(&server->state.mutex);

    if (server->listen_fd>=0) {
        close(server->listen_fd);
        server->listen_fd=-1;
    }

    if (server->tick_thread) {
        pthread_join(server->tick_thread,NULL);
        server->tick_thread=0;
    }

    while (server_is_running(server)==false) {
        uint8_t active_players;

        pthread_mutex_lock(&server->state.mutex);
        active_players=server->state.player_count;
        pthread_mutex_unlock(&server->state.mutex);

        if (active_players==0) break;
        usleep(1000);
    }

    game_state_destroy(&server->state);
}

// Makes the server operate at 20 ticks per second
static void* tick_thread_loop(void* arg)
{
    server_t* server=(server_t*)arg;
    useconds_t sleep_time=1000000/TICKS_PER_SECOND;

    for (;;) {
        usleep(sleep_time);

        pthread_mutex_lock(&server->state.mutex);
        if (!server->state.running) {
            pthread_mutex_unlock(&server->state.mutex);
            break;
        }

        // Simulation here
        // TODO: authoritative game simulation:
        // - process queued actions
        // - update bomb timers
        // - resolve explosions
        // - detect deaths
        // - check win condition
        pthread_mutex_unlock(&server->state.mutex);
    }
    return NULL;
}

void init_slot(player_slot_t* slot,int client_fd,uint8_t id)
{
    slot->connected=true;
    slot->alive=true;
    slot->ready=false;
    slot->socket_fd=client_fd;
    slot->id=id;
    slot->name[0]='\0';
}

static void free_player_slot(server_t* server, uint8_t slot_id)
{
    pthread_mutex_lock(&server->state.mutex);
    player_slot_t* slot=&server->state.players[slot_id];
    if (slot->connected) {
        slot->connected=false;
        slot->ready=false;
        slot->alive=false;
        slot->socket_fd=-1;
        slot->name[0]='\0';
        if (server->state.player_count>0) server->state.player_count--;
    }
    pthread_mutex_unlock(&server->state.mutex);
}

static void cleanup(server_t* server,int client_fd,uint8_t slot_id)
{
    close(client_fd);
    free_player_slot(server,slot_id);
}

static int send_welcome(server_t* server, int client_fd, uint8_t slot_id)
{
    typedef struct {
        uint8_t id;
        uint8_t ready;
        char name[MAX_NAME_LEN];
    } player_info;

    msg_header_t header;
    uint8_t status;
    char server_id[MAX_CLIENT_ID_LEN]="server";
    player_info players[MAX_PLAYERS];
    uint8_t player_count=0;

    pthread_mutex_lock(&server->state.mutex);
    status=(uint8_t)server->state.status;
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* slot=&server->state.players[i];
        if (!slot->connected) continue;
        players[player_count].id=slot->id;
        players[player_count].ready=slot->ready?1:0;
        memcpy(players[player_count].name,slot->name,MAX_NAME_LEN);
        player_count++;
    }
    pthread_mutex_unlock(&server->state.mutex);

    header.msg_type=MSG_WELCOME;
    header.sender_id=slot_id;
    header.target_id=slot_id;

    if (send_header(client_fd,&header)<0) return -1;
    if (write_exact(client_fd, server_id, MAX_CLIENT_ID_LEN)<0) return -1;
    if (send_u8(client_fd,status)<0) return -1;
    if (send_u8(client_fd,player_count)<0) return -1;

    for (uint8_t i=0;i<player_count;i++) {
        if (send_u8(client_fd,players[i].id)<0) return -1;
        if (send_u8(client_fd,players[i].ready)<0) return -1;
        if (write_exact(client_fd, players[i].name,MAX_NAME_LEN)<0) return -1;
    }

    return 0;
}

static int handle_hello(server_t* server,int client_fd,uint8_t slot_id,msg_header_t header)
{
    msg_hello_t msg;

    msg.header=header;
    if (read_exact(client_fd,msg.client_id,MAX_CLIENT_ID_LEN)<0) return -1;
    if (read_exact(client_fd,msg.player_name,MAX_NAME_LEN)<0) return -1;

    pthread_mutex_lock(&server->state.mutex);
    // TODO: Implement check if the name is valid and is unique
    if (true) {
        memcpy(server->state.players[slot_id].name,msg.player_name,MAX_NAME_LEN);
        server->state.players[slot_id].name[MAX_NAME_LEN]='\0';
    }
    pthread_mutex_unlock(&server->state.mutex);

    if (send_welcome(server,client_fd,slot_id)!=0) return -1;

    printf("HELLO FROM SLOT %u\n",slot_id);
    return 0;
}

// PROCESS MAIN CLIENT THREAD HERE
static void* client_thread_main(void* arg)
{
    client_thread_args_t* args=(client_thread_args_t*)arg;
    server_t* server=args->server;
    int client_fd=args->client_fd;
    uint8_t slot_id=args->slot_id;
    bool connection_active=true;
    free(args);
    // Process incoming messages from client
    while (connection_active) {
        msg_header_t header;
        if (get_header(client_fd,&header)<0) {
            connection_active=false;
            break;
        }

        switch (header.msg_type) {
            case MSG_HELLO: {
                if (handle_hello(server,client_fd,slot_id,header)<0) connection_active=false;
                break;
            }
            case MSG_LEAVE:
                connection_active=false;
                break;
            default: {
                printf("Unknown request type %u from slot %u\n",header.msg_type,slot_id);
                connection_active=false;
                break;
            }
        }
    }

    cleanup(server,client_fd,slot_id);
    return NULL;
}

int run_server(server_t* server)
{
    if (pthread_create(&server->tick_thread,NULL,tick_thread_loop,server)!=0) return -1;
    printf("Starting bomberman server on port %u\n",server->port);

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len=sizeof(client_addr);
        int client_fd=accept(server->listen_fd,(struct sockaddr*)&client_addr,&client_addr_len);
        if (client_fd<0) {
            if (!server_is_running(server)) break;
            if (errno==EINTR) continue;
            perror("Error accepting client..");
            return -1;
        }

        pthread_mutex_lock(&server->state.mutex);
        bool slot_found=false;
        uint8_t i;
        for (i=0;i<MAX_PLAYERS;i++) {
            player_slot_t* curr_slot=&server->state.players[i];
            // Curr slot is free
            if (!curr_slot->connected) {
                init_slot(curr_slot,client_fd,i);
                server->state.player_count++;
                slot_found=true;
                break;
            }
        }

        // No free slot found
        if (!slot_found) {
            pthread_mutex_unlock(&server->state.mutex);
            close(client_fd);
            continue;
        }
        pthread_mutex_unlock(&server->state.mutex);

        client_thread_args_t* args=malloc(sizeof(client_thread_args_t));
        if (!args) {
            close(client_fd);
            free_player_slot(server,i);
            continue;
        }

        args->server=server;
        args->client_fd=client_fd;
        args->slot_id=i;

        pthread_t client_thread;
        if (pthread_create(&client_thread,NULL,client_thread_main,args)!=0) {
            uint8_t slot_id=args->slot_id;
            free(args);
            close(client_fd);
            free_player_slot(server,slot_id);
            continue;
        }

        // Make the client independant
        pthread_detach(client_thread);
    }
    return 0;
}
