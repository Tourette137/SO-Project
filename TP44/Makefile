CC = gcc
CFLAGS = -Wall -g -I$(INCDIR)
OBJDIR = obj
SRCDIR = src
INCDIR = includes
OBJS = $(OBJDIR)/auxs.o $(OBJDIR)/task.o
SERVER_OBJS = $(OBJDIR)/argusd.o $(OBJDIR)/server_child.o
CLIENT_OBJS = $(OBJDIR)/argus.o

all: argusd argus

argusd: $(SERVER_OBJS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

argus: $(CLIENT_OBJS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/argus.o: argus.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/argusd.o: argusd.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

zip:
	zip -r ../TP44.zip ../TP44

clean:
	rm -f $(OBJDIR)/*.o
	rm -f argusd
	rm -f argus
	rm -f server_client_pipe
	rm -f client_server_pipe
	rm -f error.txt
	rm -f log.txt
	rm -f log.idx
