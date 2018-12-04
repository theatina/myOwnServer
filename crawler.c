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

//#define max(x, y) (((x) > (y)) ? (x) : (y))

#define bufsize 999

static const char rootdir[] = "sites";

bool workingonlist = false, downloading = true;

pthread_mutex_t downloadmut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t nexttogo; 

struct links *llist = NULL;

int port, running = 1,  pagetot = 0;//, first = 1, lsize = 0, firstflag = 1;
long bytestot = 0;

char *host, *dirname;

void perror_exit(char *message);

int remove_dir(char *path);

void *threadfun();


int main(int argc, char **argv)
{
	int k;

	printf("\n-----I am the CRAWLLLLEEEEEEER-----\n\n");

	char buffer[bufsize];
	char *initurl;
	int comport, threadnum, err;
	int templ;

	DIR *pdir;

	if(argc != 12)
	{
  		printf("\nToo Few Arguments!\n");
		fprintf(stderr,"\nUsage: %s -h <host_or_IP> -p <port> -c <command_port> -t <num_of_threads> -d <save_dir> <starting_URL>\n\n", argv[0]);
		return -4;
    }
    else
    {

    	for (k=0;k<argc;k++)
    	{
            if (strcmp(argv[k],"-h") == 0)
            {
                host = argv[k+1]; 
            }
        }

        for (k=0;k<argc;k++)
        {
            if (strcmp(argv[k],"-p") == 0){
                port = atoi(argv[k+1]);
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
                
               // filepos = k+1;
                //strncpy(root_dir, argv[k+1], strlen(argv[k+1])+1);
           
				templ = strlen(argv[k+1])+1;
			    dirname = (char*) malloc(sizeof(char)*(templ));
			    strcpy(dirname, argv[k+1]);

            }
        }

        initurl = argv[11];
    }


	int dirpathsz = strlen(dirname) + strlen("./") +1;
	char *dirpath = malloc(sizeof(char)*dirpathsz);
	sprintf(dirpath, "./%s", dirname);
	
	pdir=opendir(dirpath);

	if (pdir)
	{
	    closedir(pdir);

	    remove_dir(dirpath);

	    mkdir(dirpath, 0744);

	    //strcpy(dirname, argv[8]);
	}
	else if (ENOENT == errno)
	{
		mkdir(dirpath, 0744);
	}

	//---IN CASE OF SEGM---
	//host = argv[2];
	//port = atoi(argv[4]);
	//comport = atoi(argv[6]);
	//threadnum = atoi(argv[8]);
	
	char *initpath, *initpathstr;
	int doubledotscounter = 0, found = 0;
	initpathstr = initurl;
	//printf("initpathstr: %s\n", initpathstr );

	while(!found ) //extract initial path
	{

		if(!(strncmp(initpathstr, ":", sizeof(char)) ) )
		{
			doubledotscounter++;
		}

		if (doubledotscounter == 2)
		{
			while (!found)
			{

				if(!(strncmp(initpathstr, "/", sizeof(char)) ) )
				{
					found = 1;
				}
				else
				{
					initpathstr++;
				}
			}
		}

		if (!found)
			initpathstr++;

		

	}

	//printf("\ninitpathstr: %s\n", initpathstr);

	
	int pathlen = strlen(initpathstr) + 1;
	initpath = malloc(sizeof(char)*pathlen);
	strcpy(initpath, initpathstr);

	printf("Host: %s\n", host );
	printf("Port: %d\n", port );
	printf("Command port: %d\n", comport );
	printf("# of threads: %d\n", threadnum );
	printf("Directory: %s\n", dirname );
	int lenurl = strlen(initpath)+1;
	printf("Initial path: %s\n", initpath );


	//create link queue - linked list
	llist = malloc(sizeof(struct links));
	struct links *tempnode, *newnode;
	//int lenurl = strlen(initurl)+1;
	char *pathnode = (char*) malloc(sizeof(char)*lenurl); 
	(llist->path) = pathnode;
	strcpy(llist->path, initpath);
	llist->next = NULL;
	//printf("llist init path: %s\n", llist->path);


	printf("\n");
	char threadnumstr[sizeof(int)];
	sprintf(threadnumstr, "%d", threadnum);
	char portstr[sizeof(int)];
	sprintf(portstr, "%d", port);
	//char comportstr[sizeof(int)];
	//sprintf(comportstr, "%d", sizeof(int));



	//command socket
	int  comsock, enable, connlim; 
	//int comport = atoi(argv[4]);
	//printf("\ncomport: %d\n", comport );

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

	int newsock, tempsock ;
	socklen_t comclientlen = 0;
	struct hostent *rem;
	comclient.sin_family = AF_INET; 
	comclient.sin_addr.s_addr = inet_addr("127.0.0.1"); //IMPORTANT!! !! ---- WHY
	//client.sin_port = htons(servport);
	long bytesread;

	memset((void*)buffer, 0, bufsize);

	//crawler start time
	time_t startime = time(&startime);	


	//condition variable
	pthread_cond_init(&nexttogo, NULL);

 	//create the threads
	printf("\nMain thread: %ld\n", pthread_self() );
	
	pthread_t threadp[threadnum];
 	void **status = NULL;
 	for(int i = 0; i < threadnum; i++)
 	{
	  	if (err = pthread_create(&threadp[i], NULL, threadfun, NULL)) 
	  	{ /* New thread */

	      	perror2("pthread_create", err);
	      	exit(1); 
	    }
	    printf("Created thread %ld\n", threadp[i] );
	}	
	printf("\n");



	int stop = 0;
	while(!stop)
	{
		//accept connection 
		if ((newsock = accept(comsock, comclientptr , &comclientlen)) < 0) perror("accept");
		// Find client's address
		//printf("\nThis printf makes the world go round! %s \n", (char* ) &client.sin_addr.s_addr);
		if ((rem = gethostbyaddr((char *) &comclient.sin_addr.s_addr, sizeof(comclient.sin_addr.s_addr), comclient.sin_family)) == NULL) 
		{
	    	perror("line 308: gethostbyaddr"); 
	    	exit(1);
		}
		printf("\nCommand Port(%d): Accepted connection from %s (socket fd: %d)\n",comport, rem->h_name, newsock);


		bytesread = read(newsock, buffer, bufsize);

		if((strncmp(buffer, "SHUTDOWN", 8)==0) /* || ((strncmp(buffer, "/SHUTDOWN", 9))==0)*/)
		{
			printf("\n - - - CRAWLLLLEEEEEEER OUT.... - - -\n\n");
			write(newsock, "\n - - - CRAWLLLLEEEEEEER OUT.... - - -\n\n", strlen("\n - - - CRAWLLLLEEEEEEER OUT.... - - -\n\n"));
			downloading = false;
			stop = 1;
			pthread_cond_broadcast(&nexttogo);
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

			// for (int i = 0 ; i<3; i++)
			// 	printf("\ntime %d: %d\n",i,  tarray[i]);

			printf("Crawler up for %d:%d.%d, downloaded %d pages, %d bytes\n\n", tarray[0], tarray[1], tarray[2], pagetot, bytestot);	
			sprintf(buffer, "Crawler up for %d:%d.%d, downloaded %d pages, %d bytes\n\n", tarray[0], tarray[1], tarray[2], pagetot, bytestot);	
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
  		}
  		
  	}




  	//free memory
  	free(dirpath);
  	free(dirname);
  	free(initpath);
  	//free(pathnode);


	return 1;

	

}


