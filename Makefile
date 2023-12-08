CC = gcc
LD = gcc
CFLAGS = -Wall -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment#-Wextra -Wunreachable-code 
#-Wcast-align -Wconversion

TARGET_EXECS := AS user 

INCLUDE_DIRS := utils 
INCLUDES := $(addprefix -I, $(INCLUDE_DIRS))

SERVER_SOURCES := $(wildcard server/*.c)
SERVER_HEADERS := $(wildcard server/*.h)
SERVER_OBJECTS := $(SERVER_SOURCES:.c=.o)

CLIENT_SOURCES := $(wildcard client/*.c)
CLIENT_HEADERS := $(wildcard client/*.h)
CLIENT_OBJECTS := $(CLIENT_SOURCES:.c=.o)

UTILS_SOURCES := $(wildcard utils/*.c)
UTILS_OBJECTS := $(UTILS_SOURCES:.c=.o)

CFLAGS += $(INCLUDES)

# run with DEBUG=no to deactivate
ifneq ($(strip $(DEBUG)), no)
  CFLAGS += -g
endif

.PHONY: all server client clean

all: $(TARGET_EXECS) 

server: AS

client: user

AS: $(UTILS_OBJECTS) $(SERVER_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

user: $(UTILS_OBJECTS) $(CLIENT_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	-rm -rf ASDIR $(TARGET_EXECS) $(SERVER_OBJECTS) $(CLIENT_OBJECTS) $(UTILS_OBJECTS) 2> /dev/null || true
