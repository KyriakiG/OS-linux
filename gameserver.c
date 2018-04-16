#include "mutual.h"

/* Size of the request queue. */
#define LISTENQ 20

/**************************************************************************/
/* GLOBAL VARIABLES */

/* semaphore to sychronize chat */
sem_t *sem;

/* global variables helpful for chat with threads and alarm function */
int connfd;
char line[100];

/* global shared memory id helpful for chat with threads and necessary
 * to close shared memory at Ctrl-C signal handler
 */
int shmid=0;

int qshmid=0;

/* END GLOBAL VARIABLES */
/**************************************************************************/


/**************************************************************************/
/* SIGNAL HANDLERS DECLARATION - DEFINITION */

/* set alarm signal handler, so can send waiting message to client every 5 sec */
void alarmclient(int sig)
{
	(void) sig; // just to avoid warning...
	int *qshm=NULL;
	int ret;

	// send data of 'line' to client through connfd
	ret = write(connfd, line, sizeof(char[100]));
	if(ret<=0)
	{
		printf("wrong while sending\n");

		/* attach the segment to data space */
		if ((qshm = (int *) shmat(qshmid, NULL, 0)) == (int *) -1)
		{
			/* error message*/
			perror("shmat");
			exit(1);
		}

		(*qshm)++;

		printf("QSHM : %d\n", *qshm);

	}
	else { printf("all ok sending...\n"); }

	alarm(5); // call alarm(5) again
	/* function terminating when alarm(0) is called */
}

/* The use of this functions avoids the generation of "zombie" processes. */
void sig_chld(int signo)
{
	(void) signo; // just to avoid warning...

	pid_t pid;
	int stat;

	/* waiting for children to die */
	while ( ( pid = waitpid( -1, &stat, WNOHANG ) ) > 0 )
	{
		/* dying message... */
		printf( "Child %d terminated.\n", pid );
	}
}

/* set exit signal handler, so print exiting message and close shared memory that is still open */
void catch_int(int signo)
{
	(void) signo; // just to avoid warning...

	/* close shared memory */
	if (shmctl(shmid, IPC_RMID, (struct shmid_ds *) NULL) == 0)
	{
		printf("Shared memory deleted, all ok!!!\n");
	}

	/* close quit shared memory */
	if (shmctl(qshmid, IPC_RMID, (struct shmid_ds *) NULL) != 0)
	{
		/* error message */
		printf("Error deleting quit shared memory\n");
	}

	/* exiting message */
	printf("\n\t\t Gameserver Closed. Everything ok...\n\n");	

	/* main server exiting */
	exit(0);
}
/* END SIGNAL HANDLERS DECLARATION - DEFINITION */
/**************************************************************************/


/**************************************************************************/
/* METHODS' DECLARATION */

/* initialize server's parameters so the game server can begin */
int setparameters(char **ch, int *p, int *q, inventory *in);

/* initialize parent-server's pipe-flag so it will be able to know when to generate a new server */
int initpipeflag(int fd[2], char data[5]);

/* start running main server! */
void runmainserver(int *players, int *quota, inventory *inv , int fd[2], char buf[5], char data[5]);

/* start running room server! */
void runroomserver(int *players, int *quota, inventory *inv , int fd[2], char data[5], key_t key, int listenfd, socklen_t clilen, struct sockaddr_in cliaddr);

/* initialize room-server's pipe-flag so it will be able to know when to create and delete the chat room */
void initpipe(int fd[2], char data[]);

/* initialize socket's characteristics so set able the connection with the client */
void initializesocket(int *listenfd, struct sockaddr_in *servaddr);

/* turn string into player's data */
void stringtodata(char str[], char *pname, inventory *pinv);

/* check if player's inventory is conventional to participate */
int checkinventory(inventory *sinv, inventory cinv, int sumq);

/* open shared memory and put inventory inside */
int setshm(char *shm, key_t key , inventory inv);

/* get inventory struct into shared memory */
void putinvtoshm(char *shm, inventory inv);

/* get data from shared memory and put them in the field 'quantity' of the given inventory */
void informinv(char *shm, inventory *inv);

/* send messages to accepted player while waiting for room to fill */
void waitingmode(int connfd, char line[100], int fd[2], char buf[2]);

/* open reading and writing thread */
void openchat(void *nam);

/* lock playerserver in reading from client mode until player detaches */
void *ifreadplayer(void *nam);

/* lock playerserver in writing to client mode until ifreadplayer returns == player detaches */
void *ifwriteplayer();

/* called by roomserver, so can synchronize chat */
void messagehandle(char *shm, int PLAYERS);

/* END METHODS' DECLARATION */
/**************************************************************************/

