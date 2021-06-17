#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>
#include <sem.h>

void* latest_mmap_addr;
void *__real_mmap(void* addr, size_t len, int prot, int flags, int fildes, off_t off);

static char msg1[TPS_SIZE] = "Hello world!\n";
static char msg2[TPS_SIZE] = "hello world!\n";
static char msg3[25] = "Testing this please pass\n";
static char msg4[TPS_SIZE] = "More testing\n";
static char msg5[TPS_SIZE] = "I am so messy :(\n";
static int int1[3] = {1, 2, 3};

static sem_t sem1, sem2;
void *__wrap_mmap(void* addr, size_t len, int prot, int flags, int fildes, off_t off)
{
  latest_mmap_addr = __real_mmap(addr, len, prot, flags, fildes, off);
  return latest_mmap_addr;
}
/***********************************************************************************/

void *thread0( void* tps_addr)
{
  printf("Address: %s\n",(char*)tps_addr);
  return NULL;
}
/***********************************************************************************/

void *SegHandler()
{
  pthread_t tid;
  tps_create();
  char *tps_addr = latest_mmap_addr;
  pthread_create(&tid, NULL, thread0, (void*)tps_addr);
  sem_up(sem1);
  sem_down(sem2);
  return NULL;
}
/***********************************************************************************/

void *thread2(__attribute__((unused)) void *arg)
{
  int offset = 0;
  char *buffer = malloc(TPS_SIZE);
  /* Create TPS and initialize with *msg1 */
  tps_create();
  //printf("buffer before: %s\n",buffer);
  tps_write(offset, TPS_SIZE, msg1);

  /* Read from TPS and make sure it contains the message */
  memset(buffer, 0, TPS_SIZE);
  tps_read(offset, TPS_SIZE, buffer);
  //printf("buffer after0: %s\n",buffer);
  assert(!memcmp(msg1, buffer, TPS_SIZE));
  printf("thread2: read OK!\n");

  /* Transfer CPU to thread 1 and get blocked */
  sem_up(sem1);
  sem_down(sem2);

  /* When we're back, read TPS and make sure it sill contains the original */
  memset(buffer, 0, TPS_SIZE);
  tps_read(offset, TPS_SIZE, buffer);
  //printf("buffer after1: %s\n",buffer);
  assert(!memcmp(msg1, buffer, TPS_SIZE));
  printf("thread2: read OK!\n");

  /* Transfer CPU to thread 1 and get blocked */
  sem_up(sem1);
  sem_down(sem2);

  /* Destroy TPS and quit */
  tps_destroy();
  free(buffer);
  return NULL;
}
/***********************************************************************************/

void *thread1(__attribute__((unused)) void *arg)
{
  pthread_t tid;
  int offset = 0;
  char *buffer = malloc(TPS_SIZE);
  /* Create thread 2 and get blocked */
  pthread_create(&tid, NULL, thread2, NULL);
  sem_down(sem1);
  /* When we're back, clone thread 2's TPS */
  tps_clone(tid);

  /* Read the TPS and make sure it contains the original */
  memset(buffer, 0, TPS_SIZE);
  tps_read(offset, TPS_SIZE, buffer);
  //printf("buffer after2: %s\n",buffer);
  assert(!memcmp(msg1, buffer, TPS_SIZE));
  printf("thread1: read OK!\n");

  /* Modify TPS to cause a copy on write */
  buffer[0] = 'h';
  tps_write(offset, 1, buffer);

  /* Transfer CPU to thread 2 and get blocked */
  sem_up(sem2);
  sem_down(sem1);

  /* When we're back, make sure our modification is still there */
  memset(buffer, 0, TPS_SIZE);
  tps_read(offset, TPS_SIZE, buffer);
  //printf("buffer after3: %s\n",buffer);
  assert(!strcmp(msg2, buffer));
  printf("thread1: read OK!\n");

  /* Transfer CPU to thread 2 */
  sem_up(sem2);

  /* Wait for thread2 to die, and quit */
  pthread_join(tid, NULL);
  tps_destroy();
  free(buffer);
  return NULL;
}
/***********************************************************************************/

void ReadAndWrite()
{
  pthread_t tid;
  pthread_create(&tid, NULL, thread1, NULL);
  pthread_join(tid, NULL);
}
/***********************************************************************************/

void *thread3()
{
  tps_create();

  //Try to create a second TPS under the same TID
  assert(tps_create() == -1);
  printf("Create TPS with same TID test passes\n");;
  return NULL;
}
/***********************************************************************************/

void CreateThreadWithSameTID()
{
  pthread_t tid;
  pthread_create(&tid, NULL, thread3, NULL);
  pthread_join(tid, NULL);
}
/***********************************************************************************/

