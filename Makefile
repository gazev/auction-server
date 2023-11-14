CC = gcc
LD = gcc
CFLAGS = -Wall -Wextra -Wunreachable-code -Wcast-align -Wconversion

INCLUDE_DIRS := utils 
INCLUDES := $(addprefix -I, $(INCLUDE_DIRS))

TARGET_EXECS := AS user 
SERVER_SOURCES := $(wildcard server/*.c)
SERVER_HEADERS := $(wildcard server/*.h)
SERVER_OBJECTS := $(SERVER_SOURCES:.c=.o)


CLIENT_SOURCES := $(wildcard client/*.c)
CLIENT_HEADERS := $(wildcard client/*.h)
CLIENT_OBJECTS := $(CLIENT_SOURCES:.c=.o)

UTILS_SOURCES := $(wildcard utils/*.c)
UTILS_OBJECTS := $(UTILS_SOURCES:.c=.o)


CFLAGS += $(INCLUDES)

all: $(TARGET_EXECS) 

server: AS

client: user

AS: $(UTILS_OBJECTS) $(SERVER_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

user: $(UTILS_OBJECTS) $(CLIENT_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	-rm $(TARGET_EXECS) $(SERVER_OBJECTS) $(CLIENT_OBJECTS) $(UTILS_OBJECTS) 2> /dev/null || true