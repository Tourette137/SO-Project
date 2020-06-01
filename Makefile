CC = gcc
CFLAGS = -Wall -g
OBJDIR = obj
OBJS = $(OBJDIR)/auxs.o $(OBJDIR)/task.o
SERVER_OBJS = $(OBJDIR)/server.o
CLIENT_OBJS = $(OBJDIR)/client.o
DEPS = macros.h

all:
	make server
	make client

server: $(SERVER_OBJS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

client: $(CLIENT_OBJS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJDIR)/*.o
	rm server
	rm client
	rm server_pipe
