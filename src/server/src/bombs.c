#include "../include/bombs.h"

#include <stdint.h>

void update_bomb_timers(server_t* server)
{
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        if (server->state.bombs[i].active && server->state.bombs[i].timer_ticks>0) {
            server->state.bombs[i].timer_ticks--;
        }
    }
}

void collect_bomb_events(server_t* server, bool* exploding_bombs, bool* end_exploding_bombs)
{
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        if (server->state.bombs[i].active && server->state.bombs[i].timer_ticks==0) {
            exploding_bombs[i]=true;
            server->state.bombs[i].active=0;
            server->state.bombs[i].timer_ticks++;
        } else if (!server->state.bombs[i].active && server->state.bombs[i].timer_ticks==1) {
            end_exploding_bombs[i]=true;
        }
    }
}

void apply_explosion_start(server_t* server, bomb_t* slot)
{
    uint16_t cell_index=make_cell_index(slot->row,slot->col,server->state.map.cols);

    server->state.map.tiles[cell_index]=TILE_BOMB_EXPLODE;
    int index=0, radius=slot->radius;
    for (int i=1;i<radius+1;i++) {
        index=cell_index+i;
        if (cell_index/server->state.map.cols!=index/server->state.map.cols) break;
        if (server->state.map.tiles[index]!=TILE_HARD_WALL) {
            server->state.map.tiles[index]=TILE_BOMB_EXPLODE;
        } else if (server->state.map.tiles[index]==TILE_HARD_WALL) {
            break;
        }
    }
    for (int i=1;i<radius+1;i++) {
        index=cell_index+i*server->state.map.cols;
        if (cell_index%server->state.map.cols!=index%server->state.map.cols) break;
        if (server->state.map.tiles[index]!=TILE_HARD_WALL) {
            server->state.map.tiles[index]=TILE_BOMB_EXPLODE;
        } else if (server->state.map.tiles[index]==TILE_HARD_WALL) {
            break;
        }
    }
    for (int i=-1;i>(-1*(radius+1));i--) {
        index=cell_index+i;
        if (cell_index/server->state.map.cols!=index/server->state.map.cols) break;
        if (server->state.map.tiles[index]!=TILE_HARD_WALL) {
            server->state.map.tiles[index]=TILE_BOMB_EXPLODE;
        } else if (server->state.map.tiles[index]==TILE_HARD_WALL) {
            break;
        }
    }
    for (int i=-1;i>(-1*(radius+1));i--) {
        index=cell_index+i*server->state.map.cols;
        if (cell_index%server->state.map.cols!=index%server->state.map.cols) break;
        if (server->state.map.tiles[index]!=TILE_HARD_WALL) {
            server->state.map.tiles[index]=TILE_BOMB_EXPLODE;
        } else if (server->state.map.tiles[index]==TILE_HARD_WALL) {
            break;
        }
    }
}

void apply_explosion_end(server_t* server, bomb_t* slot)
{
    uint16_t cell_index=make_cell_index(slot->row,slot->col,server->state.map.cols);

    server->state.map.tiles[cell_index]=TILE_EMPTY;
    int index=1;
    while (true) {
        if (server->state.map.tiles[cell_index+index]==TILE_BOMB_EXPLODE) {
            server->state.map.tiles[cell_index+index]=TILE_EMPTY;
            index++;
        }
        if (server->state.map.tiles[cell_index+index]!=TILE_BOMB_EXPLODE) break;
    }
    index=-1;
    while (true) {
        if (server->state.map.tiles[cell_index+index]==TILE_BOMB_EXPLODE) {
            server->state.map.tiles[cell_index+index]=TILE_EMPTY;
            index--;
        }
        if (server->state.map.tiles[cell_index+index]!=TILE_BOMB_EXPLODE) break;
    }
    index=1;
    while (true) {
        if (server->state.map.tiles[cell_index+index*server->state.map.cols]==TILE_BOMB_EXPLODE) {
            server->state.map.tiles[cell_index+index*server->state.map.cols]=TILE_EMPTY;
            index++;
        }
        if (server->state.map.tiles[cell_index+index*server->state.map.cols]!=TILE_BOMB_EXPLODE) break;
    }
    index=-1;
    while (true) {
        if (server->state.map.tiles[cell_index+index*server->state.map.cols]==TILE_BOMB_EXPLODE) {
            server->state.map.tiles[cell_index+index*server->state.map.cols]=TILE_EMPTY;
            index--;
        }
        if (server->state.map.tiles[cell_index+index*server->state.map.cols]!=TILE_BOMB_EXPLODE) break;
    }
}
