# Bomberman

Multiplayer Bomberman course project in C with a strict authoritative client-server architecture.

The server owns the game state, validates all actions, runs the fixed-tick simulation, and broadcasts results. The client sends input, receives authoritative updates, and renders the game. Current rendering includes the original `ncurses` debug client and a second `raylib` client implementation in `src/client_v2/`.

## Current Status

Implemented so far:

- shared protocol/config contract
- TCP client/server connection flow
- `HELLO`, `WELCOME`, `LEAVE`, `DISCONNECT`
- lobby state with ready-up flow
- live lobby ready-count sync for all clients
- lobby map/config selection
- status transitions: `GAME_LOBBY -> GAME_RUNNING -> GAME_END`
- fixed-tick server loop at `20` ticks per second
- authoritative movement handling
- bomb placement, timers, explosion start/end
- chain explosions
- full-map broadcast for authoritative tile sync
- death detection and `MSG_DEATH`
- winner detection and `MSG_WINNER`
- round reset by pressing ready again after game end
- separate authoritative bonus state on the server
- initial map bonuses loaded from map files
- bonus spawning from destroyed soft blocks
- bonus retrieval with server-side stat updates
- supported bonuses: `S` speed, `R` radius, `T` timer, `N` bomb-count
- shared bomb pool for multiple simultaneous bombs per player
- bonus sync to newly connected clients and on round restart
- `MSG_BLOCK_DESTROYED`
- late-join name sync for already connected clients
- selected map sync for lobby clients
- `MSG_READY_STATE` sync for lobby readiness display
- client bonus rendering
- client refactored into networking / protocol / UI / main modules
- `MSG_SYNC_REQUEST` debug resync path using existing authoritative messages
- separate `client_v2` raylib client with live map / player rendering and gameplay input

Current limitations:

- client is still a debug client, not the final graphical one
- client currently discards detailed bomb/explosion event payloads and relies on authoritative `MSG_MAP` updates for tile rendering
- bonus rendering is event-driven, not part of `MSG_MAP`
- gameplay around late join is still minimal and not fully specified beyond state sync
- no final lobby polish or end-screen polish
- map-file config values beyond the layout/bonus content are not fully used yet
- map selection currently uses repo-level protocol extensions because the base spec does not fully define the wire flow for it

## Project Structure

```text
src/
  shared/
    include/
      config.h
      protocol.h
      map_loader.h
      net_utils.h
    src/
      map_loader.c
      net_utils.c
  server/
    include/
    src/
  client/
    include/
      client.h
      client_net.h
      client_protocol.h
      client_ui.h
    src/
      main.c
      client_net.c
      client_protocol.c
      client_ui.c
  client_v2/
    include/
      client_v2_app.h
    src/
      main.c
      client_v2_app.c
  assets/
    maps/
```

## Build

```bash
make
```

Build outputs:

- `build/bin/server`
- `build/bin/client`
- `build/bin/client_v2` via `make client-v2`

Clean build artifacts:

```bash
make clean
```

Optional raylib client build:

```bash
make client-v2
```

## Run

Start the server:

```bash
./build/bin/server
```

or:

```bash
make run-server
```

Start one or more clients:

```bash
./build/bin/client
./build/bin/client Alice
./build/bin/client Bob
```

or:

```bash
make run-client
```

Optional raylib client scaffold:

```bash
./build/bin/client_v2
```

or:

```bash
make run-client-v2
```

Default connection target is `127.0.0.1:1727`.

## Client Controls

- arrow keys: move
- `space`: place bomb
- `r`: ready / restart next round
- `[` / `]`: previous / next map in lobby
- `s`: request a full resync from the server
- `q`: leave

## Current Gameplay Flow

1. Clients connect and send `HELLO`.
2. Server replies with `WELCOME`, the current map, current bonuses, and the selected map name.
3. In lobby, the first connected player can cycle available maps from `src/assets/maps/` with `[` and `]`.
4. All clients see the current lobby readiness as `ready/connected`.
5. Players press `r` in lobby.
6. When at least two connected players are ready, the server resets the round and switches to `GAME_RUNNING`.
7. Server processes movement, bombs, explosions, bonus collection, and deaths on the tick loop.
8. When one or zero alive players remain, the server switches to `GAME_END`.
9. Players press `r` again to start the next round.

## TODO

- deploy the server on a real IP address and port instead of the current local default `127.0.0.1:1727`

## Notes

- Wire directions use numeric enum values from `config.h`, not ASCII `U/D/L/R`.
- Game statuses used by this repo are:
  - `0 = GAME_LOBBY`
  - `1 = GAME_RUNNING`
  - `2 = GAME_END`
- Big-endian encoding is used for multibyte integer values on the wire.
- `BONUS_RETRIEVED` follows the text spec payload: `player_id + cell_index`. The client infers the removed bonus type from its current bonus layer.
- The server scans `src/assets/maps/` for `.txt` map files and sorts them alphabetically for lobby selection.
- `MSG_READY_STATE` is a repo-level message used to keep both client versions synchronized on lobby readiness counts.
