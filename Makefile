CC ?= cc

BUILD_DIR := build
SERVER_BIN := $(BUILD_DIR)/bin/server
CLIENT_BIN := $(BUILD_DIR)/bin/client
CLIENT_V2_BIN := $(BUILD_DIR)/bin/client_v2

CPPFLAGS := -Isrc/shared/include -Isrc/server/include -Isrc/client/include -Isrc/client_v2/include
CFLAGS := -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter
LDFLAGS :=
LDLIBS := -pthread
RAYLIB_CFLAGS ?=
RAYLIB_LIBS ?= -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

SERVER_SRCS := \
	src/server/src/main.c \
	src/server/src/server.c \
	src/server/src/bombs.c \
	src/server/src/lobby.c \
	src/server/src/server_messages.c \
	src/server/src/game_state.c \
	src/server/src/player_actions.c \
	src/server/src/bonuses.c \
	src/shared/src/map_loader.c \
	src/shared/src/net_utils.c

SERVER_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SERVER_SRCS))

CLIENT_SRCS := \
	src/client/src/client_net.c \
	src/client/src/client_protocol.c \
	src/client/src/client_ui.c \
	src/client/src/main.c \
	src/shared/src/map_loader.c \
	src/shared/src/net_utils.c

CLIENT_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(CLIENT_SRCS))

CLIENT_V2_SRCS := \
	src/client_v2/src/client_v2_app.c \
	src/client_v2/src/main.c \
	src/client/src/client_net.c \
	src/shared/src/net_utils.c

CLIENT_V2_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(CLIENT_V2_SRCS))

.PHONY: all clean server client client-v2 run-server run-client run-client-v2

all: server client

server: $(SERVER_BIN)

client: $(CLIENT_BIN)

client-v2: $(CLIENT_V2_BIN)

$(SERVER_BIN): $(SERVER_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(CLIENT_BIN): $(CLIENT_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@ -lncurses

$(CLIENT_V2_BIN): $(CLIENT_V2_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) $(RAYLIB_LIBS) -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(RAYLIB_CFLAGS) $(CFLAGS) -c $< -o $@

run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN)

run-client-v2: $(CLIENT_V2_BIN)
	./$(CLIENT_V2_BIN)

clean:
	if [ -d $(BUILD_DIR) ]; then find $(BUILD_DIR) -depth -delete; fi