void *threadfun()
{
	int err;
	char buffer[bufsize];
	printf("\nI am the newly created thread %ld\n", pthread_self());

	while (downloading)
	{
		if (err = pthread_mutex_lock(&downloadmut)) 
		{ /* Lock mutex */
	        perror2("pthread_mutex_lock", err); 
	    	exit(1); 	
		}
		//printf("Thread %ld(not main): Locked the mutex\n", pthread_self());

	
		while ((workingonlist==true))
		{
			//printf("active threads: %d-> Thread %ld(not main) waits for whosnext\n", workingonlist, pthread_self() );
			pthread_cond_wait( &nexttogo , &downloadmut);
			//printf("line 67\n");
			if (!downloading) //check if the files are downloaded
			{	
				//printf("line 74\n");
				printf("Thread %ld exiting...\n", pthread_self() );
				
				if (err = pthread_mutex_unlock(&downloadmut)) 
				{ /* Unlock mutex */
			        perror2("pthread_mutex_unlock", err); 
			    	exit(1); 
				}
				//printf("Thread %ld(not main): Unlocked the mutex\n", pthread_self());

				pthread_cond_signal(&nexttogo);
				pthread_exit((void *)9);

			}

		}
		

		//first = 0;

		//printf("Thread %ld: Locked the mutex\n", pthread_self());

		
		workingonlist = true;
		
		if (err = pthread_mutex_unlock(&downloadmut)) 
		{ /* Unlock mutex */
	        perror2("pthread_mutex_unlock", err); 
	    	exit(1); 
		}
		//printf("Thread %ld: Unlocked the mutex\n", pthread_self());

		if (!downloading) //check if it's the end
		{				
			printf("Thread %ld exiting...\n", pthread_self() );
			//pthread_cond_signal(&whosnext);
			printf("%d\n", __LINE__);
			pthread_exit((void *)9);

		}

		//critical section ------------------------------------

		
		memset((void*)buffer, 0, bufsize);
		
		int err, bytesread = 0, sock, i;

		struct sockaddr_in server;
	    struct sockaddr *serverptr = (struct sockaddr*)&server;
	    struct hostent *rem;


	    if (llist == NULL)
	    {
	    	downloading = false;
			//printf("line %d\n", __LINE__);
			printf("Thread %ld exiting...\n", pthread_self() );
			
			if (err = pthread_mutex_unlock(&downloadmut)) 
			{ /* Unlock mutex */
		        perror2("pthread_mutex_unlock", err); 
		    	exit(1); 
			}
			//printf("Thread %ld(not main): Unlocked the mutex\n", pthread_self());

			pthread_cond_signal(&nexttogo);
			pthread_exit((void *)9);		

	    }


		struct links *tempnode, *temp2node;

		tempnode = temp2node = llist; 

		while ((tempnode->next)!=NULL) //list has at least one path
	    {
	    	//printf("Path = %s\n", tempnode->path );
	    	temp2node = tempnode;
	    	tempnode = tempnode->next;
	    }
		
	   	int listpath = strlen(tempnode->path) + 1;
	   	char *pathfromlist = malloc(sizeof(char)*listpath);
	   	strcpy(pathfromlist, tempnode->path);

		//printf("Path = %s\n", pathfromlist );

		//delete node - free memory
		temp2node->next = NULL;
		free(tempnode->path);
		free(tempnode);
		if (temp2node == tempnode)
		{	

			llist = NULL;
		}


	    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	    	perror_exit("socket");

	    //printf("\nSocket to write: %d\n", sock);

		/* Find server address */
	    if ((rem = gethostbyname(host)) == NULL)
	    {	
		   perror("gethostbyname"); /*exit(1)*/;
	    }
	    //port = port; /*Convert port number to integer*/
	    server.sin_family = AF_INET;       /* Internet domain */
	    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
	    server.sin_port = htons(port);         /* Server port */
	    /* Initiate connection */
	    if (connect(sock, serverptr, sizeof(server)) < 0)
		   perror_exit("connect");

		//tempnode = llist;
		strcpy(buffer, pathfromlist);

		memset((void*)buffer, 0, bufsize);
		sprintf(buffer, "GET %s\n", pathfromlist);
		int lentoprint = strlen(buffer) + 1;
		printf("GET %s\n", pathfromlist );

		write(sock, buffer, lentoprint);

		
		
		int blen = 0;
		memset((void*)buffer, 0, bufsize);
		char *newbuf;
		int counter = 0, btoread = strlen("HTTP/1.1 200");
		newbuf = buffer;
		while (counter < btoread)
		{	
			bytesread = read(sock, newbuf, sizeof(char));
			newbuf++;
			counter++;
		}
		strcpy(newbuf, "\0");

		//printf("\nBuffer %s\n", buffer);

		int responsenumint = 0;
	 	char *input = NULL, *responsenum, *temp2; 
	 	char delimiters[] = " \t\n\r";

	 	input = buffer;
		strtok_r(input,delimiters,&temp2);
		responsenum = strtok_r(temp2,delimiters,&temp2);
		responsenumint = atoi(responsenum);	
		//printf("responsenum: %d\n", responsenumint);

		int newlinecounter = 0, newlineflag = 0;
		char *charac = (char*)malloc(sizeof(char)*2);

		//ifs for response
		if (responsenumint == 200) //file found
		{
			printf("Page found!\n");
			pagetot++;

			int writetofile = 0, clength = 0;
			char *clengthstr;
			int lengthflag = 0;
			while (!writetofile)
			{
				memset((void*)charac, 0, 2*sizeof(char));
				bytesread = read(sock, charac, sizeof(char));
				
				if (newlineflag) //if previous character is newline
				{	
					if (!(strncmp(charac ,"\n", sizeof(char)))) //if i have two, then start writing the page to new file
					{	
						writetofile = 1;
						newlinecounter++;
						newlineflag = 0;
					}
					else
					{
						newlineflag = 0;
					}
				}
				else  //if the previous character wasn't a newline
				{
					if (!(strncmp(charac , "\n", sizeof(char)))) //if current char is '\n'
					{	
						newlineflag = 1;
						newlinecounter++;

					}
				}

				//take content length 
				if (!lengthflag && (newlinecounter == 3)) //now we are at content-length 
				{	
					lengthflag = 1;
					memset((void*)buffer, 0, bufsize);
					btoread = strlen("Content-Length: ") + sizeof(long) ;
					newbuf = buffer;
					counter = 0;
					while (counter < btoread)
					{
						memset((void*)charac, 0, 2*sizeof(char));
						bytesread = read(sock, charac, sizeof(char));
						strcpy(newbuf, charac);

						newbuf++;
						counter++;
					}

					strncpy(newbuf, "\0", sizeof(char));

					input = buffer;
					strtok_r(input,delimiters,&temp2);
					clengthstr = strtok_r(temp2,delimiters,&temp2);
					clength = atoi(clengthstr);	
					printf("Content-Length: %d\n", clength);

				}

			}

			bytestot+=clength;

			//read content- length chars from socket to write to new file
			printf("Copying file %s\n\n", pathfromlist);
			
			char *pagename ;		
			pagename = pathfromlist;
			while((strncmp(pagename, "p", sizeof(char)) ) )
			{
				pagename++;
			}

			int namesize = strlen(pagename) + 1;
			char lfile[namesize];
			sprintf(lfile, "%s", pagename);
			int pathsize = strlen("./") + strlen(dirname) + strlen(pathfromlist) + 1;
			char lpath[pathsize];
			sprintf(lpath, "./%s%s", dirname, pathfromlist);

			char *temp2 ;

	 		char delimiters[] = " /";

	 		strcpy(input, lpath);

	 		input++; 

	 		char *token=strtok_r(input,delimiters,&temp2);
	 		input = temp2;
	 		int tokenlen = strlen(token);

	 		//analyze path to extract subdirs to create
	 		while ((token!=NULL) && (strncmp(token, lfile, strlen(token))))
	 		{
	 			tokenlen = strlen(token);

	 			//create subdirs
	 			if(tokenlen > 1)
	 			{

	 				if (strncmp(token, dirname, strlen(token) ))
	 				{
		 				DIR *pdir;
		 				int dirpathsz = strlen(token) + strlen(".///") + strlen(dirname) + 1;
						char *dirpath = malloc(sizeof(char)*dirpathsz);
						sprintf(dirpath, "./%s/%s/", dirname, token);
						
						pdir=opendir(dirpath);

						if (pdir) 
						{
						    closedir(pdir);
						    
						}
						else if (ENOENT == errno) //if dir does not exist, create it
						{
							mkdir(dirpath, 0744);
						}

						free(dirpath);
					}

	 			}

				token=strtok_r(input,delimiters,&temp2);
				input = temp2;

			}

			FILE *newfile = fopen(lpath, "w+");  //create and copy to new file

			// if(newfile == NULL)
			// {
			// 	perror("fopen");
			// }  

			counter = 0;
			while (counter < clength) //copy to new file
			{
				memset((void*)charac, 0, 2*sizeof(char));
				bytesread = read(sock, charac, sizeof(char));
				fprintf(newfile, "%s", charac);

				counter++;
			}

			fclose(newfile);

			close(sock);

			//find all the links in page and save them
			ssize_t linelen;	
			size_t len = 0 ;
			char *docline = NULL;

			//extract all the paths from page
			FILE *doc =fopen(lpath, "r");
			// if(doc == NULL)
			// {
			// 	perror("fopen");
			// } 

			char *linkpos, *bufstr;
			char tempc;
			while (!feof(doc))
			{

				linelen = getline(&docline, &len, doc);
				memset((void*)buffer, 0, bufsize);

				linkpos = strstr(docline, "href");
				if (linkpos!=NULL)
				{
					int movepos = strlen("href='..");
					linkpos +=movepos;
					bufstr = buffer;
					
					while ((strncmp(linkpos, "\"", sizeof(char)) ) )
					{
						
						strncpy(bufstr, linkpos, sizeof(char));

						linkpos++;
						bufstr++;
					}
					strncpy(bufstr, "\0", sizeof(char));


					int docpathsz = strlen(buffer) + 1;
					char *docpath = malloc(sizeof(char)*docpathsz);
					char *docpathstr = docpath;

					int docpathsavesz = strlen(buffer) + strlen(dirname) + 2 + 1;
					char *docpathsave = malloc(sizeof(char)*docpathsavesz);
					char *docpathsavestr = docpathsave;

					int docpathrootsz = strlen(buffer) + strlen(rootdir) + 2 + 1;
					char *docpathroot = malloc(sizeof(char)*docpathrootsz);
					char *docpathrootstr = docpathroot;

					strncpy(docpath, buffer, docpathsz);
					sprintf(docpathsave, "./%s%s", dirname, docpath );
					sprintf(docpathroot, "./%s%s", rootdir, docpath );
					
					FILE *filestatus;

					int fileflag=0; //file exists and can be opened

					if ((filestatus = fopen(docpathsave, "r")) == NULL)
		  			{  	
		  				//perror("\nfopen source-file"); printf("\n"); 

		  				if (errno == ENOENT)  //file does not exist 
		  				{
		  					fileflag = 1;

		  				}
		  				else if (errno == EACCES) //not having the perimission to open the file
		  				{
		  					fileflag = 2;

		  				}
		  				else 
		  				{
		  					fileflag = 3;
		  									
		  				}

		  			}
		  			else
		  			{
		  				fclose(filestatus);
		  			}

					if(fileflag == 1) //file does not exist 
					{
						//check if it's on list
						int pathfound = 0;

						struct links *tttempnode;

						tttempnode = llist; 

						int found = 0;
						while (tttempnode!=NULL) //if list has at least one path
					    {
					    	if (!(strncmp(tttempnode->path, docpath, strlen(docpath))))
					    	{	
					    		found = 1;
					    	}
					    	tttempnode = tttempnode->next;
					    }
						
						if (!found) //if there is not already on the paths list, place it 
						{
							struct links *tnode = llist, *newnode;
						
					    	if (llist == NULL)
					    	{	
					    		newnode = malloc(sizeof(struct links));
						    	newnode->path = docpath;
						    	newnode->next = NULL;
						    	llist = newnode;
						    	
					    	}
					    	else
					    	{
						    	
						    	//add task to queue
						    	newnode = malloc(sizeof(struct queue));
						    	newnode->path = docpath;
						    	newnode->next = NULL;
						    	llist = newnode;
					    		llist->next = tnode;
						    
						    }
						}
						else
						{
							free(docpath);

						}
					
					}
					else
					{
						free(docpath);
					}


					free(docpathsavestr);
					free(docpathrootstr);

				}

			}

			free(docline);
		
			fclose(doc);
		}
		else if(responsenumint == 404) //file not found
		{

			printf("\nFile \"%s\" not found =( !\n", pathfromlist);

		}
		else if (responsenumint == 403) //not accessible file
		{

			printf("\nFile \"%s\" not accessible =( !\n", pathfromlist);

		}

		//---------------critical section

		if (err = pthread_mutex_lock(&downloadmut)) 
		{ /* Lock mutex */
	        perror2("pthread_mutex_lock", err); 
	    	exit(1); 
		}
		//printf("Thread %ld(not main): Locked the mutex\n", pthread_self());

		workingonlist = false;

		pthread_cond_signal(&nexttogo);

		if (err = pthread_mutex_unlock(&downloadmut)) 
		{ /* Unlock mutex */
	        perror2("pthread_mutex_unlock", err); 
	    	exit(1); 
		}
		//printf("Thread %ld(not main): Unlocked the mutex\n", pthread_self());
	

		//downloading = 0;  //just for the testing phase

		free(charac);
		free(pathfromlist);
	}

	printf("Thread %ld exiting...\n", pthread_self() );
	//pthread_cond_signal(&whosnext);	

	//free memory
	//free(buffer);
	

	pthread_exit((void *)9);

}

void perror_exit(char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

int remove_dir(char *path)
{
   DIR *d = opendir(path);
   size_t path_len = strlen(path);
   int r = -1;

   if (d)
   {
      struct dirent *p;

      r = 0;

      while (!r && (p=readdir(d)))
      {
          int r2 = -1;
          char *buf;
          size_t len;

          /* Skip the names "." and ".." as we don't want to recurse on them. */
          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
          {
             continue;
          }

          len = path_len + strlen(p->d_name) + 2; 
          buf = malloc(len);

          if (buf)
          {
             struct stat statbuf;

             snprintf(buf, len, "%s/%s", path, p->d_name);

             if (!stat(buf, &statbuf))
             {
                if (S_ISDIR(statbuf.st_mode))
                {
                   r2 = remove_dir(buf);
                }
                else
                {
                   r2 = unlink(buf);
                }
             }

             free(buf);
          }

          r = r2;
      }

      closedir(d);
   }

   if (!r)
   {
      r = rmdir(path);
   }

   return r;
}





