CC ?= cc

BUILD_DIR := build
SERVER_BIN := $(BUILD_DIR)/bin/server
CLIENT_BIN := $(BUILD_DIR)/bin/client

CPPFLAGS := -Ishared/include -Iserver/include -Iclient/include
CFLAGS := -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter
LDFLAGS :=
LDLIBS := -pthread

SERVER_SRCS := \
	src/server/src/main.c \
	src/server/src/server.c \
	src/server/src/game_state.c \
	src/shared/src/net_utils.c

SERVER_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SERVER_SRCS))

CLIENT_SRCS := \
	src/client/src/main.c \
	src/shared/src/net_utils.c

CLIENT_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(CLIENT_SRCS))

.PHONY: all clean server client run-server run-client

all: server client

server: $(SERVER_BIN)

client: $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(CLIENT_BIN): $(CLIENT_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@ -lncurses

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN)

clean:
	if [ -d $(BUILD_DIR) ]; then find $(BUILD_DIR) -depth -delete; fi
