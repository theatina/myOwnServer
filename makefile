CC=gcc  
CFLAGS= -g -c -Wall -O0 -lpthread
OBJa = server.o 
OBJb = crawler.o

LIBS= -lm
OUTa  = myhttpd 
OUTb = mycrawler

all: $(OUTa) $(OUTb)

$(OUTa): $(OBJa)
	$(CC) -g $(OBJa) -o $@ $(LIBS) -lpthread 

$(OUTb): $(OBJb)
	$(CC) -g $(OBJb) -o $@ $(LIBS) -lpthread 

server.o: server.c
	$(CC) $(CFLAGS) server.c

crawler.o: crawler.c
	$(CC) $(CFLAGS) crawler.c

clean:
	rm -f $(OBJa) $(OBJb) $(OUTa) $(OUTa) 
