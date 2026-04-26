# Bomberman

Multiplayer Bomberman course project in C with a strict authoritative client-server architecture.

The server owns the game state, validates all actions, runs the fixed-tick simulation, and broadcasts results. The client sends input, receives authoritative updates, and renders the game. Current rendering is a simple `ncurses` debug client.

## Current Status

Implemented so far:

- shared protocol/config contract
- TCP client/server connection flow
- `HELLO`, `WELCOME`, `LEAVE`, `DISCONNECT`
- lobby state with ready-up flow
- status transitions: `GAME_LOBBY -> GAME_RUNNING -> GAME_END`
- fixed-tick server loop at `20` ticks per second
- authoritative movement handling
- bomb placement, timers, explosion start/end
- full-map broadcast for authoritative tile sync
- death detection and `MSG_DEATH`
- winner detection and `MSG_WINNER`
- round reset by pressing ready again after game end
- separate authoritative bonus state on the server
- initial map bonuses loaded from map files
- bonus spawning from destroyed soft blocks
- bonus retrieval with server-side stat updates
- bonus sync to newly connected clients and on round restart
- late-join name sync for already connected clients
- client bonus rendering
- client refactored into networking / protocol / UI / main modules

Current limitations:

- client is still a debug client, not the final graphical one
- client currently discards detailed bomb/explosion event payloads and relies on authoritative `MSG_MAP` updates for tile rendering
- bonus rendering is event-driven, not part of `MSG_MAP`
- gameplay around late join is still minimal and not fully specified beyond state sync
- no chain explosion / advanced bomb interaction polish yet
- no final lobby polish or end-screen polish

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

Clean build artifacts:

```bash
make clean
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

Default connection target is `127.0.0.1:1727`.

## Client Controls

- arrow keys: move
- `space`: place bomb
- `r`: ready / restart next round
- `q`: leave

## Current Gameplay Flow

1. Clients connect and send `HELLO`.
2. Server replies with `WELCOME`, the current map, and current bonuses.
3. Players press `r` in lobby.
4. When at least two connected players are ready, the server resets the round and switches to `GAME_RUNNING`.
5. Server processes movement, bombs, explosions, bonus collection, and deaths on the tick loop.
6. When one or zero alive players remain, the server switches to `GAME_END`.
7. Players press `r` again to start the next round.

## TODO

High priority:

- chain reactions when a bomb explosions triggers another bomb
- clean up remaining bomb / explosion edge cases and behavior
- decide and document the intended late-join behavior during `GAME_RUNNING`
- add stronger validation for player names and lobby edge cases

Medium priority:

- refactor large gameplay modules further as mechanics grow
- improve round reset and end-state UX
- add better disconnect/error handling paths

Later:

- replace the debug `ncurses` client with the planned graphical client
- add final art/audio pipeline decisions
- add gameplay polish and visual effects

## Notes

- Wire directions use numeric enum values from `config.h`, not ASCII `U/D/L/R`.
- Game statuses used by this repo are:
  - `0 = GAME_LOBBY`
  - `1 = GAME_RUNNING`
  - `2 = GAME_END`
- Big-endian encoding is used for multibyte integer values on the wire.
- This repo currently uses one extra protocol message, `MSG_PLAYER_JOINED`, to keep player names in sync for already connected clients.
