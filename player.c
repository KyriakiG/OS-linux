#include "mutual.h"

int sockfd;
int writeMe=0; // flag up when this player sends a message

/* set exit signal handler, so print exiting message and close shared memory that is still open */
void ccatch_int(int signo)
{
	(void) signo; // just to avoid warning...

	/* exiting message */
	printf("\n\t\tPlayer is gone...\n\n");	

	/* close socket sockfd */
	close(sockfd);

	/* main server exiting */
	exit(0);
}


/**************************************************************************/
/* METHODS' DECLARATION */

/* initialize client's parameters */
int setparameters(char **ch, char **name, inventory *in, char **shost);

/* connect client with server */
void connclient(int *sockfd, struct sockaddr_in *servaddr, struct hostent *server, char *serverhost);

/* send player's data to server */
void senddata(int *sockfd, char *name, inventory inv);

/* turn player's data into char* */
void datatostring(char str[], char *name, inventory inv);

/* open reading and writing thread */
void openchat(int *sockfd);

/* read user's message */
void readfromter(char line[100]);

/* read user's message */
void *ifread(void *sfd);

/* write to server */
void *ifwrite(void *sfd);

/* END METHODS' DECLARATION */
/**************************************************************************/

/**************************************************************************/
/* MAIN */
int main(int argc, char **argv)
{
	/* ------------------- DATA DECLARARION --------------------------------- */
	/* arg-data, gamer's data */
	char *name = NULL, *serverhost = NULL;
	inventory inv;
	/* client's */
//	int sockfd;
	struct sockaddr_in servaddr; /* Struct for the server socket address. */
	struct hostent *server=NULL;
	/* helpful variables */
	char send[100];
	/* ---------------------------------------------------------------------- */

	signal(SIGINT, ccatch_int);

	/* ------------ SET GAMER'S SO HE 'LL BE ABLE TO PLAY ------------------- */
	/* check for correct number of input args */
	if (argc!=6) { return -1; }

	/* read arguments and check for correct type and right value of them */
	if (setparameters(argv, &name, &inv, &serverhost) != 0)
	{
		/* error message */
		perror("set parameters");
		return -1;
	}
	/* ---------------------------------------------------------------------- */

	/* ------------ GET READY TO CONNECT WITH SERVER ------------------------ */
	/* connect with server */
	connclient(&sockfd, &servaddr, server, serverhost);
	if (sockfd < 0) { printf("Couldn't connect to server.\n"); exit(1); }

	/* ---------------------------------------------------------------------- */

	/* --------- SEND DATA TO SERVER AND WAIT FOR ACCEPT -------------------- */
	/* send name and inventory to server */
	senddata(&sockfd, name, inv);

	/* check if access to the room is ok and enter in waiting mode */
	while(read(sockfd, send , sizeof(char[100]))>0)
	{
		/* show data send from server */
		printf("Data from server: %s\n", send);

		/* if get a START message break, to enter chatroom! */
		if (strcmp(send, "START\n")==0)
			break;

		/* if access is denied exit */
		if (strcmp(send, "Access denied...\n")==0)
		{
			/* close socket sockfd and exit */
			close(sockfd);
			exit(0);
		}
	}
	/* ---------------------------------------------------------------------- */

	/* --------------------- OPEN CHATROOM!!! ------------------------------- */
	/* open chatroom */
	openchat(&sockfd);
	/* ---------------------------------------------------------------------- */

	/* ------------------------- BYE BYE... --------------------------------- */
	/* close socket sockfd */
	close(sockfd);

	/* client exit */
	exit(0);
}
/* END MAIN */
/**************************************************************************/

/**************************************************************************/
/* METHODS' DEFINITION */

/* initialize client's parameters */
/* Details....
 * RETURNS: 0 -> ALL OK. -1 -> ERROR.
 * CHECKS: all args are there, their types are right, file name of inventory exists,
 *			inventory file has the correct structure and there is no syntax error in
 *			it
 * USE: initialising client's parameters so gets in waiting list to play
 */
