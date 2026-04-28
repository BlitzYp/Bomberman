#!/usr/bin/env bash

set -eu

MODE="${1:-}"

if [ -z "$MODE" ]; then
    echo "Usage:"
    echo "  ./run_bomberman.sh server [port]"
    echo "  ./run_bomberman.sh client [name] [-l] [port]"
    echo "  ./run_bomberman.sh client-v2 [name] [-l] [port]"
    exit 1
fi

make

case "$MODE" in
    server)
        PORT="${2:-1727}"
        ./build/bin/server "$PORT"
        ;;
    client)
        NAME="${2:-Player}"
        if [ "${3:-}" = "-l" ]; then
            PORT="${4:-1727}"
            ./build/bin/client "$NAME" -l "$PORT"
        else
            PORT="${3:-1727}"
            ./build/bin/client "$NAME" "$PORT"
        fi
        ;;
    client-v2)
        NAME="${2:-Player}"
        make client-v2
        if [ "${3:-}" = "-l" ]; then
            PORT="${4:-1727}"
            ./build/bin/client_v2 "$NAME" -l "$PORT"
        else
            PORT="${3:-1727}"
            ./build/bin/client_v2 "$NAME" "$PORT"
        fi
        ;;
    *)
        echo "Unknown mode: $MODE"
        exit 1
        ;;
esac
