#include "../include/player_states.h"
#include "../include/server.h"


#include <stdio.h>

int main(int argc,char** argv)
{
    int port=1727;
    server_t server;
    if (init_server(&server, port)!=0) {
        fprintf(stderr, "Failed to start server!\n");
        return -1;
    }

    run_server(&server);
    shutdown_server(&server);
    return 0;
}
