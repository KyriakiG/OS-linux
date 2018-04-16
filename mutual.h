#include <stdio.h>
#include <unistd.h>
#include <sys/types.h> 		/* basic system data types */
#include <sys/socket.h> 	/* basic socket definitions */
#include <errno.h> 			/* for the EINTR constant */
#include <sys/wait.h> 		/* for the waitpid() system call */
#include <sys/un.h> 		/* for Unix domain sockets */
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>		/* to use AF_NET*/

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <netdb.h>			/* port initialization */

#include <pthread.h>		/* threads' header */

#include <semaphore.h>		/* semaphores' header */
#include <fcntl.h>			/* for 0_CREATE */
/**************************************************************************/

/**************************************************************************/
/* DEFINE */

#define INVNUM 100
#define PORT 55000

/* copied for itoa() */
#define INT_DIGITS 19		/* enough for 64 bit integer */

/* END DEFINE */
/**************************************************************************/

/**************************************************************************/
/* STRUCTS */

/* helpful struct inventory */
typedef struct inventory
{
	char element[INVNUM][INVNUM]; // elements' names
	int quantity[INVNUM]; // element's quantity
	int num; // number of elements
}inventory;

/* END STRUCTS */
/**************************************************************************/


/**************************************************************************/
/* METHODS' DECLARATION */

/* method to read inventory file and put its data into a struct inventory */
int readinv(char *fname, inventory *in);

/* copy of itoa() */
char *itoa(int i);

/* END METHODS' DECLARATION */
/**************************************************************************/


/**************************************************************************/
/* METHODS' DEFINITION */

/* method to read inventory file and put its data into a struct inventory */
/* Details....
 * RETURNS: 0 -> file's struct is ok. -1 -> file's struct is not correct.
 * CHECKS: if inventory file has the required form.
 * USE: read the data from the given inventory file if it exists.
 *			then try to put them into a struct while checking that
 *			data's kind is correct.
 */
int readinv(char *fname, inventory *in)
{
	FILE *fp;			// file's pointer
	int tempq=0;		// temp value to read integers from the file
	char tempe[INVNUM];	// temp value to read strings from the file

	/* trying to open file for reading ("r") */
	fp = fopen(fname,"r");
 
 	/* check if file exists */
 	/* if file doesn't exists */
   	if( fp == NULL )
	{
		/* print error message */
		perror("Error while opening the file.\n");
		exit(EXIT_FAILURE);

		/* return -1 so point out something went wrong */
		return -1;
	}

	/* if file exist, go on */

	/* set number of elements 0 */
	in->num = 0;

	/* reading from file... */
	while (!feof(fp))
	{
		/* try to read line after line, checking that line's form is the required one */
		if (fscanf(fp, "%s\t%d", tempe, &tempq)<0)
		{
			/* line has not the correct form, return -1 so point out something went wrong */
			return -1;
		}
		
		strcpy(in->element[in->num], tempe);	// save element's name to server's inventory struct
		in->quantity[in->num] = tempq;	// save element's quantity to server's inventory struct
		in->num++;	// increase index of elements in server's inventory
	}
 
 	/* close file's pointer */
	fclose(fp);

	/* return 0 so point out everything is ok */
	return 0;
}

/* copy of itoa() */
/* Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */
char *itoa(int i)
{
  /* Room for INT_DIGITS digits, - and '\0' */
  static char buf[INT_DIGITS + 2];
  char *p = buf + INT_DIGITS + 1;	/* points to terminating '\0' */
  if (i >= 0) {
    do {
      *--p = '0' + (i % 10);
      i /= 10;
    } while (i != 0);
    return p;
  }
  else {			/* i < 0 */
    do {
      *--p = '0' - (i % 10);
      i /= 10;
    } while (i != 0);
    *--p = '-';
  }
  return p;
}
/* END METHODS' DEFINITION */
/**************************************************************************/