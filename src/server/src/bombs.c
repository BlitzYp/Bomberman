#include "../include/bombs.h"

#include <stdint.h>

// ~50ms, can change this later
#define EXPLOSION_DURATION_TICKS 5

void update_bomb_timers(server_t* server)
{
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        bomb_t* bomb=&server->state.bombs[i];

        if (bomb->state==BOMB_PLANTED && bomb->timer_ticks>0) {
            bomb->timer_ticks--;
        } else if (bomb->state==BOMB_EXPLODING && bomb->explosion_ticks>0) {
            bomb->explosion_ticks--;
        }
    }
}

void collect_bomb_events(server_t* server, bool* exploding_bombs, bool* end_exploding_bombs)
{
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        bomb_t* bomb=&server->state.bombs[i];

        if (bomb->state==BOMB_PLANTED && bomb->timer_ticks==0) {
            exploding_bombs[i]=true;
            bomb->state=BOMB_EXPLODING;
            bomb->explosion_ticks=EXPLOSION_DURATION_TICKS;
        } else if (bomb->state==BOMB_EXPLODING && bomb->explosion_ticks==0) {
            uint8_t owner_id=bomb->owner_id;
            if (owner_id<MAX_PLAYERS) {
                player_slot_t* slot=&server->state.players[owner_id];
                if (slot->p.bomb_count<1) slot->p.bomb_count++;
            }
            end_exploding_bombs[i]=true;
            bomb->state=BOMB_INACTIVE;
            bomb->timer_ticks=0;
            bomb->explosion_ticks=0;
        }
    }
}

static bool valid_cell(map_t* map,int r,int c)
{
    return r>=0 && c>=0 && r<map->rows && c<map->cols;
}

static bool mark_explosion_start_cell(game_state_t* state,int r,int c,bool* spawned_bonus,bonus_type_t* spawned_type)
{
    // false means explosion propagation ends in this cell
    if (spawned_bonus) *spawned_bonus=false;
    if (spawned_type) *spawned_type=BONUS_NONE;
    if (!valid_cell(&state->map,r,c)) return false;

    uint16_t cell=make_cell_index((uint16_t)r,(uint16_t)c,state->map.cols);
    tile_t tile=state->map.tiles[cell];

    if (tile==TILE_HARD_WALL) return false;

    state->map.tiles[cell]=TILE_BOMB_EXPLODE;
    if (tile==TILE_SOFT_BLOCK) {
        if (state->bonuses[cell]==BONUS_NONE) {
            state->bonuses[cell]=BONUS_RADIUS;
            if (spawned_bonus) *spawned_bonus=true;
            if (spawned_type) *spawned_type=BONUS_RADIUS;
        }

        return false;
    }

    return true;
}

bool mark_explosion_end_cell(map_t* map,int r,int c)
{
    if (!valid_cell(map,r,c)) return false;

    uint16_t cell=make_cell_index((uint16_t)r,(uint16_t)c,map->cols);
    tile_t tile=map->tiles[cell];

    if (tile!=TILE_BOMB_EXPLODE) return false;

    map->tiles[cell]=TILE_EMPTY;
    return true;
}

void apply_explosion_start(server_t* server, bomb_t* bomb,bool* bonus_cells_changed,bonus_type_t* available_cell_types)
{
    // left,right,up,down
    int8_t dr[4]={0,0,-1,1};
    int8_t dc[4]={-1,1,0,0};
    mark_explosion_start_cell(&server->state,bomb->row,bomb->col,NULL,NULL);

    for (uint8_t dir=0;dir<4;dir++) {
        for (uint8_t dist=1;dist<=bomb->radius;dist++) {
            int r=(int)bomb->row+dr[dir]*dist;
            int c=(int)bomb->col+dc[dir]*dist;
            bool spawned_bonus=false;
            bonus_type_t spawned_type=BONUS_NONE;

            // Attempt to start an explosion from this cell
            if (!mark_explosion_start_cell(&server->state,r,c,&spawned_bonus,&spawned_type)) {
                if (spawned_bonus) {
                    uint16_t cell_index=make_cell_index((uint16_t)r,(uint16_t)c,server->state.map.cols);
                    bonus_cells_changed[cell_index]=true;
                    available_cell_types[cell_index]=spawned_type;
                }
                break;
            }
        }
    }
}

void apply_explosion_end(server_t* server, bomb_t* bomb)
{
    // left,right,up,down
    int8_t dr[4]={0,0,-1,1};
    int8_t dc[4]={-1,1,0,0};
    mark_explosion_end_cell(&server->state.map,bomb->row,bomb->col);

    for (uint8_t dir=0;dir<4;dir++) {
        for (uint8_t dist=1;dist<=bomb->radius;dist++) {
            int r=(int)bomb->row+dr[dir]*dist;
            int c=(int)bomb->col+dc[dir]*dist;

            // Attempt to clear explosion in this cell
            if (!mark_explosion_end_cell(&server->state.map,r,c)) break;
        }
    }
}
