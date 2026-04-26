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

Current limitations:

- client is still a debug client, not the final graphical one
- client currently discards detailed bomb/explosion event payloads and relies on authoritative `MSG_MAP` updates for tile rendering
- no bonuses yet
- no soft-block destruction / bonus spawning protocol flow completed
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
    src/
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

## Current gameplay Flow

1. Clients connect and send `HELLO`.
2. Server replies with `WELCOME` and the current map.
3. Players press `r` in lobby.
4. When at least two connected players are ready, the server resets the round and switches to `GAME_RUNNING`.
5. Server processes movement and bombs on the tick loop.
6. When one or zero alive players remain, the server switches to `GAME_END`.
7. Players press `r` again to start the next round.

## TODO

High priority:

- finish map gameplay rules around soft-block destruction
- implement bonus spawning and retrieval
- decide whether bomb/explosion rendering should remain map-driven or move back to dedicated event handling on the client
- add stronger validation for player names and lobby edge cases

Medium priority:

- refactor large server modules further as gameplay grows
- improve round reset and end-state UX
- add better disconnect/error handling paths
- add protocol and gameplay tests

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
