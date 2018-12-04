#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>  
#include <stdbool.h>
#include <sys/select.h>

#include "structs.h"

#define perror2(s,e) fprintf(stderr, "%s: %s\n", s, strerror(e))

#define max(x, y) (((x) > (y)) ? (x) : (y))

#define bufsize 512

struct queue *rlist = NULL;

int qsize = 0, totalbytes = 0, pagestotal=0;
DIR* pdir;
char *dirname;

bool activethread = false, running = true;

pthread_mutex_t taskqmut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t whosnext; 

void perror_exit(char *message);

void *threadf();


int main(int argc, char **argv)
{
	char buffer[bufsize];
	int threadnum=1, err, k;
	int comport, servport;

	if(argc != 9)
	{
  		printf("\nToo Few Arguments!\n");
		fprintf(stderr,"Usage: %s -p <serving_port> -c <command_port> -t <# of threads> -d <root_dir>\n", argv[0]);
		return -4;
    }
    else
    {

    	// servport = atoi(argv[2]);
    	// comport = atoi(argv[4]);
    	// threadnum = atoi(argv[6]);

        for (k=0;k<argc;k++)
        {
            if (strcmp(argv[k],"-p") == 0){
                servport = atoi(argv[k+1]);
            }
        }

        for (k=0;k<argc;k++)
        {
            if (strcmp(argv[k],"-c") == 0){
                comport = atoi(argv[k+1]);
            }
        }

        for (k=0;k<argc;k++)
        {
            if (strcmp(argv[k],"-t") == 0){
                threadnum = atoi(argv[k+1]);
            }
        }

        for (k=0;k<argc;k++)
        {
            if (strcmp(argv[k],"-d") == 0)
            {
                
                pdir = opendir(argv[k+1]);
            	if (pdir)
				{
				    closedir(pdir);
				    int templ = strlen(argv[k+1])+1;
				    dirname = (char*) malloc(sizeof(char)*(templ));
				    strcpy(dirname, argv[k+1]);
				    //printf("Dirname: \"%s\"\n", dirname);
				   
				}
				else if (ENOENT == errno)
				{
				    printf("\nERROR: Directory %s does not exist! \n", argv[k+1]);
				    return -56;
				}
           
				

            }
        }

    }

    //printf("Host: %s\n", host );
	printf("\nServing Port: %d\n", servport );
	printf("Command port: %d\n", comport );
	printf("# of threads: %d\n", threadnum );
	printf("Directory: %s\n", dirname );


	//socket creation
	int servsock, comsock; 
	//comport = atoi(argv[4]);
	//servport = atoi(argv[2]);//argv[2];

	//serving socket
	if ((servsock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		perror("Socket creation failed!");

	int enable=1;
  	setsockopt(servsock, SOL_SOCKET, SO_REUSEADDR, &enable , sizeof(int));

	struct sockaddr_in server, client;

    struct sockaddr *serverptr=(struct sockaddr *)&server;
    struct sockaddr *clientptr=(struct sockaddr *)&client;

	server.sin_family = AF_INET; 
	server.sin_addr.s_addr = htonl(INADDR_ANY); 
	server.sin_port = htons(servport);

	if (bind(servsock, serverptr, sizeof(server)) < 0)
        perror_exit("bind");

	int connlim = 10;

	if (listen(servsock, connlim) < 0) 
		perror_exit("listen");


	//command socket	
	if ((comsock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		perror_exit("Socket creation failed!");

	enable=1;
  	setsockopt(comsock, SOL_SOCKET, SO_REUSEADDR, &enable , sizeof(int));

	struct sockaddr_in comserver, comclient;
    struct sockaddr *comserverptr=(struct sockaddr *)&comserver;
    struct sockaddr *comclientptr=(struct sockaddr *)&comclient;	

    comserver.sin_family = AF_INET; 
	comserver.sin_addr.s_addr = htonl(INADDR_ANY); 
	comserver.sin_port = htons(comport);

	if (bind(comsock, comserverptr , sizeof(comserver)) < 0)
    {   
    	perror_exit("bind");
		exit(1);
	}

	connlim = 100;

	if (listen(comsock, connlim) < 0) 
		perror_exit("listen");


	//select preparation
    fd_set readSockSet;  
	int smax = max(servsock, comsock);

	//struct pollfd *sockfds = malloc(sizeof(pollfd)*2);

	pthread_cond_init(&whosnext, NULL);

	printf("\nMain thread: %ld\n", pthread_self() );
	//creating threads
	pthread_t threadp[threadnum];
 	void **status = NULL;
 	for(int i = 0; i < threadnum; i++)
 	{
	  	if (err = pthread_create(&threadp[i], NULL, threadf, NULL)) 
	  	{ /* New thread */

	      	perror2("pthread_create", err);
	      	exit(1); 
	    }
	    printf("Created thread %ld\n", threadp[i] );
	}	
	printf("\n");


	//accepting connections
	int newsock, tempsock ;
	socklen_t clientlen = 0, comclientlen = 0;
	struct hostent *rem;
	client.sin_family = AF_INET; 
	client.sin_addr.s_addr = inet_addr("127.0.0.1"); //IMPORTANT!! !! ---- WHY
	comclient.sin_family = AF_INET; 
	comclient.sin_addr.s_addr = inet_addr("127.0.0.1"); //IMPORTANT!! !! ---- WHY
	//client.sin_port = htons(servport);
	int flag = 1, bytesread;
	struct queue *tempnode, *newnode;
	int counter = 0, selectvar = 0;
	memset((void*)buffer, 0, bufsize);

	//server start time
	time_t startime = time(&startime);	


	while (running) 
	{
		FD_ZERO(&readSockSet);
   		FD_SET(servsock, &readSockSet);
    	FD_SET(comsock, &readSockSet);

    	printf("\nServer waiting for connections...\n");
    	selectvar = select(smax+1, &readSockSet, NULL, NULL, NULL);
	    if (selectvar > 0)
	    { 
	    	if(FD_ISSET(servsock, &readSockSet))
	    	{
		        /* accept connection */
		        
		    	if ((newsock = accept(servsock, clientptr, &clientlen)) < 0) perror("accept");
		    	/* Find client's address */
		    	if ((rem = gethostbyaddr((char *) &client.sin_addr.s_addr, sizeof(client.sin_addr.s_addr), client.sin_family)) == NULL) 
		    	{
		   	    	perror("line 154: gethostbyaddr"); 
		   	    	exit(1);
		   	    }
		    	printf("Serving Port(%d): Accepted connection from %s (socket fd: %d)\n", servport, rem->h_name, newsock);

		    	if (err = pthread_mutex_lock(&taskqmut)) 
		    	{ /* Lock mutex */
		        	perror2("pthread_mutex_lock", err); 	
		        	exit(1); 
				}
				//printf("Thread %ld(main): Locked the mutex\n", pthread_self());

				//printf("Active thread : %d\n", activethread);
				while (activethread)
		    	{
		    		pthread_cond_wait(&whosnext , &taskqmut);
		    		
		    	}
		    	//printf("Thread %ld(main): Locked the mutex\n", pthread_self());
		  		
		  		activethread = true;

		    	if (err = pthread_mutex_unlock(&taskqmut)) 
			    { /* Unlock mutex */
		        	perror2("pthread_mutex_unlock", err); 
		        	exit(1); 
		        }
		        //printf("Thread %ld(main): Unlocked the mutex\n", pthread_self());
		        if (!running)
		    	{	

		    		break;

		    	}

		    	tempnode = rlist;

		    	if (rlist == NULL)
		    	{
		    		newnode = malloc(sizeof(struct queue));
			    	newnode->fd = newsock;
			    	newnode->command = 0;
			    	newnode->next = NULL;
			    	qsize++;
			    	rlist = newnode;
			    	
		    	}
		    	else
		    	{

			    	// while(tempnode->next != NULL) //go to last node
			    	// {
			    	// 	tempnode = tempnode->next;
			    	// }

			    	//add task to queue
			    	newnode = malloc(sizeof(struct queue));
			    	newnode->fd = newsock;
			    	newnode->command = 0;
			    	newnode->next = NULL;
			    	qsize++;
			    	rlist = newnode;
			    	rlist->next = tempnode;
			    	//tempnode->next = newnode;
			    }

			    if (err = pthread_mutex_lock(&taskqmut)) 
			    { /* Lock mutex */
		        	perror2("pthread_mutex_lock", err); 
		        	exit(1); 
		        }
		        //printf("Thread %ld(main): Locked the mutex\n", pthread_self());

		        activethread = false;

		        if (qsize == 1)
		        	pthread_cond_signal(&whosnext);

			    if (err = pthread_mutex_unlock(&taskqmut)) 
			    { /* Unlock mutex */
		        	perror2("pthread_mutex_unlock", err); 
		        	exit(1); 
		        }
		        //printf("Thread %ld(main): Unlocked the mutex\n", pthread_self());


		    	// printf("Accepted connection\n");
		    	// switch (fork()) {    /* Create child for serving client */
		    	// case -1:     /* Error */
		    	//     perror("fork"); break;
		    	// case 0:	     /* Child process */
		    	//     close(sock); child_server(newsock);
		    	//     exit(0);
		    	// }

		    	// while ((strncmp(buffer, "telos", 5))!=0)
		    	// {
		    	// 	read(newsock, buffer, bufsize);
		   		// 	printf("%s\n", buffer); 
		   		// }	
		   	
		    		 /* parent closes socket to client            */
					/* must be closed before it gets re-assigned */  
		        //printf("Running var: %d\n", running );	
		    }

		  	if (FD_ISSET(comsock, &readSockSet))
		  	{
		  		/* accept connection */
		    	if ((newsock = accept(comsock, comclientptr , &comclientlen)) < 0) perror("accept");
		    	/* Find client's address */
		    	//printf("\nThis printf makes the world go round! %s \n", (char* ) &client.sin_addr.s_addr);
		    	if ((rem = gethostbyaddr((char *) &comclient.sin_addr.s_addr, sizeof(comclient.sin_addr.s_addr), comclient.sin_family)) == NULL) 
		    	{
		   	    	perror("line 444: gethostbyaddr"); 
		   	    	exit(1);
		   	    }
		    	printf("Command Port(%d): Accepted connection from %s (socket fd: %d)\n",comport, rem->h_name, newsock);


				bytesread = read(newsock, buffer, bufsize);

				if((strncmp(buffer, "SHUTDOWN", 8)==0) /* || ((strncmp(buffer, "/SHUTDOWN", 9))==0)*/)
				{
					printf("\n - - - Shutting down server - - -\n\n");
					write(newsock, "\n - - - Shutting down server - - -\n\n", strlen("\n - - - Shutting down server - - -\n\n"));
					running = false;
					pthread_cond_broadcast(&whosnext);
				}
				else if((strncmp(buffer, "STATS", 5)==0) /*|| ((strncmp(buffer, "/STATS", 6))==0)*/)
				{
					printf("\nSTATS:\n");					
					write(newsock, "\nSTATS:\n", strlen("\nSTATS:\n"));
					memset((void*)buffer, 0, bufsize);

					time_t stopped; 
					time(&stopped);
					int secsrunning = (stopped - startime);

					//int *timearray= NULL;
					//timearray = time_convertion(secsrunning);

					//time conversion
					int tarray[3];
					tarray[0]=0;
					tarray[1]=0;
					tarray[2]=0;
					tarray[0]=(secsrunning/3600);	// hours
					secsrunning -= (tarray[0]*3600);
					tarray[1]=(secsrunning/60);	// minutes
					secsrunning -= (tarray[1]*60);
					tarray[2]=secsrunning;

					
					printf( "Server up for %d:%d.%d, served %d pages, %d bytes\n\n", tarray[0], tarray[1], tarray[2], pagestotal, totalbytes);	
					sprintf(buffer, "Server up for %d:%d.%d, served %d pages, %d bytes\n\n", tarray[0], tarray[1], tarray[2], pagestotal, totalbytes);	
					write(newsock, buffer, strlen(buffer));		


				}
				else
				{
					printf("\nCommand not found!\n");
					// int tlen = strlen(buffer) + strlen() + 1;
					// char *tempbuffer = malloc(sizeof(char)*tlen);
					// sprintnf(tempbuffer, "<html><br>Command %s not found!<br></html>", buffer);
					// write(newsock, "<html><br>Command ", strlen("<html><br>Command "));	
					// write(newsock, buffer, strlen(buffer));	
					// write(newsock, " not found!<br></html>", strlen(" not found!<br></html>"));	
					write(newsock, "\nCommand not found!\n", strlen("\nCommand not found!\n"));	

					//Server up for 02:03.45, served 124 pages, 345539 bytes
				}	


				close(newsock);

		  	}

	    }
	    else if (selectvar < 0 )
	    {
	    	perror2( "select: " , err);
	    	exit(1);
	    }
    }

    // char *tempbuf = malloc(sizeof(char)*bufsize);
    // int bytesread;
    // struct queue *tt = rlist;
    // while (tt!=NULL)
    // {	printf("fdd %d\n",tt->fd );
    // 	bytesread=read(tt->fd, tempbuf, bufsize);
    // 	char stringtemp[bytesread+1];
    // 	strncpy(stringtemp, tempbuf, bytesread);
    // 	printf("Rlist : %s\n", stringtemp);
    // 	tt = tt->next;
    // }

  
    
    //printf("\nActiveThread: %d Running: %d\n", activethread, running);
    printf("Thread %ld waiting for %d threads to join\n", pthread_self(), threadnum);
	//joining - exiting threads
	for (int i = 0; i <threadnum; i++)
	{
		if (err = pthread_join(threadp[i], status)) 
		{ /* Wait for thread */
	      perror2("pthread_join", err); /* termination */
	      exit(1); 
	    }
  		else 
  		{	
  			printf("Thread no.%d %ld joined!\n", i, threadp[i]);
  			pthread_cond_signal(&whosnext);
  		}
  		//running = false;
  	}

  	//free
  	//free(buffer);
  	free(dirname);


	return 1;
}


void *threadf()
{

	//char *buffer = (char*) malloc(sizeof(char)*bufsize);
	char buffer[bufsize];
	

	int err, bytesread;

	struct queue *tempnode, *temp2node;

	while (running)
	{
		printf("I am the newly created thread %ld\n", pthread_self());


		if (err = pthread_mutex_lock(&taskqmut)) 
		{ /* Lock mutex */
	        perror2("pthread_mutex_lock", err); 
	    	exit(1); 	
		}

		while ((qsize < 1) || (activethread==true))
		{
			pthread_cond_wait( &whosnext , &taskqmut);
			if (!running) //check if it's the end
			{	
				printf("Thread %ld exiting...\n", pthread_self() );
				
				if (err = pthread_mutex_unlock(&taskqmut)) 
				{ /* Unlock mutex */
			        perror2("pthread_mutex_unlock", err); 
			    	exit(1); 
				}
	
				pthread_cond_signal(&whosnext);
				pthread_exit((void *)9);

			}

		}
		
		activethread = true;
		
		if (err = pthread_mutex_unlock(&taskqmut)) 
		{ /* Unlock mutex */
	        perror2("pthread_mutex_unlock", err); 
	    	exit(1); 
		}

		if (!running) //check if it's the end
		{				
			printf("Thread %ld exiting...\n", pthread_self() );
			//pthread_cond_signal(&whosnext);
			printf("%d\n", __LINE__);
			pthread_exit((void *)9);

		}
		
		tempnode = temp2node = rlist; 

		while ((tempnode->next)!=NULL) //list has at least one fd
	    {
	    	temp2node = tempnode;
	    	tempnode = tempnode->next;
	    }
			
		//printf("Fd = %d\n", tempnode->fd );
		memset((void*)buffer, 0, bufsize);
		int templen = strlen(buffer);
		//printf("Length = %d\n", templen );
		bytesread=read(tempnode->fd, buffer, bufsize);

		ssize_t linelen;
	 	size_t len = 0 ; 	//me len (buffersize) 0, i getline kanei realloc oso xreiazetai gia to input
	 	char *input = NULL, *temp2; 
	 	char delimiters[] = " \t\n\r";

		char* command = strtok_r(buffer,delimiters,&temp2);

		if (command!=NULL)
		{	
			//printf("Typed: %s\n", command );

			char *pagepath, *pagepathtemp;
			int length = 0;
			FILE *file;
			if((strncmp(command, "GET", 3))==0)
			{

				char *path = strtok_r(temp2,delimiters,&temp2);

				int lengthtemp = strlen(dirname) + strlen(path) + 1;
				pagepathtemp = (char*) malloc (sizeof(char)*lengthtemp);
				int length = strlen(dirname) + strlen(path) + 1 + 1 + 1;
				pagepath = (char*) malloc (sizeof(char)*length);
				strcpy(pagepathtemp, dirname);
				strcat(pagepathtemp, path);
				sprintf(pagepath, "./%s", pagepathtemp);
				
				int fileflag=0; //file exists and can be opened

				//printf("\nGET %s\n" , pagepath);

				if ((file = fopen(pagepath, "r")) == NULL)
	  			{  	perror("\nfopen source-file"); printf("\n"); 

	  				if (errno == ENOENT)
	  				{
	  					fileflag = 1;

	  				}
	  				else if (errno == EACCES)
	  				{
	  					fileflag = 2;

	  				}
	  				else
	  				{
	  					fileflag = 3;
	  									
	  				}

	  			}

	  			
	  			int countlines = 1;
	  			long filesz = 0;

	  			time_t      t;
				struct tm *ttm;
				time(&t);
				ttm = localtime(&t);		   
				char date[60];
				strftime(date, sizeof(date), "%c %Z", ttm);


	  			if( !fileflag ) // file exists
	  			{
	  				pagestotal++;
	  				printf("\nFetching page: %s ...\n\n", pagepath);

					while(fgetc(file) != EOF)
					{
						filesz++;
					}
					rewind(file);
					
					write(tempnode->fd, "HTTP/1.1 200 OK\n", strlen("HTTP/1.1 200 OK\n"));
					printf("\nHTTP/1.1 200 OK\n");
					
					int tlen = strlen("Date: \n") + sizeof(date) + 1 ;
					char datechar[tlen];
					sprintf(datechar, "Date: %s\n", date);
					write(tempnode->fd, datechar, strlen(datechar));
					printf("Date: %s\n", date);
					
					write(tempnode->fd, "Server: myhttpd/1.0.0\n", strlen("Server: myhttpd/1.0.0\n"));
					printf("Server: myhttpd/1.0.0\n");

					tlen = strlen("Content-Length: \n") + sizeof(filesz) + 1;
					char contentlengthchar[tlen];
					sprintf(contentlengthchar, "Content-Length: %jd\n", filesz);
					write(tempnode->fd, contentlengthchar, strlen(contentlengthchar));
					printf("Content-Length: %jd\n", filesz);
					
					write(tempnode->fd, "Content-Type: text/html\n", strlen("Content-Type: text/html\n"));
					printf("Content-Type: text/html\n");

					write(tempnode->fd, "Connection: Closed\n", strlen("Connection: Closed\n"));
					printf("Connection: Closed\n");

					write(tempnode->fd, "\n", strlen("\n"));
					
					
					int byteswritten;

				    while(!feof(file) ) //mexri na teleiwsei to arxeio
					{			
						linelen = getline(&temp2, &len, file);
						
						int temp2size = sizeof(temp2);
						int temp2len = strlen(temp2);
						byteswritten = write(tempnode->fd, temp2, temp2len);


						countlines++;
						totalbytes+=byteswritten;
					}

					free(temp2);

				}
				else if (fileflag == 1) //file does not exist
				{


					write(tempnode->fd, "HTTP/1.1 404 Not Found\n", strlen("HTTP/1.1 404 Not Found\n"));
					printf("HTTP/1.1 404 Not Found\n");
					
					int tlen = strlen("Date: \n") + sizeof(date) + 1 ;
					char datechar[tlen];
					sprintf(datechar, "Date: %s\n", date);
					write(tempnode->fd, datechar, strlen(datechar));
					printf("Date: %s\n", date);
					
					write(tempnode->fd, "Server: myhttpd/1.0.0\n", strlen("Server: myhttpd/1.0.0\n"));
					printf("Server: myhttpd/1.0.0\n");

					// tlen = strlen("Content-Length: \n") + sizeof(filesz) + 1;
					// char contentlengthchar[tlen];
					// sprintf(contentlengthchar, "Content-Length: %jd\n", filesz);
					write(tempnode->fd, "Content-Length: 124\n", strlen("Content-Length: 124\n"));
		
					write(tempnode->fd, "Content-Type: text/html\n", strlen("Content-Type: text/html\n"));
					
					write(tempnode->fd, "Connection: Closed\n", strlen("Connection: Closed\n"));
					
					write(tempnode->fd, "\n", strlen("\n"));
					
					write(tempnode->fd, "\n<html><br>Sorry dude, couldn't find this file.<br></html>\n", strlen("\n<html><br>Sorry dude, couldn't find this file.<br></html>\n"));

				} 
				else if (fileflag == 2) //not having the perimission to open the file
				{
					write(tempnode->fd, "HTTP/1.1 403 Forbidden\n", strlen("HTTP/1.1 403 Forbidden\n"));
					printf("HTTP/1.1 403 Forbidden\n");
					
					int tlen = strlen("Date: \n") + sizeof(date) + 1 ;
					char datechar[tlen];
					sprintf(datechar, "Date: %s\n", date);
					write(tempnode->fd, datechar, strlen(datechar));
					printf("Date: %s\n", date);
					
					write(tempnode->fd, "Server: myhttpd/1.0.0\n", strlen("Server: myhttpd/1.0.0\n"));
					printf("Server: myhttpd/1.0.0\n");

					// tlen = strlen("Content-Length: \n") + sizeof(filesz) + 1;
					// char contentlengthchar[tlen];
					// sprintf(contentlengthchar, "Content-Length: %jd\n", filesz);
					write(tempnode->fd, "Content-Length: 124\n", strlen("Content-Length: 124\n"));
		
					write(tempnode->fd, "Content-Type: text/html\n", strlen("Content-Type: text/html\n"));
					
					write(tempnode->fd, "Connection: Closed\n", strlen("Connection: Closed\n"));
					
					write(tempnode->fd, "\n", strlen("\n"));

					write(tempnode->fd, "<html><br>Trying to access this file but don't think I can make it.<br></html>", strlen("<html><br>Trying to access this file but don’t think I can make it.<br></html>"));

				}
				else
				{


					write(tempnode->fd, "Something weird went wrong with the file dude!!\n", strlen("Something weird went wrong with the file dude!!\n"));

					printf("Something weird went wrong with file: \"%s\" dude!!\n", pagepath);
				}


				free(pagepath);
			
				free(pagepathtemp);

				fclose(file);

			}
			else
			{
				write(tempnode->fd, "\nERROR\nUsage: GET /siteX/pageX_WYZ\nClosing connection..\n\n", strlen("\nERROR\nUsage: GET /siteX/pageX_WYZ\nClosing connection..\n\n"));


				printf("\nERROR\nUsage: GET /siteX/pageX_WYZ\nClosing connection..\n\n"); 

			}
			
			
			close(tempnode->fd);  //close socket
			//pop - delete last fd from queue
			temp2node->next = NULL;
			free(tempnode);
			if (temp2node == tempnode)
				rlist = NULL;
			qsize--;


			if (err = pthread_mutex_lock(&taskqmut)) 
			{ /* Lock mutex */
		        perror2("pthread_mutex_lock", err); 
		    	exit(1); 
			}
			//printf("Thread %ld(not main): Locked the mutex\n", pthread_self());

			activethread = false;

			pthread_cond_signal(&whosnext);

			if (err = pthread_mutex_unlock(&taskqmut)) 
			{ /* Unlock mutex */
		        perror2("pthread_mutex_unlock", err); 
		    	exit(1); 
			}
			//printf("Thread %ld(not main): Unlocked the mutex\n", pthread_self());
						
		}
		else
		{
			printf("\nNothing was typed!\n\n");
		}

		//free memory
		//free(temp2);
	}


	printf("Thread %ld exiting...\n", pthread_self() );

	//printf("145\n");	
	pthread_exit((void *)9);

	
}


void perror_exit(char *message)
{
	perror(message);
	exit(EXIT_FAILURE);
}

