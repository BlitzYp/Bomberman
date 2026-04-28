#define _DEFAULT_SOURCE
#include "../include/server.h"
#include "../../shared/include/protocol.h"
#include "../../shared/include/net_utils.h"
#include "../include/player_actions.h"
#include "../include/server_messages.h"
#include "../include/bombs.h"
#include "../include/lobby.h"

#include <ncurses.h>
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

    int reuse=1;
    if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse))<0) {
        perror("Error setting socket reuse option...");
        close(fd);
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

    for(;;) {
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
        bool moved_players[MAX_PLAYERS]={0};
        bool placed_bombs[MAX_BOMBS]={0};
        bool start_bombs[MAX_BOMBS]={0};
        bool pending_starts[MAX_BOMBS]={0};
        bool started_this_tick[MAX_BOMBS]={0};
        bool end_exploding_bombs[MAX_BOMBS]={0};
        bool dead_players[MAX_PLAYERS]={0};

        bool bonus_collected_players[MAX_PLAYERS]={0};
        uint16_t collected_bonus_cells[MAX_PLAYERS];
        for (uint8_t i=0;i<MAX_PLAYERS;i++) collected_bonus_cells[i]=UINT16_MAX;

        bool winner_found=false;
        uint8_t winner_id=SERVER_TARGET_ID;
        bool game_end=false;

        pthread_mutex_lock(&server->state.mutex);
        if (!server->state.running) {
            pthread_mutex_unlock(&server->state.mutex);
            break;
        }
        if (server->state.status!=GAME_RUNNING) {
            pthread_mutex_unlock(&server->state.mutex);
            continue;
        }
        uint32_t cell_count=(uint32_t)server->state.map.rows*server->state.map.cols;
        bonus_type_t available_cell_types[cell_count];
        bool bonus_cells_changed[cell_count];
        bool destroyed_block_cells[cell_count];
        memset(destroyed_block_cells,0,sizeof(destroyed_block_cells));
        memset(bonus_cells_changed,0,sizeof(bonus_cells_changed));
        memset(available_cell_types,0,sizeof(available_cell_types));
        //-- Process queued actions
        bool map_change=false;
        action_t action;
        while (dequeue_action(&server->state,&action)==0) {
            if (action.type==ACTION_MOVE) {
                player_slot_t* slot=&server->state.players[action.player_id];
                bonus_type_t collected_bonus_type=BONUS_NONE;
                uint16_t collected_bonus_cell=UINT16_MAX;
                if (!slot->connected || !slot->alive) continue;

                handle_action_move(server,slot,action,&collected_bonus_type,&collected_bonus_cell);
                if (collected_bonus_type!=BONUS_NONE) {
                    bonus_collected_players[slot->id]=true;
                    collected_bonus_cells[slot->id]=collected_bonus_cell;
                }
                moved_players[slot->id]=true;
            }
            else if (action.type==ACTION_BOMB) {
                bomb_t* bomb=find_free_bomb(&server->state);
                if (!bomb) continue;
                if (handle_action_bomb(server,bomb,action)) {
                    uint8_t bomb_id=(uint8_t)(bomb-server->state.bombs);
                    placed_bombs[bomb_id]=true;

                    map_change=true;
                }
            }
        }

        // - update bomb timer
        update_bomb_timers(server);

        // -- resolve explosions
        collect_bomb_events(server,start_bombs,end_exploding_bombs);
        for (uint8_t i=0;i<MAX_BOMBS;i++) {
            if (start_bombs[i]) {
                pending_starts[i]=true;
                started_this_tick[i]=true;
            }
            if (end_exploding_bombs[i]) {
                apply_explosion_end(server,&server->state.bombs[i]);
                map_change=true;
            }
        }

        // Start the initial explosions and and continue till the chain explosions are also done
        bool found_pending;
        do {
            found_pending=false;
            for (uint8_t i=0;i<MAX_BOMBS;i++) {
                if (!pending_starts[i]) continue;

                pending_starts[i]=false;
                apply_explosion_start(server,&server->state.bombs[i],destroyed_block_cells,bonus_cells_changed,available_cell_types,pending_starts,started_this_tick);
                map_change=true;
                found_pending=true;
            }
        } while (found_pending);

        uint8_t alive_count=0,last_alive_player=SERVER_TARGET_ID;
        // - detect deaths and count alive players
        for (uint8_t i=0;i<MAX_PLAYERS;i++) {
            player_slot_t* p=&server->state.players[i];
            if (!p->connected || !p->alive) continue;

            uint16_t cell=make_cell_index(p->p.row,p->p.col,server->state.map.cols);
            if (server->state.map.tiles[cell]==TILE_BOMB_EXPLODE) {
                p->alive=false;
                p->p.alive=false;
                dead_players[i]=true;
                continue;
            }
            alive_count++;
            last_alive_player=i;
        }
        if (alive_count<=1) {
            server->state.status=GAME_END;
            game_end=true;
            if (alive_count==1) {
                winner_found=true;
                winner_id=last_alive_player;
            }
        }

        // - check win condition
        pthread_mutex_unlock(&server->state.mutex);

        // Send move message to all here to avoid deadlock situation
        send_move_broadcast(server,moved_players);

        // All of the map changes are recorded when we broadcast the new map, we can use these functions later for sound effects maybe
        // For now I think we should discard them in the client
        send_bomb_broadcast(server,placed_bombs);
        send_exploding_broadcast(server,started_this_tick);
        send_end_explode_broadcast(server,end_exploding_bombs);

        // - Send player alive statuses
        send_death_broadcast(server,dead_players);

        // - Update bonuses for players
        send_bonus_retrieved_broadcast(server,bonus_collected_players,collected_bonus_cells);

        // - Notify destroyed soft blocks
        send_block_destroyed_broadcast(server,destroyed_block_cells);

        // - Update avaialable bonuses
        send_bonus_available_broadcast(server,bonus_cells_changed,available_cell_types);

        if (game_end) send_status_broadcast(server,GAME_END);
        if (winner_found) send_winner_broadcast(server, winner_id);
        if (map_change) broadcast_map(server);
    }
    return NULL;
}

