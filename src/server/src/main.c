#include "../include/server.h"

#include <stdlib.h>
#include <stdio.h>

int main(int argc,char** argv)
{
    int port=1727;

    if (argc>1) {
        port=atoi(argv[1]);
        if (port<=0 || port>65535) {
            fprintf(stderr,"Invalid port: %s\n",argv[1]);
            return -1;
        }
    }

    server_t server;
    if (init_server(&server, port)!=0) {
        fprintf(stderr, "Failed to start server!\n");
        return -1;
    }

    run_server(&server);
    shutdown_server(&server);
    return 0;
}
