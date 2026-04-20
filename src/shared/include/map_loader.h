#ifndef MAP_LOADER_H
#define MAP_LOADER_H

#include "config.h"

int map_load_from_file(const char* path, map_t* map);
void map_destroy(map_t* map);
tile_t map_get_tile(const map_t* map, uint16_t row, uint16_t col);
bool map_is_walkable(const map_t* map, uint16_t row, uint16_t col);

#endif