void *thread4()
{
  tps_create();
  assert(tps_clone(pthread_self()) == -1);
  printf("Clone current thread test passes\n");
  tps_destroy();
  return NULL;
}
/***********************************************************************************/

void CloneCurrentThread()
{
  pthread_t tid;
  pthread_create(&tid, NULL, thread4, NULL);
  pthread_join(tid, NULL);
}
/***********************************************************************************/

void *thread5()
{
  char *buffer = malloc(TPS_SIZE);
  int offset = 5;
  size_t tpsSize = sizeof(msg3);
  /* Create TPS and initialize with *msg1 */
  tps_create();
  tps_write(offset, tpsSize, msg3);

  /* Read from TPS and make sure it contains the message */
  memset(buffer, '#', tpsSize);
  tps_read(0, TPS_SIZE, buffer);
  printf("buffer after: %s\n",buffer);
  assert(!memcmp(msg3 + offset, buffer, tpsSize));
  printf("Offset write test passed :) \n");
  tps_destroy();
  return NULL;
}
/***********************************************************************************/

void WriteWithOffset()
{
  pthread_t tid;
  pthread_create(&tid, NULL, thread5, NULL);
  pthread_join(tid, NULL);
}
/***********************************************************************************/

void *thread7(void* tid)
{
  char *buffer = malloc(TPS_SIZE);
  int offset = 0;
  pthread_t TID = (pthread_t)tid;
  size_t tpsSize = TPS_SIZE;
  tps_clone(TID);
  tps_read(offset, tpsSize, buffer);
  printf("buffer after read: %s\n",buffer);
  sem_up(sem1);
  sem_down(sem2);
  tps_destroy();
  return NULL;

}
/***********************************************************************************/

void *thread6()
{
  pthread_t tid;
  char *buffer = malloc(TPS_SIZE);
  int offset = 0;
  size_t tpsSize = TPS_SIZE;
  /* Create TPS and initialize with *msg1 */
  tps_create();
  tps_write(offset, tpsSize, msg4);
  memset(buffer, 0, tpsSize);
  pthread_create(&tid, NULL, thread7, (void*)pthread_self());
  sem_down(sem1);
  tps_read(offset, tpsSize, buffer);
  printf("buffer after: %s\n",buffer);
  assert(!memcmp(msg4, buffer, tpsSize));
  printf("Clone test passed :) \n");
  tps_destroy();
  return NULL;
}
/***********************************************************************************/

void CloneTest1()
{
  pthread_t tid;
  pthread_create(&tid, NULL, thread6, NULL);
  pthread_join(tid, NULL);
}
/***********************************************************************************/

void *thread9(void* tid)
{
  char *buffer = malloc(TPS_SIZE);
  int offset = 0;
  pthread_t TID = (pthread_t)tid;
  size_t tpsSize = TPS_SIZE;
  tps_clone(TID);
  tps_read(offset, tpsSize, buffer);
  printf("buffer in thread 9 that thread8 wrote: %s\n",buffer);

  //Thread9 writes msg5
  tps_write(offset, tpsSize, msg5);
  memset(buffer, 0, tpsSize);
  tps_read(offset, tpsSize, buffer);
  printf("buffer written by thread9: %s\n", buffer);
  sem_up(sem1);
  sem_down(sem2);
  tps_destroy();
  return NULL;

}
/***********************************************************************************/

void *thread8()
{
  pthread_t tid;
  char *buffer = malloc(TPS_SIZE);
  int offset = 0;
  size_t tpsSize = TPS_SIZE;

  // Thread8 writes msg4
  tps_create();
  tps_write(offset, tpsSize, msg4);
  memset(buffer, 0, tpsSize);
  pthread_create(&tid, NULL, thread9, (void*)pthread_self());
  sem_down(sem1);
  tps_read(offset, tpsSize, buffer);
  printf("buffer in thread8 after thread9: %s\n",buffer); // Race condition here
  assert(memcmp(msg5, buffer, tpsSize));
  printf("Clone test 2 passed :) \n");
  return NULL;
}
/***********************************************************************************/

void CloneTest2()
{
  pthread_t tid;
  pthread_create(&tid, NULL, thread8, NULL);
  pthread_join(tid, NULL);
}
/***********************************************************************************/

void *thread10()
{
  pthread_t tid;
  tid = 1101001101010;
  tps_create();
  assert(tps_clone(tid) == -1);
  printf("TID does not exist test passes\n");
  return NULL;
}
/***********************************************************************************/

void CloneNonexistentTID()
{
  pthread_t tid;
  pthread_create(&tid, NULL, thread10, NULL);
  pthread_join(tid, NULL);
}
/***********************************************************************************/