/**************************************************************************/
/* MAIN */
int main(int argc, char **argv)
{
	/* ------------------- DATA DECLARARION --------------------------------- */
	/* arg-data, game's data */
	int players=0, quota=0;
	inventory inv;
	/* pipe's data */
	int fd[2];
	char buf[5];
	char data[5] = "open"; // gets two values: "open" -> open a room, "wait" -> do nothing
	/* ---------------------------------------------------------------------- */

	/* ------------------- GET GAMESERVER READY TO BEGIN -------------------- */
	/* check for correct number of input args */
	if (argc != 7) { exit(1); } // terminating process if invalid number of args

	/* read arguments and check for correct type and right value of them */
	if (setparameters(argv, &players, &quota, &inv) != 0) { exit(1); } // terminating process if invalid args

	/* try to initialize pipe flag */
	if (initpipeflag(fd, data) != 0) { exit(1); }
	/* ---------------------------------------------------------------------- */
	
	/* --------------- RUN GAMESERVER!!! LET THE GAME BEGIN! ---------------- */
	/* start running the main server */
	runmainserver( &players, &quota, &inv , fd, buf, data );
	/* ---------------------------------------------------------------------- */

	/* -------------------- EXITING THE GAME... BYE BYE --------------------- */
	/* exiting, all ok */
	exit(0);
	/* ---------------------------------------------------------------------- */
}
/* END MAIN */
/**************************************************************************/

/**************************************************************************/
/* METHODS' DEFINITION */

/* initialize server's parameters so the game server can begin */
/* Details....
 * RETURNS: 0 -> ALL OK. -1 -> ERROR.
 * CHECKS: all args are there, their types are right, file name of inventory exists,
 *			inventory file has the correct structure and there is no syntax error in
 *			at server's running
 * USE: initialising server's parameters so the game can begin
 */
int setparameters(char **ch, int *p, int *q, inventory *in)
{
	int  i;
	int gp=0, gi=0, gq=0; // flags point out that an argument has been already read (flag==1)

	for (i=1 ; i<7 ; i=i+2)
	{
		/* if following argument is the number of players */
		if (strcmp(ch[i],"-p")==0 && gp==0)
		{
			*p = atoi(ch[i+1]); // save number of players
			gp = 1; // update flag
		}
		/* if following argument is the number of quota */
		else if (strcmp(ch[i],"-q")==0 && gq==0)
		{
			*q = atoi(ch[i+1]); // save number of quota
			gq = 1; // update flag
		}
		/* if following argument is the string that indicates the name of inventory file */
		else if (strcmp(ch[i],"-i")==0 && gi==0)
		{
			/* try to read inventory from file */
			if (readinv(ch[i+1],in)==-1)
			{
				/* error message */
				perror("readinv");
				return -1;
			}
			gi = 1; // update flag
		}
		else
		{
			/* error message */
			printf("error!\n");
			return -1;
		}
	}

	return 0; // all ok
}

/* initialize parent-server's pipe-flag so it will be able to know when to generate a new server */
/* Details....
 * RETURNS: 0 -> ALL OK. -1 -> ERROR.
 * CHECKS: if the value of data (then value of flag) is right and if pipe-flag is setting correctly
 * USE: initialising server's pipe-flag so the first room can begin
 */
int initpipeflag(int fd[2], char data[5])
{
	/* check for correct data value: "open" OR "wait" */
	if (strcmp(data, "open")!=0 && strcmp(data, "close")!=0) 
	{
		printf("Wrong input value\n"); // error message
		return -1; // terminate process if invalid data value
	}

	/* pipe initialization... */
	pipe(fd); // initialize pipe

	write(fd[1], data, strlen(data)); // write from data to pipe

	return 0; // if everything is ok, return 0
}

/* start running main server! */
/* Details....
 * RETURNS: nothing.
 * USE: initializes listen queue and listening socket, waits to open new room 
 *			when it's necessary
 */
void runmainserver(int *players, int *quota, inventory *inv , int fd[2], char buf[5], char data[5])
{
	/* ------------------- DATA DECLARARION --------------------------------- */
	/* listen socket's characteristics */
	int listenfd;
	struct sockaddr_in servaddr;
	/* connection socket's characteristics */
    socklen_t clilen = -1;
    struct sockaddr_in cliaddr;
    /* pid */
    pid_t childppid = 1;
    /* room's data */
	key_t key = 1001; // room key
	/* ---------------------------------------------------------------------- */

	/* --------------------- PRE-RUNNING INITIALIZATIONS -------------------- */
    /* Avoid "zombie" process generation. */
    signal( SIGCHLD, sig_chld );

	/* open listening socket and check if it opens correctly */
	initializesocket(&listenfd, &servaddr);
	if (listenfd<0) { printf("Could not open socket.\n"); exit(1); }

	/* helpful info so can observe the process tree */
	printf("PID : %d\n", getpid());
	/* ---------------------------------------------------------------------- */

	/* ------------------------------- RUN MAIN SERVER... ------------------- */
	for ( ; ; )
	{
		/* child process */
		if (childppid==0)
		{
			/* start running room server */
			runroomserver(players, quota, inv , fd, data , key, listenfd, clilen, cliaddr);
		}
		/* parent process */
		else
		{
			if (read(fd[0], buf, 5) >= 0) // try to read pipe's data, else wait for someone to write in pipe
			{
				buf[4] = '\0'; /* terminate the string */

				/* if need to open new room-server */
				if (strcmp(buf, "open")==0)
				{
					/* opening message */
					printf("Open new room-server...\n");

					/* new room */
					childppid = fork();

					if (childppid!=0) /* so child doesn't write to the pipe */
					{
						/* write "wait" to pipe */
						data[0] = 'w'; data[1] = 'a'; data[2] = 'i'; data[3] = 't'; // w - a - i - t -> wait! :P
						data[4] = '\0'; /* terminate the string */
						write(fd[1], data, strlen(data)); //write "wait" from data to pipe

						/* prepare next room's data */
						key++; // change next room's key
					}
				}
				/* if current room isn't fill yet */
				else if (strcmp(buf, "wait")==0)
				{
					/* waiting message */
					printf("Wait for room to fill\n");
				}
			}
			else
			{
				exit(1); // if could not read data from pipe, exit
			} // if read

		} // if childppid

	} // for (;;)
	/* ---------------------------------------------------------------------- */

	/* close all remaining pipes and sockets */
	close(fd[0]);
	close(fd[1]);
	close(listenfd);
}