int setparameters(char **ch, char **name, inventory *in, char **shost)
{
	int  i;
	int gn=0, gi=0, gh=0, minus=0; // flags point out that an argument has been already read (flag==1)

	for (i=1 ; i<7 ; i=i+2)
	{
		/* if following argument is the name of the player */
		if (strcmp(ch[i+minus],"-n")==0 && gn==0)
		{
			*name = ch[i+minus+1];
			gn = 1; // update flag
		}
		/* if following argument is the string that indicates the name of inventory file */
		else if (strcmp(ch[i+minus],"-i")==0 && gi==0)
		{
			if (readinv(ch[i+minus+1],in)==-1) {return -1;}
			gi = 1; // update flag

		}
		/* if following argument is the hostname */
		else if (minus==0 && gh==0)
		{
			*shost = ch[i];
			gh = 1; minus = -1; // update flags
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

/* connect client with server */
/* Details....
 * RETURNS: nothing.
 * CHECKS: if hostname and connection are ok.
 * USE: get client ready to attach to server, and open client-server connection
 */
void connclient(int *sockfd, struct sockaddr_in *servaddr, struct hostent *server, char *serverhost)
{
	/* check if hostname is ok */
	server = gethostbyname(serverhost);
	if ( server == NULL ) {
		/* error message */
		fprintf(stderr, "Invalid hostname \n"); //
		exit(1);
	}

	/* Create the client's endpoint. */
	*sockfd = socket( AF_INET, SOCK_STREAM, 0 );

    /* set CLEAR all servaddr fields */
    bzero( servaddr, sizeof( *servaddr ));
    /* set values on servaddr fields*/
	// copying server values to the hostent struct
	bcopy((char *)server->h_addr, (char *)&servaddr->sin_addr.s_addr, server->h_length);
    servaddr->sin_family = AF_INET; /* Socket type is INET. */
    servaddr->sin_port = htons(PORT); /* Set the used port number. */

    /* Connect the client's and the server's endpoint. */
	connect(*sockfd, (struct sockaddr *) servaddr, sizeof(*servaddr));
}

/* send player's data to server */
/* Details....
 * RETURNS: nothing
 * USE: send inventory and name to server
 */
void senddata(int *sockfd, char *name, inventory inv)
{
	char send[100];

	/* call datatostring function to parse inventory and name into string */
	datatostring(send, name, inv);

	/* send data to server */
	write(*sockfd, send, sizeof(char[100]));
}

/* turn player's data into char* */
/* Details....
 * RETURNS: nothing
 * USE: turns players data and inventory into char*
 */
void datatostring(char str[], char *name, inventory inv)
{
	int i;
	char *temp;

	/* set ready the string that client sends to server */
	/* write player's name in the string */
	strcpy(str, name);

	/* write all the elements' names and their quantities into the string one by one */
	for (i=0 ; i<(inv.num) ; i++)
	{
		/* write i element's name */
		strcat(str, "\n");
		strcat(str, inv.element[i]);
		/* write i element's quantity */
		strcat(str, "\t");
		temp = itoa(inv.quantity[i]); // turn quantity from integer to string
		strcat(str, temp);
	}

	/* write \n\n at the end of the string */
	strcat(str, "\n");
	strcat(str, "\n");
}

/* open reading and writing thread */
/* Details....
 * RETURNS: nothing.
 * CHECKS: nothing.
 * USE: open reading thread, open writing thread and waiting until both
 *			threads return/quit. wait for the both threads to end, 
 *			which happens when client quits (Ctrl-C).
 */
void openchat(int *sockfd)
{
       pthread_t idread, idwrite;

       /* open threads */
       pthread_create(&idread, NULL, ifread, sockfd);
       pthread_create(&idwrite, NULL, ifwrite, sockfd);
       
       /* wait till reading thread to return/quit */
       pthread_join(idread, NULL);
       pthread_join(idwrite, NULL);
}

/* read user's message */
/* Details....
 * RETURNS: nothing.
 * CHECKS: nothing.
 * USE: read line from terminal
 */
void readfromter(char line[100])
{
	char temp = NULL;
	int i=0;

	/* read first character */
	temp = getchar();

	/* while user doesn's type \n read character and save it into a string */
	while ((temp!='\n' && i!=99) || i==0)
	{
		/* save character */
		line[i] = temp;

		/* increase index */
		i++;

		/* don't send empty text */
		if (i==1 && (line[0]=='\n' || line[0]==' '))
			i = 0;

		/* read next character */
		temp = getchar();
	}

	/* terminating string */
	line[i] = '\0';
	writeMe = 1; // acknowledge that message is sent from me
}

/* read from server */
/* Details....
 * RETURNS: nothing.
 * USE: stay in reading mode and print every not empty message, while
 *			connection is open.
 */
void *ifread(void *sfd)
{
	char line[100];

	int sockfd = * (int *) sfd; // set sockfd

	line[0] = '\0'; // initialize string

	/* read from server, until connection is open */
	while(read(sockfd, line, sizeof(char[100]))>0)
	{
		/* if message is not empty, print it to the screen */
		if (line[0] != '\0' && line[0] != '\n' && writeMe==0)
		{
			printf("%s\n", line);
			line[0] = '\0'; // set line to initial value again
		}
		writeMe = 0; // after a message is read or not, set i sent message to 0
	}

	/* function type is void* so return NULL */
	return NULL;
}

/* write to server */
/* Details....
 * RETURNS: nothing.
 * USE: stay in writing mode and send every message to server, while
 *			connection is open.
 */
void *ifwrite(void *sfd)
{
	char line[100];

	int sockfd = * (int *) sfd;

	/* read from terminal and write to server, untill connection is open */
	do {

		/* read from terminal */
		readfromter(line);

	} while (write(sockfd, line, sizeof(char[100]))>=0); // check for open connection

	/* function type is void* so return NULL */
	return NULL;
}
/* END METHODS' DEFINITION */
/**************************************************************************/