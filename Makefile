
CC=gcc
OBJS=comm.o
CFLAGS = -O3
LDFLAGS = -lpthread

all: comm

comm: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f comm comm.o *~