/* start running room server! */
/* Details....
 * RETURNS: nothing.
 * USE: steps:
 *			1. accept clients-players while room is not full and check if their inventory is conventional
 *			2. inform pipe that room is full, and there is need for a new room to open
 *			3. put them in waiting mode until the required number players is ok
 *			4. open the chatroom when room is ready
 *			5. sychronize chat
 *			6. wait till everyone lives the room
 *			7. close shared memory and all remaining open sockets
 */
void runroomserver(int *players, int *quota, inventory *inv , int fd[2], char data[5], key_t key, int listenfd, socklen_t clilen, struct sockaddr_in cliaddr)
{
	/* ------------------- DATA DECLARARION --------------------------------- */
	/* pipe counter's data */
	int fdstart[2];
	char bufstart[2];
	char datastart[2] = "0"; // 1->start..
	/* pipe counter's data */
	int fdcounter[2];
	char bufcounter[3];
	char datacounter[3] = "0"; // counting players..
	/* counter */
	char counter[3];
	/* server's pid */
	pid_t childpid = 1;
    /* client's data */
    inventory pinv;
    char pname[20];
	/* temp shared memory data */
	char *shm=NULL;
	int *qshm=NULL;
	/* helpers */
	int tcounter, n, flag, i;
	/* ---------------------------------------------------------------------- */

	/* ------------------- GET ROOM SERVER READY TO BEGIN ------------------- */
	/* open room's semaphore */
	sem = sem_open("semone", O_CREAT, 0600, 1);

	/* check if semaphore opened ok, else exit */
	if (sem == SEM_FAILED)
	{
		perror("sem_open");
		exit(1);
	}

	/* initialize pipe counter */
	initpipe(fdcounter, datacounter);

	/* try to read pipe counter's data, else wait for someone to write in pipe */
	n = read(fdcounter[0], bufcounter, 3);
	if (n >= 0)
	{
		bufcounter[n] = '\0'; /* terminate the string */
	}
	else
	{
		/* error message */
		perror("read pipe");
		exit(1);
	}

	/* initialize local counters */
	strcpy(counter, itoa(*players)) ;
	tcounter = 0;

	/* initialize pipe counter */
	initpipe( fdstart, datastart );

	/* try to read pipe counter's data, so no child will read it before the other */
	n = read(fdstart[0], bufstart, 2);
	if ( n >= 0 )
	{
		bufstart[n] = '\0'; /* terminate the string */
	}
	else
	{
		/* error message */
		perror("read pipe");
		exit(1);
	}

	/* informing message */
	printf("Room Key: %d\n", (int)key);

	/* try to initialize common inventory */
	if (setshm(shm, key, *inv) == -1)
	{
		printf("Problem shared memory\n");
		exit(1);
	}

	/* Create the segment */
	if ((qshmid = shmget(1000, sizeof(int), IPC_CREAT | SHM_R | SHM_W)) < 0)
	{
		/* error message */
		perror("qshmget");
		exit(1);
	}
	/* ---------------------------------------------------------------------- */

	for ( ; ; )
	{
		/* attach the segment to data space */
		if ((qshm = (int *) shmat(qshmid, NULL, 0)) == (int *) -1)
		{
			/* error message*/
			perror("shmat");
			exit(1);
		}

		*qshm = 0;

		/* attach the segment to data space */
		if ((shm = shmat(shmid, NULL, 0)) == (char *) -1)
		{
			/* error message*/
			perror("shmat");
			exit(1);
		}

		/* inform room server's inventory */
		informinv(shm, inv);

		/* get size */
    	clilen = sizeof( cliaddr );

    	/* Copy next request from the queue to connfd and remove it from the queue. */
		connfd = accept( listenfd, ( struct sockaddr * ) &cliaddr, &clilen );

		printf("connfd : %d\n", connfd);

		/* check if socket opens correctly */
		if ( connfd < 0 ) {
			/* Something interrupted us. */
			if ( errno == EINTR ) {continue;} /* Back to while() */
			else {fprintf( stderr, "Accept Error\n" ); exit( 0 );}
		}

		/* Spawn a child. */
		childpid = fork();

		/* Child process. */
		if ( childpid == 0 )
		{
			/* Close listening socket. */
			close(listenfd);
			close(fd[0]);
			close(fd[1]);

			/* read from client */
			if (read( connfd, line, sizeof( char[100] ) ) < 0 ) { exit(1); }

			/* put player's data into struct pinv (inventory) and into pname (char*) */
			stringtodata(line, pname, &pinv);

			/* check if player's inventory is ok and upadate flag */
			flag = checkinventory(inv, pinv, *quota);

			/* print accept or not message */
			if (flag==0)
			{
				/* message to server */
				printf("Player accepted.\n");
				tcounter++; // increase number of players in the room
			}
			else
			{
				/* message to server */
				printf("Access denied.\n");
			}

			/* update pipe counter and shared memory */
			putinvtoshm(shm, *inv);
			tcounter = tcounter - *qshm; // increase number of players in the room

			printf("writing now %d players in shared memory\n", tcounter);

			*qshm = 0;
			strcpy(datacounter, itoa(tcounter));
			write(fdcounter[1], datacounter, strlen(datacounter)); // write from data to pipe

			/* if access denied to player */
			if (flag!=0)
			{
				/* set access denied message */
				strcpy(line, "Access denied...\n");
				/* send access denied message */
				write(connfd, line, sizeof(char[100]));

				/* close all remaining sockets and pipes */
				close(fdcounter[0]);
				close(fdcounter[1]);
				close(fdstart[0]);
				close(fdstart[1]);
				/* close connection with client */
				close(connfd);

				/* not accepted child exit */
				exit(0);
			}

			/* if reach this point, player accepted */
			/* set player into waiting mode until room is ready to begin */
			waitingmode(connfd, line, fdstart, bufstart);

			/* get player into chatroom */
			openchat(pname);

			/* try to read pipe counter's data, else wait for someone to write in pipe */
			n = read(fdcounter[0], bufcounter, 3);
			if ( n >= 0) // try to read pipe counter's data, else wait for someone to write in pipe
			{
			    bufcounter[n] = '\0'; /* terminate the string */
			}
			else
			{
				/* error message */
				perror("read pipe");
				exit(1);
			}

			/* decrease number of players */
			tcounter = atoi(bufcounter);
			tcounter--;

			/* turn into string and write to pipe the new number of players after one gone */
			strcpy(datacounter, itoa(tcounter));
			write(fdcounter[1], datacounter, strlen(datacounter)); // write from data to pipe

			/* if last player is gone, write 'C' to the first position of shared memory, so
			 * room server can close
			 */
			if (strcmp(datacounter, "0")==0) { *shm = 'C'; }

			/* close all remaining sockets and pipes */
			close(fdcounter[0]);
			close(fdcounter[1]);
			close(fdstart[0]);
			close(fdstart[1]);
			/* close connection with client */
			close(connfd);

			/* child exit */
			exit(0);
		}
		/* Parent process. */
		else
		{
			/* Parent closes connected socket */
			close(connfd);

			/* try to read pipe counter's data, else wait for someone to write in pipe */
			n = read(fdcounter[0], bufcounter, 3);
			if (n >= 0) // try to read pipe counter's data, else wait for someone to write in pipe
			{
				bufcounter[n] = '\0'; /* terminate the string */
			}
			else
			{
				/* error message */
				perror("read pipe");
				exit(1);
			}

			/* update tcounter */
			tcounter = atoi(bufcounter);

			/* informing message */
			printf("We have %s players for now\n", bufcounter);

			/* check if room is full and inform pipe flag */
			if (tcounter==*players)
			{
				/* set pipe ready for child processes of the room to be able to terminate */
				strcpy(datacounter, itoa(*players));
				write(fdcounter[1], datacounter, strlen(datacounter)); // write from data to pipe

				/* close the remaining sockets and the pipe counter */
				close(listenfd);
				close(fdcounter[0]);
				close(fdcounter[1]);

				/* close quit shared memory */
				if (shmctl(qshmid, IPC_RMID, (struct shmid_ds *) NULL) != 0)
				{
					/* error message */
					printf("Error deleting quit shared memory\n");
				}

				/* let the next room open... */
				data[0] = 'o'; data[1] = 'p'; data[2] = 'e'; data[3] = 'n'; // o - p - e - n -> open! :P
				data[4] = '\0'; /* terminate the string */
				write(fd[1], data, strlen(data)); // write "wait" from data to pipe

				/* close the remaining pipes */
				close(fd[0]);
				close(fd[1]);

				/* first message send: */
				strcpy(shm, ".I:CHAT\n\n");

				/* send start flag through pipe to all players */
				datastart[0] = '1'; datastart[1] = '\0';
				for (i=0 ; i<*players ; i++)
				{
					write(fdstart[1], datastart, strlen(datastart));
					sleep(1);
				}

				/* close the start pipe */
				close(fdstart[0]);
				close(fdstart[1]);

				/* informing message */
				printf("Room starts!");

				/* open chat room */
				messagehandle(shm, *players);

				/* close shared memory */
    			if (shmctl(shmid, IPC_RMID, (struct shmid_ds *) NULL) == 0)
    			{
					printf("Shared memory deleted, all ok!!!\n");
				}

				/* room-server exit */
				exit(0);

			} // if room full

		} // if child/parent processes

	} // for (;;)
}