void *thread11()
{
  char *buffer = malloc(TPS_SIZE);
  int offset = 0;
  size_t tpsSize = TPS_SIZE;
  tps_create();
  tps_write(offset, tpsSize, msg4);
  memset(buffer, 0, tpsSize);
  tps_read(offset, tpsSize, buffer);
  int status = tps_destroy();
  assert(status == 0);
  printf("Destroy TPS passes\n");
  return NULL;
}
/***********************************************************************************/

void DestroyTPS()
{
  pthread_t tid;
  pthread_create(&tid, NULL, thread11, NULL);
  pthread_join(tid, NULL);
}
/***********************************************************************************/
void *thread12()
{
  int status = tps_destroy();
  assert(status == -1);
  printf("Destroy nonexist TPS test passes\n");
  return NULL;
}
/***********************************************************************************/

void DestroyNonexistent()
{
  pthread_t tid;
  pthread_create(&tid, NULL, thread12, NULL);
  pthread_join(tid, NULL);
}
/***********************************************************************************/

void *thread13()
{
  int offset = 0;
  char *buffer = malloc(TPS_SIZE);
  int status = tps_read(offset, TPS_SIZE, buffer);
  assert(status == -1);
  printf("Reading a thread with no TID test passed.\n");
  return NULL;
}
/***********************************************************************************/

void ReadThreadNoTPS()
{
  pthread_t tid;
  pthread_create(&tid, NULL, thread13, NULL);
  pthread_join(tid, NULL);
}
/***********************************************************************************/
void *thread14()
{
  int offset = 4090;
  char *buffer = malloc(TPS_SIZE);
  tps_create();
  int status;
  status = tps_write(offset,TPS_SIZE, "Don't print this");
  memset(buffer,0, TPS_SIZE);
  assert(status == -1);
  printf("Write off the block test passed.\n");
  tps_destroy();
  return NULL;
}
/***********************************************************************************/

void WriteOffBlock()
{
  pthread_t tid;
  pthread_create(&tid, NULL, thread14, NULL);
  pthread_join(tid, NULL);
}
/***********************************************************************************/
void *thread15()
{
  int offset = 0;
  char *buffer = NULL;
  tps_create();
  int status = tps_read(offset, TPS_SIZE, buffer);
  assert(status == -1);
  printf("No buffer test passed.\n");
  return NULL;
}
/***********************************************************************************/

void ReadNoBuffer()
{
  pthread_t tid;
  pthread_create(&tid, NULL, thread15, NULL);
  pthread_join(tid, NULL);
}
/***********************************************************************************/

void *thread16()
{
  char *buffer = malloc(TPS_SIZE);
  int offset = 8;
  size_t tpsSize = sizeof(msg3);
  /* Create TPS and initialize with *msg1 */
  tps_create();
  tps_write(0, TPS_SIZE, msg3);

  /* Read from TPS and make sure it contains the message */
  memset(buffer, '#', tpsSize);
  tps_read(offset, tpsSize, buffer);
  printf("buffer after: %s\n",buffer);
  assert(!memcmp(msg3 + offset, buffer, tpsSize));
  printf("Offset read test passed :) \n");
  tps_destroy();
  return NULL;
}
/***********************************************************************************/

void *thread17(){
  char *buffer = malloc(TPS_SIZE);
  tps_create();
  size_t size = sizeof(int1);
  tps_write(0, size, &int1);

  memset(buffer, 0, TPS_SIZE);
  tps_read(0. TPS_SIZE, buffer);
  printf("buffer after:%\n", buffer);
  assert(!memcmp(int1, buffer, 3));
  printf("Integer read and write test passed \n");
  tps_destroy;
  return NULL;

}
/***********************************************************************************/

void ReadWithOffset()
{
  pthread_t tid;
  pthread_create(&tid, NULL, thread16, NULL);
  pthread_join(tid, NULL);
}
/***********************************************************************************/

void IntegerReadAndWrite(){
  pthread_t tid;
  pthread_create(&tid, NULL, thread17, NULL);
  pthread_join(tid, NULL);
}
/***********************************************************************************/

int main(void)
{

  /* Create two semaphores for thread synchro */
  sem1 = sem_create(0);
  sem2 = sem_create(0);

  /* Init TPS API */
  tps_init(1);

  ReadAndWrite();
  CreateThreadWithSameTID();
  CloneCurrentThread();
  WriteWithOffset();
  CloneTest1();
  CloneTest2();
  CloneNonexistentTID();
  DestroyTPS();
  DestroyNonexistent();
  WriteOffBlock();
  ReadThreadNoTPS();
  ReadNoBuffer();
  ReadWithOffset();
  SegHandler();

  /* Destroy resources and quit */
  sem_destroy(sem1);
  sem_destroy(sem2);
  return 0;
}
/***********************************************************************************/