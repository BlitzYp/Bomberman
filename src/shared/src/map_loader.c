#include "../include/map_loader.h"
#include "../include/config.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_tile_token(const char* token, tile_t* tile, bool* is_spawn, uint8_t* spawn_id)
{
    if (!token || !tile || !is_spawn || !spawn_id) return -1;

    *is_spawn=false;
    *spawn_id=0;

    if (strcmp(token,".")==0) {
        *tile=TILE_EMPTY;
        return 0;
    }

    if (strcmp(token,"H")==0) {
        *tile=TILE_HARD_WALL;
        return 0;
    }

    if (strcmp(token,"S")==0) {
        *tile=TILE_SOFT_BLOCK;
        return 0;
    }

    // Treat bonus like tokens like empty for now
    if (strcmp(token,"A")==0 || strcmp(token,"R")==0 || strcmp(token,"T")==0) {
        *tile=TILE_EMPTY;
        return 0;
    }

    if (strlen(token)==1 && isdigit((unsigned char)token[0])) {
        uint8_t id=(uint8_t)(token[0]-'0');
        if (id>=MAX_PLAYERS) return -1;

        *tile=TILE_EMPTY;
        *is_spawn=true;
        *spawn_id=id;
        return 0;
    }

    return -1;
}

void map_destroy(map_t* map)
{
    if (!map) return;

    free(map->tiles);
    memset(map,0,sizeof(*map));
}

tile_t map_get_tile(const map_t* map, uint16_t row, uint16_t col)
{
    if (!map || !map->tiles || row>=map->rows || col>=map->cols) return TILE_HARD_WALL;
    return map->tiles[make_cell_index(row,col,map->cols)];
}

bool map_is_walkable(const map_t* map, uint16_t row, uint16_t col)
{
    return map_get_tile(map,row,col)==TILE_EMPTY;
}

int map_load_from_file(const char* path, map_t* map)
{

    if (!path || !map) return -1;
    memset(map,0,sizeof(*map));

    FILE* file=fopen(path,"r");
    if (!file) return -1;

    uint16_t rows,cols;
    if (fscanf(file,"%hu %hu",&rows,&cols)!=2 || rows==0 || cols==0) {
        fclose(file);
        return -1;
    }
    for (;;) {
        int ch=fgetc(file);
        if (ch=='\n' || ch==EOF) break;
    }

    map->rows=rows;
    map->cols=cols;
    map->tiles=malloc((size_t)rows*cols*sizeof(*map->tiles));
    if (!map->tiles) {
        fclose(file);
        memset(map,0,sizeof(*map));
        return -1;
    }

    for (uint8_t i=0;i<MAX_PLAYERS;i++) map->spawn_cells[i]=UINT16_MAX;

    for (uint16_t r=0;r<rows;r++) {
        for (uint16_t c=0;c<cols;c++) {
            char token[16];
            tile_t tile;
            bool is_spawn;
            uint8_t spawn_id;

            if (fscanf(file,"%15s",token)!=1) {
                fclose(file);
                map_destroy(map);
                return -1;
            }

            if (parse_tile_token(token,&tile,&is_spawn,&spawn_id)!=0) {
                fclose(file);
                map_destroy(map);
                return -1;
            }

            map->tiles[make_cell_index(r,c,cols)]=tile;
            if (is_spawn) {
                map->spawn_cells[spawn_id]=make_cell_index(r,c,cols);
                if (spawn_id+1>map->spawn_count) map->spawn_count=(uint8_t)(spawn_id+1);
            }
        }
    }

    fclose(file);
    return 0;
}