/* initialize room-server's pipe-flag so it will be able to know when to create and delete the chat room */
/* Details....
 * RETURNS: nothing.
 * CHECKS: nothing.
 * USE: initialising counter of players for the specific room.
 */
void initpipe(int fd[2], char data[])
{
	/* pipe initialization... */
	pipe(fd); // initialize pipe

	write(fd[1], data, strlen(data)); // write from data to pipe
}

/* initialize socket's characteristics so set able the connection with the client */
/* Details....
 * RETURNS: nothing.
 * USE: tries to open socket and if it is succesful, loads the request queue
 */
void initializesocket(int *listenfd, struct sockaddr_in *servaddr) /* servaddr: Struct for the server socket address. */
{
    /* Avoid "zombie" process generation. */
    signal( SIGCHLD, sig_chld );
	signal( SIGINT, catch_int );

    /* Create the server's endpoint */
    *listenfd = socket( AF_INET, SOCK_STREAM, 0 );

    /* failed to open socket */
    if (*listenfd<0) { exit(1); }

    /* set CLEAR all servaddr fields */
    bzero( servaddr, sizeof( *servaddr ));
    /* set values on servaddr fields*/
    servaddr->sin_family = AF_INET; /* Socket type is INET. */
    servaddr->sin_port = htons(PORT); /* Set the used port number. */
    servaddr->sin_addr.s_addr = INADDR_ANY; /* binds the socket to all available interfaces, not only to local IP */

	/* Create the file for the socket and register it as a socket. */
    bind( *listenfd, ( struct sockaddr* ) servaddr, sizeof( *servaddr ) );
    /* Create request queue. */
    listen( *listenfd, LISTENQ );
}

