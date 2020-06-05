CC = gcc
CFLAGS = -Wall -g -I$(INCDIR)
OBJDIR = obj
SRCDIR = src
INCDIR = includes
OBJS = $(OBJDIR)/auxs.o $(OBJDIR)/task.o
SERVER_OBJS = $(OBJDIR)/server.o $(OBJDIR)/server_child.o
CLIENT_OBJS = $(OBJDIR)/client.o

all:
	make server
	make client

server: $(SERVER_OBJS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

client: $(CLIENT_OBJS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJDIR)/*.o
	rm server
	rm client
	rm server_client_pipe
	rm client_server_pipe
	rm error.txt