void init_slot(player_slot_t* slot,int client_fd,uint8_t id,map_t* map,game_status_t status)
{
    slot->connected=true;
    bool joined_mid_round=(status==GAME_RUNNING);
    slot->alive=!joined_mid_round;
    slot->ready=false;
    slot->socket_fd=client_fd;
    slot->id=id;
    slot->name[0]='\0';
    slot->p.id=id;
    slot->p.alive=!joined_mid_round;
    slot->p.ready=false;

    slot->p.bomb_count=1;
    slot->p.bomb_radius=1;
    slot->p.bomb_capacity=1;
    slot->p.bomb_timer_ticks=3*TICKS_PER_SECOND;
    slot->p.speed=3;

    if (id<map->spawn_count && map->spawn_cells[id]!=UINT16_MAX) {
        uint16_t cell=map->spawn_cells[id];
        slot->p.row=cell/map->cols;
        slot->p.col=cell%map->cols;
    }
    else {
        slot->p.row=1;
        slot->p.col=(uint16_t)(1+id);
    }
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

static void cleanup(server_t* server,int client_fd,uint8_t slot_id,bool joined)
{
    if (joined) {
        if (send_leave_broadcast(server,slot_id)<0) {
            printf("Error broadcasting leave for slot %u\n",slot_id);
        }
    }

    close(client_fd);
    free_player_slot(server,slot_id);
}

// PROCESS MAIN CLIENT THREAD HERE
static void* client_thread_main(void* arg)
{
    client_thread_args_t* args=(client_thread_args_t*)arg;
    server_t* server=args->server;
    int client_fd=args->client_fd;
    uint8_t slot_id=args->slot_id;
    bool connection_active=true;
    bool joined=false;
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
                if (handle_hello(server,client_fd,slot_id,header)<0) {
                    send_disconnect(client_fd,slot_id);
                    connection_active=false;
                }
                else joined=true;
                break;
            }
            case MSG_MOVE_ATTEMPT:
                if (handle_move(server,client_fd,slot_id,header)<0) {
                    send_disconnect(client_fd,slot_id);
                    connection_active=false;
                }
                break;
            case MSG_BOMB_ATTEMPT:
                if (handle_bomb(server,client_fd,slot_id,header)<0) {
                    send_disconnect(client_fd,slot_id);
                    connection_active=false;
                }
                break;
            case MSG_SET_READY:
                if (handle_set_ready(server,slot_id)<0) printf("Ignoring SET_READY from slot %u in invalid state\n",slot_id);

                break;
            case MSG_LEAVE:
                connection_active=false;
                break;
            default: {
                printf("Unknown request type %u from slot %u\n",header.msg_type,slot_id);
                send_disconnect(client_fd,slot_id);
                connection_active=false;
                break;
            }
        }
    }

    cleanup(server,client_fd,slot_id,joined);
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
                init_slot(curr_slot,client_fd,i,&server->state.map,server->state.status);
                server->state.player_count++;
                slot_found=true;
                break;
            }
        }

        // No free slot found
        if (!slot_found) {
            pthread_mutex_unlock(&server->state.mutex);
            send_disconnect(client_fd,SERVER_TARGET_ID);
            close(client_fd);
            continue;
        }
        pthread_mutex_unlock(&server->state.mutex);

        client_thread_args_t* args=malloc(sizeof(client_thread_args_t));
        if (!args) {
            send_disconnect(client_fd,i);
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
            send_disconnect(client_fd,slot_id);
            close(client_fd);
            free_player_slot(server,slot_id);
            continue;
        }

        // Make the client independant
        pthread_detach(client_thread);
    }
    return 0;
}