/* turn string into player's data */
/* Details....
 * RETURNS: nothing.
 * USE: turns string str into player's name (first word in the string) and into inventory
 *			(following words in form (word)\t(word)\n(word)\t(word)... -> before \t: element's
 *			name, after \t: element's quantity)
 */
void stringtodata(char str[], char *pname, inventory *pinv)
{
    int n=0;
    char *t=NULL;

    /* read string from shm while char is not '\n' or '\t' */
	t = strtok(str, "\t\n");

	/* while there are words in the string */
	while(t!=NULL)
    {
    	/* if read first word that supposed to be player's name */
    	if (n==0)
    	{
    		strcpy(pname, t); // save name
    	}
    	/* if word read supposed to be element's name*/
    	else if (n%2==1)
    	{
    		strcpy(pinv->element[((n-1)/2)], t); // save element's name
    	}
    	/* if word read supposed to be element's quantity */
    	else
    	{
    		pinv->quantity[((n-1)/2)] = atoi(t); // save element's quantity
    	}

    	n++; // counting words in the string after the name

	    /* begin from previous ending point and read string while char is not '\n' or '\t' */
    	t = strtok(NULL, "\t\n");
    } // while

    /* save number of elements in the inventory */
    pinv->num = n/2;
}

/* check if player's inventory is conventional to participate */
/* Details....
 * RETURNS: 0 -> player can play in this game, -1 -> player rejected.
 * CHECKS: if player's inventory is conventional with server's inventory and quota
 * USE: just performs the final check, so server reject or accept the already
 *			connected client and if player's inventory is ok, change server's 
 *			inventory.
 */
int checkinventory(inventory *sinv, inventory cinv, int sumq)
{
	int i,j,flag,sum;
	inventory tinv; // temp inventory object, so changes don't be saved before they are checked

	/* save initial inventory, so if player's quota is not ok server's quota stays clear */
	tinv = *sinv;

	/* calculate sum of quota */
	sum = 0;
	for (i=0 ; i<cinv.num ; i++)
	{
		sum += cinv.quantity[i];
	}

	/* check if sum of asking quota is conventional */
	if (sum > sumq)
	{
		return -1;	// too much quota
	}

	/* check if inventory element's names exist and then if the asked quantity is available */
	for (i=0 ; i<cinv.num ; i++)
	{
		j=0; flag=0;
		/* is the element part of game's inventory? */
		while (flag==0 && j<tinv.num)
		{
			if (strcmp(cinv.element[i], tinv.element[j])==0)
			{
				flag = 1; // element found
			}
			else
			{
				j++; // check next element
			}
		}

		/* the element doesn't exist */
		if (flag==0)
		{
			return -1;
		}
		/* the element exist */
		else
		{
			/* check if the element's asked quantity is available */
			if ((tinv.quantity[j] - cinv.quantity[i]) < 0)
			{
				return -1;	// the quantity of the element is not enough
			}
			else
			{
				tinv.quantity[j] -= cinv.quantity[i];	// decreasing the quantity.
				/* This change is not visible to main block, it's temporary, so if 
				for example player asks for gold twice, program will be informed if
				both decreases are able to be done */
			}
		}
	}

	/* if reached this point, no problem occured, so save changes*/
	*sinv = tinv;

	return 0; // all ok
}

