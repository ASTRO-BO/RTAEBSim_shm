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


int main()
{
  int i;
  int shmid;
  key_t key;
  char *shm;
  sem_t *mutex, *mutexsync;
  char vec[PACKETSZ];

  //name the shared memory segment
  key = 111;

  //create & initialize existing semaphore
  mutex = sem_open(SEM_NAME,0,0644,0);
  if(mutex == SEM_FAILED)
  {
    perror("reader:unable to execute semaphore");
    sem_close(mutex);
    exit(-1);
  }

  //create & initialize sync semaphore
  mutexsync = sem_open(SYNC_SEM_NAME,O_CREAT,0644,1);
  if(mutexsync == SEM_FAILED)
  {
    perror("unable to create semaphore");
    sem_unlink(SYNC_SEM_NAME);
    exit(-1);
  }

  //create the shared memory segment with this key
  shmid = shmget(key,PACKETSZ,0666);
  if(shmid<0)
  {
    perror("reader:failure in shmget");
    exit(-1);
  }

  //attach this segment to virtual memory
  shm = shmat(shmid,NULL,0);

  //lock sync mutex
  sem_wait(mutexsync);

  //unlock the client
  *shm = '*';

  //wait the first packet
  printf("Waiting the client..\n"); fflush(stdout);
  while(*shm == '*') usleep(100000);
  printf("Running..\n"); fflush(stdout);

  //start reading
  for(i=0;i<NPACKETS;i++)
  {
    //printf("Reading %d...\n", i); fflush(stdout);
    sem_wait(mutex);
    memcpy(vec, shm, PACKETSZ);
    sem_post(mutex);
  }

  //unlock sync mutex
  sem_post(mutexsync);

  sem_close(mutex);
  sem_close(mutexsync);
  shmctl(shmid, IPC_RMID, 0);

  return 0;
}
