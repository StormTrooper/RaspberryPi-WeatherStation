/*
 *     	locking.c:
 *      Prevents rapid use of application by implementing a file lock
 *      technion@lolware.net
 */

#include <sys/file.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifdef DEBUG
int ldebug(char *debug_info)
{
        FILE *fp;
        fp=fopen("/tmp/debug.log", "a");
        fprintf(fp, debug_info);
        fprintf(fp, "\n");
        fclose(fp);
        return 0;
}
#endif



int open_lockfile(const char *filename)

{
   int fd;
   fd = open(filename, O_CREAT | O_RDONLY, 0600);

   if (fd < 0)
   {
      	#ifdef DEBUG
        ldebug("Failed to access lock file");
        #endif

	printf("Failed to access lock file: %s\nerror: %s\n",
		filename, strerror(errno));
      exit(EXIT_FAILURE);
   }
   
   while(flock(fd, LOCK_EX | LOCK_NB) == -1)
   {
      if(errno == EWOULDBLOCK)
      {
         printf("Lock file is in use, exiting...\n");
         /* If the lockfile is in use, we COULD sleep and try again.
          * However, a lockfile would more likely indicate an already runaway
	  * process.
         */
	#ifdef DEBUG
        ldebug("Lock file in use");
        #endif

	 exit(EXIT_FAILURE);
      }
      perror("Flock failed");

	#ifdef DEBUG
        ldebug("flock failed");
        #endif

      exit(EXIT_FAILURE); 
   }
   return fd;
}

void close_lockfile(int fd)
{
   if(flock(fd, LOCK_UN) == -1)
   {
      perror("Failed to unlock file");
      exit(EXIT_FAILURE);
   }
   if(close(fd) == -1)
   {
      perror("Closing descriptor on lock file failed");
      exit(EXIT_FAILURE);
   }
}