/* open shared memory and put inventory inside */
/* Details....
 * RETURNS: 0 -> shared memory opened ok, and inventory put in ok, -1 -> shared memory didn't open.
 * CHECKS: if shared memory opens ok
 * USE: assign room's key to shared memory and try to open shared memory.
 *			if shared memory opens ok, put inventory's data into it.
 */
int setshm(char *shm, key_t key, inventory inv)
{
	/* Create the segment */
	if ((shmid = shmget(key, 256, IPC_CREAT | SHM_R | SHM_W)) < 0)
	{
		/* error message */
		perror("shmget");
		return -1;
	}

	/* call function to put initial inventory into shared memory, so this room can handle its own inventory */
	putinvtoshm(shm, inv);

	/* shared memory opened ok and inventory data is written into it */
	return 0;
}

/* get inventory struct into shared memory */
/* Details....
 * RETURNS: nothing.
 * CHECKS: if shared memory is attached succesfully, else exit the program.
 * USE: put inventory's data into shared memory. for every element, put its
 *			quantity into shared memory as char*. because elements array's
 *			row is not changed, there is no need to save elements' names, just
 *			their quantities.
 */
void putinvtoshm(char *shm, inventory inv)
{
	char *temp, *t; // read-write to shared memory variables
	int i, j; // just helping

	/* Attach the segment to data space */
	if ((shm = shmat(shmid, NULL, 0)) == (char *) -1)
	{
		/* error message */
		perror("shmat");
		exit(1);
	}

	/* temp char* so it will be able to write to the shm */
	temp = shm;

	/* put inventory into the memory so the program can checks if client's inventory is conventional with this room */
	/* turn the first element's quantity from integer into char* */
	t = itoa(inv.quantity[0]);

	/* write first numder (element's quantity) into shared memory */
	j = 0;
	while (t[j]!='\0')
	{
		*temp++ = t[j];
		j++;
	}

	/* for every element, write its quantity into shared memory */
	for (i=1 ; i<inv.num ; i++)
	{
		/* write \t */
		*temp++ = '\t';

		/* turn 'i' element's quantity from integer into char* */
		t = itoa(inv.quantity[i]);

		/* write 'i' numder (element's quantity) into shared memory */
		j = 0;
		while (t[j]!='\0')
		{
			*temp++ = t[j];
			j++;
		} // while
	} // for

	/* terminate string */
	*temp++ = '\n';
	*temp++ = '\0';
}

/* get data from shared memory and put them in the field 'quantity' of the given inventory */
/* Details....
 * RETURNS: nothing.
 * CHECKS: nothing.
 * USE: read char* from shared memory and read every word in it. for each 'i' word
 *			turn string (word) into integer and set quantity of 'i' element.
 */
void informinv(char *shm, inventory *inv)
{
	int i;
    char *t=NULL; // read parts of string variable

    /* read string from shm while char is not '\n' or '\t' */
	t = strtok(shm, "\t\n");

	/* for every element inform its quantity */
	for (i=0 ; i<inv->num ; i++)
	{
		/* inform 'i' element's quantity */
		inv->quantity[i] = atoi(t); // atoi -> string to integer

	    /* begin from previous ending point and read string while char is not '\n' or '\t' */
    	t = strtok(NULL, "\t\n");
	}
}

/* send messages to accepted player while waiting for room to fill */
/* Details....
 * RETURNS: nothing.
 * CHECKS: if pipe flag to start is ok, and has the correct data.
 * USE: send "access ok" message, start sending waiting message every 5 seconds,
 *			while waiting to read from pipe flag. when room is full able to read
 *			from pipe, check flag, send start message and return.
 *			
 */
void waitingmode(int connfd, char line[100], int fd[2], char buf[2])
{
	int n;

	/* set signal handler for SIGALRM */
	signal(SIGALRM, alarmclient);

	/* set access message */
	strcpy(line, "Access OK\n");
	/* send access message */
	write(connfd, line, sizeof(char[100]));

	/* set waiting message */
	strcpy(line, "Wait for room to fill...\n");

	/* call alarm function, so sending waiting message to client every 5 seconds */
	alarm(5);

	/* waiting while read from pipe */
	if ((n = read(fd[0], buf, 2)) >= 0)
	{
		buf[n] = '\0'; /* terminate the string */
	}
	else
	{
		exit(1); /* if something went wrong exit */
	}

	/* stop alarm */
	alarm(0);

	/* check if flag is ok */
	if (buf[0]!='1')
		exit(1); // if flag is not ok, exit

	/* set start message */
	strcpy(line, "START\n");
	/* send start message */
	write(connfd, line, sizeof(char[100]));
}

