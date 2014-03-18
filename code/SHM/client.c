/***************************************************************************
    copyright            : (C) 2014 Andrea Bulgarelli
                               2014 Andrea Zoli
    email                : bulgarelli@iasfbo.inaf.it
                           zoli@iasfbo.inaf.it
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "shm_common.h"

#ifdef __MACH__
#include <sys/time.h>
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0
//clock_gettime is not implemented on OSX
int clock_gettime(int clk_id, struct timespec* t) {
    struct timeval now;
    int rv = gettimeofday(&now, NULL);
    if (rv) return rv;
    t->tv_sec  = now.tv_sec;
    t->tv_nsec = now.tv_usec * 1000;
    return 0;
}
#else
#include <time.h>
#endif


int main()
{
  int i;
  int shmid;
  key_t key;
  char *shm;
  sem_t *mutex, *mutexsync;
  struct timespec start, stop;

  //name the shared memory segment
  key = 111;

  //create & initialize semaphore
  mutex = sem_open(SEM_NAME,O_CREAT,0644,1);
  if(mutex == SEM_FAILED)
  {
    perror("unable to create semaphore");
    sem_unlink(SEM_NAME);
    exit(-1);
  }

  //create the shared memory segment with this key
  shmid = shmget(key,PACKETSZ,IPC_CREAT|0666);
  if(shmid<0)
  {
    perror("failure in shmget");
    exit(-1);
  }

  //attach this segment to virtual memory
  shm = shmat(shmid,NULL,0);

  //wait the server startup
  printf("Waiting the server..\n"); fflush(stdout);
  *shm = 0;
  while(*shm != '*') usleep(100000);
  printf("Running..\n"); fflush(stdout);

  //open sync semaphore
  mutexsync = sem_open(SYNC_SEM_NAME,0,0644,0);
  if(mutexsync == SEM_FAILED)
  {
    perror("unable to open semaphore");
    sem_close(mutexsync);
    exit(-1);
  }

  //start writing into memory
  clock_gettime( CLOCK_MONOTONIC, &start);

  for(i=0;i<NPACKETS;i++)
  {
    //printf("Writing %d...\n", i);
    sem_wait(mutex);
    memset(shm, 'A', PACKETSZ);
    sem_post(mutex);
  }

  //wait for the lock of sync mutex
  sem_wait(mutexsync);
  clock_gettime( CLOCK_MONOTONIC, &stop);

  double MB = ((double)NPACKETS * (double)PACKETSZ) / (double)1000000;
  double secs = (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) / (double)1000000000.0;
  double MBs = MB / secs;
  printf("throughput: %.3f MB/s\n", MBs);
  sem_close(mutex);
  sem_close(mutexsync);
  sem_unlink(SEM_NAME);
  sem_unlink(SYNC_SEM_NAME);
  shmctl(shmid, IPC_RMID, 0);

  return 0;
}
