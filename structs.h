#ifndef STRUCTS_H_  
#define STRUCTS_H_


struct queue
{	
	int fd;
	int command;			//1- command, 0-request
	struct queue *next; 
};

struct head
{
	int size;
	struct queue *next;

};

struct links
{
	char *path;
	struct links *next;

};


#endif /* STRUCTS_H_ */