/* open reading and writing thread */
/* Details....
 * RETURNS: nothing.
 * CHECKS: nothing.
 * USE: open reading thread, open writing thread and waiting until reading
 *			thread return/quit. wait just for the reading thread, because
 *			it returns when client quits. ifreadplayer detects client's
 *			detachment, so it's enough just to wait the reading thread to end.
 */
void openchat(void *nam)
{
       pthread_t idread, idwrite;

       /* open threads */
       pthread_create(&idread, NULL, ifreadplayer, nam); // open reading thread
       pthread_create(&idwrite, NULL, ifwriteplayer, NULL); // open writing thread

       /* wait till reading thread to return/quit */
       pthread_join(idread, NULL);
}

/* lock playerserver in reading from client mode until player detaches */
/* Details....
 * RETURNS: NULL.
 * CHECKS: if player is connected to serverserver.
 * USE: lock player into a while that reads data from client while is connected.
 *			when you read message from client, write it to shared memory and estamblish
 *			that message is send from player ('P' in second position declares that the 
 *			following message has been just send from player and no other player has
 *			read it yet). when player quits, inform room server through shared memory that
 *			one player is gone ('-' character in first position of shared memory) and
 *			return NULL.
 */
void *ifreadplayer(void *nam)
{
	char *shm;
	char *temp;
	char line[100];
	int i=0;

	char *name = (char *) nam; // set name

	/* attach the segment to our data space */
	if ((shm = shmat(shmid, NULL, 0)) == (char *) -1)
	{
		/* print error message */
		perror("shmat");
		exit(1);
	}

	/* while server-client connection is open, so read function return positive value */
	while(read(connfd, line , sizeof(char[100]))>0)
	{
		/* initialize temp char pointer at third position of shared memory */
		temp = shm + 2;

		/* set shared memory form: ".#:#########" (# -> irrelevant character) */
		*temp++ = ':';

		/* put player's name in front of message */
		*temp++ = '('; i=0;

		while (*(name+i)!='\0')
		{
			*temp++ = *(name+i);
			i++;
		}

		*temp++ = ')';
		*temp++ = ' ';

		i=0;

		/* copy line's data (client's message) to shared memory */
		while (line[i]!='\0')
		{
			*temp++ = line[i];
			i++;
		}

		*temp++ = line[i];

		/* now shared memory form: ".#:(new message)" */

		/* set shared memory form: ".P:(new message)" ('P' -> the following message 
		 * is from player and no other player has read it yet)
		 */
		*(shm+1) = 'P';

		/* clear temp char pointer and i counter, so next time write to shared memory correctly */
		temp = NULL;
		i = 0;
	}

	/* set shared memory form: "-###########" (# -> irrelevant character) */
	/* '-' character at first position of shared memory indicates that player
	 * is gone, and roomserver have to decrease the number of players in the
	 * room.
	 */
	*shm = '-';

	/* goodbye message from player */
	/* initialize temp char pointer at third position of shared memory */
	temp = shm + 2;

	/* set shared memory form: ".#:#########" (# -> irrelevant character) */
	*temp++ = ':';

	/* put player's name in front of message */
	*temp++ = '('; i=0;

	while (*(name+i)!='\0')
	{
		*temp++ = *(name+i);
		i++;
	}

	*temp++ = ')';
	*temp++ = ' ';

	i=0;

	strcpy(line,"-> player is gone...\0");

	/* copy line's data (client's message) to shared memory */
	while (line[i]!='\0')
	{
		*temp++ = line[i];
		i++;
	}

	*temp++ = line[i];

	/* now shared memory form: ".#:(new message)" */

	/* set shared memory form: ".P:(new message)" ('P' -> the following message 
	 * is from player and no other player has read it yet)
	 */
	*(shm+1) = 'P';




	/* function type is void* so return NULL */
	return NULL;
}

/* lock playerserver in writing to client mode until ifreadplayer returns == client detaches */
/* Details....
 * RETURNS: NULL.
 * CHECKS: if player is connected to serverplayer.
 * USE: lock playerserver into a while that writes data to client while is connected.
 *			all player servers see a shared memory, which works as a temporery buffer, so they
 *			all can see the written messages. when a player writes into shared memory, the
 *			second position character turns into 'P' so all players get that a new message is on
 *			shared memory. then they stay in waiting mode, until every playerserver identifies
 *			that the message in shared memory is new and get ready to read and send the message.
 */
