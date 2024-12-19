# Compiler and flags
CC = gcc
CFLAGS = -g

# Source files and object files
SRCS = node.c initialization.c controller.c join_network.c utils.c communication.c signal_handler.c client_communication.c \
		hashtable.c hash.c queue.c
OBJS = $(SRCS:.c=.o)

# Main target
TARGET = node

# Build rules
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

# Pattern rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