void *ifwriteplayer()
{
	char *shm;
	int flag=0; // check message is read (flag==1) flag
	int wflag=1; // check write works flag
	char line[100];
	char temp[3];

	// int connfd = * (int *) cfd; // set connfd

	/* attach the segment to our data space */
	if ((shm = shmat(shmid, NULL, 0)) == (char *) -1)
	{
		/* print error message */
		perror("shmat");
		exit(1);
	}

	/* check if write works allright and that room is not closed */
	while (wflag>0 && *shm!='C')
	{
		/* declare that previous message has been read from shared memory and send to client */
		flag = 1;

		/* while the message that shared memory contains has been read, don't do anything */
		while (flag==1 && *(shm+1)=='S')
			sleep(0);

		/* passing through this point, means that character in second position of shared memory is not 'S'
		 * so there are two possibilities:
		 * 		1.	a new message has been written in shared memory -> go ahead and count players passed through,
		 *			so they can all see the new message.
		 *		2.	room is closed, shared memory is deleted, terminate writing thread by closing all the threads
		 *			and sockets
		 */

		/* going ahead... */
		/* only one player can enter this point at the time, so there is no conflict when 
		 * two players try to write into shared memory and state they passed through by increasing 
		 * the number in second and third position in shared memory (eg: if two players come 
		 * simultaneously, do not write both the same number, the one will be waiting the other 
		 * to finish)
		 */

		sem_wait(sem);

		/* check if player entering this point is the first one writing into shared memory */
		if (*(shm+1)=='P')
		{
			/* put "01" in second and third position of shared memory if player is first */
			*(shm+1) = '0';
			*(shm+2) = '1';
		}
		else
		{
			/* read characters of second and third position of shared memory 
			 * and put them into string (temp, length=3). temp == char[3], cause
			 * number of players should not get ahead 99 (i don't know game with 100 players :P)
			 */
			temp[0] = *(shm+1);
			temp[1] = *(shm+2);
			temp[2] = '\0';

			/* save new number of players (+1) passed through this point into temp (string form) */
			strcpy(temp, itoa(atoi(temp)+1));

			/* check if number of players passed through is above 9 */
			if (atoi(temp)>9)
			{
				/* put the two digits into the second and the third position of shared memory */
				*(shm+1) = temp[0];
	            *(shm+2) = temp[1];
	     	}
			else
			{
				/* put the one digit into the third position of shared memory and '0' into the second one */
				*(shm+1) = '0';
	            *(shm+2) = temp[0];
			}
		}

		/* free semaphore, so next player can enter the writing into shared memory point */
		sem_post(sem);

		/* declare that there is a new message in shared memory that has not been read and send yet */
		flag = 0;

		/* wait until room server makes sure that all playerservers are ready to read and send the new message */
		while (*(shm+1)!='S' && flag==0)
			sleep(0);

		/* passing through this point means that all playerservers are ready to read and send the message, and
		 * roomserver has changed the character into second position of shared memory to 'S' so the playerservers 
		 * start reading from shared memory.
		 */

		/* copy message from shared memory into line */
		strcpy(line, (shm+3));

		/* write line to client, update wflag so point out write works correctly */
		wflag = write(connfd, line, sizeof(char[100]))>0;
	}

	/* function type is void* so return NULL */
	return NULL;
}

/* called by roomserver, so can synchronize chat */
/* Details....
 * RETURNS: nothing.
 * CHECKS: if room has players in it.
 * USE: while there are players in the room, check when a playerserver writes a new message in
 *			shared memory. when that happens, check when all playerservers are ready to read the 
 *			the new message from shared memory. when everyone is ready, allow them to read the new
 *			message from shared memory.
 *			check all the time if a playerserver is gone, so can update the number of players who
 *			are connected.
 */
void messagehandle(char *shm, int PLAYERS)
{
	char temp[3];

	/* initialize shared memory's state */
	*shm = '.';
	*(shm+1) = 'S';

	/* while there is at least one player in the room */
	while (*shm != 'C')
	{
		/* check if some playerserver has written a new message into shared memory */
		if (*(shm+1)!='S')
		{
			/* wait until all players are ready, and check when they are */
			while(1)
			{
				/* copy number of playerservers that are ready to read new message and send it */
				temp[0] = *(shm+1);
				temp[1] = *(shm+2);
				temp[2] = '\0';

				/* if everyone is ready, break */
				if (PLAYERS==atoi(temp)) { break; }

				/* checks for players exiting the room while a new message is sent */
				if (*shm == '-')
				{
					/* set first character of shared memory '.', the decrease is done */	
					*shm = '.';

					/* decrease number of players */
					PLAYERS--;
				} // if '-'

			} // while

			/* allow playerservers to read and send new message */
			*(shm+1) = 'S';
			*(shm+2) = ':';
		} // if

		/* checks for players exiting the room */
		if (*shm == '-')
		{
			/* set first character of shared memory '.', the decrease is done */	
			*shm = '.';

			/* decrease number of players */
			PLAYERS--;
		} // if '-'
	} // while 'C'
}

/* END METHODS' DEFINITION */
/**************************************************************************/