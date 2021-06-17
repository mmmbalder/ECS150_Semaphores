#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "queue.h"
#include "thread.h"
#include "tps.h"

queue_t tps_queue;
typedef struct TPS *tps_t;
typedef struct PAGE *page_t;
/**************************************************************************/

struct TPS {
  page_t page;
  pthread_t tid;
};
/**************************************************************************/

struct PAGE {
  void* mem_page;
  int ref_counter;
};
/**************************************************************************/

static int find_tid(void* data, void* arg)
{
  tps_t currTPS = (tps_t)data;
  if (currTPS->tid == (pthread_t)arg)
    return 1;
  return 0;
}
/**************************************************************************/

static int find_page(void* data, void* arg) {
  tps_t currTPS = (tps_t)data;
  if (currTPS->page->mem_page == arg)
    return 1;
  return 0;
}
/**************************************************************************/

static void segv_handler(int sig, siginfo_t *si,__attribute__((unused)) void *context)
{
  tps_t found = NULL;
  void *p_fault = (void*)((uintptr_t)si->si_addr & ~(TPS_SIZE - 1));
  /* Iterate through all the TPS areas and find if p_fault matches one of them */
  queue_iterate(tps_queue,find_page,p_fault,(void**)&found);
  if (found) {
    /* Printf the following error message */
    fprintf(stderr, "TPS protection error!\n");
  }
  /* In any case, restore the default signal handlers */
  signal(SIGSEGV, SIG_DFL);
  signal(SIGBUS, SIG_DFL);
  /* And transmit the signal again in order to cause the program to crash */
  raise(sig);
}
/**************************************************************************/

int tps_init(int segv)
{
  tps_queue = queue_create();
  if (!tps_queue) {
    return -1;
  }
  if (segv) {
    /*the TPS API should install a
     * page fault handler that is able to recognize TPS protection errors */
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = segv_handler;
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGSEGV, &sa, NULL);
  }
  return 0;
}
/**************************************************************************/

int tps_create(void)
{
  tps_t found = NULL;
  /* Iterate to the queue to find the current's thread tid and set it to */
  /* equal to the found tps we just created */
  queue_iterate(tps_queue,find_tid,(void*)pthread_self(),(void**)&found);
  /* If the current thread's tid is already in the queue, don't create */
  /* a new tps with that id and don't add it to the queue */
  if (found) {
    return -1;
  }
  /* Create memory for new tps */
  tps_t new_tps = malloc(sizeof(tps_t));
  if (!new_tps) {
    return -1;
  }
  /* Make the new tps' tid equal to the current thread's tid */
  new_tps->tid = pthread_self();
  /* Allocate memory for a new page */
  page_t new_page = malloc(sizeof(new_page));
  /* Check if allocation of memory fails */
  if (!new_page){
    return -1;
  }
  /* Allocate memory for the new page's memory page using mmap. */
  /* Mmap returns a void pointer */
  new_page->mem_page = mmap(NULL, TPS_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1,0);
  /* Check to see if allocation of memory fails */
  if (!new_page->mem_page){
    return -1;
  }
  /* Set ref_counter to 1 */
  new_page->ref_counter = 1;
  /* Set the new tps' page equal to the new page */
  new_tps->page = new_page;
  /* Add the new tps to the tps queue */
  queue_enqueue(tps_queue, (void*)new_tps);
  return 0;
}
/**************************************************************************/

int tps_destroy(void)
{
  tps_t found = NULL;
  /* Iterate to the queue to find the current's thread tid and set it to */
  /* equal to the found tps we just created */
  queue_iterate(tps_queue,find_tid,(void*)pthread_self(),(void**)&found);
  /* Check to see if found */
  if (!found) {
    return -1;
  }
  /* Delete the found tps from the tps queue */
  queue_delete(tps_queue, (void*)found);
  /* Unmap or free the tps' memory page. TPS_SIZE is how much we delete */
  munmap(found->page->mem_page, TPS_SIZE);
  /* free page because otherwise page would still exist after */
  /* we free found */
  free(found->page);
  /* Decrement the page's reference counter */
  found->page->ref_counter -= 1;
  /* Destroy the tps area associated to the current thread */
  free(found);
  found = NULL;
  /* Check to see if found tps was actually freed */
  if (found){
    return -1;
  }
  return 0;
}
/**************************************************************************/

int tps_read(size_t offset, size_t length, void *buffer)
{
  tps_t found = NULL;
  /* Iterate through the tps queue to find tps with tid of current thread */
  /* and make found equal to that tps */
  queue_iterate(tps_queue,find_tid,(void*)pthread_self(),(void**)&found);
  /* Check to see if current thread doesn't have a tps or if the reading */
  /* operation is out of bounds or if the buffer is null */
  if (!found || length + offset > TPS_SIZE || !buffer) {
    return -1;
  }
  enter_critical_section();
  /* Change protection to read */
  mprotect(found->page->mem_page, TPS_SIZE,PROT_READ);
  /* Copy the found tps' memory page plus offset to the buffer */
  memcpy(buffer, found->page->mem_page + offset, length + offset);
  /* Change protection back to none */
  mprotect(found->page->mem_page, TPS_SIZE,PROT_NONE);
  exit_critical_section();
  return 0;
}
/**************************************************************************/

int tps_write(size_t offset, size_t length, void *buffer)
{
  tps_t found = NULL;
  queue_iterate(tps_queue,find_tid,(void*)pthread_self(),(void**)&found);
  /* Check to see if current thread doesn't have a tps or if the reading */
  /* operation is out of bounds or if the buffer is null */
  if (!found || length + offset > TPS_SIZE || !buffer) {
    return -1;
  }
  enter_critical_section();
  if(found->page->ref_counter > 1) {
    /* Allocate memory for a new page */
    page_t new_page = malloc(sizeof(new_page));
    /* Check to see if it fails to allocate memory for new page */
    if (!new_page){
      return -1;
    }
    /* Allocate memory for the new page's memory page using mmap */
    /* Mmap returns a void pointer */
    new_page->mem_page = mmap(NULL, TPS_SIZE, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1,0);
    /* Change protection to be able to read */
    mprotect(found->page->mem_page, TPS_SIZE,PROT_READ);
    /* Copy memory page of found tps to the new page's memory page */
    memcpy(new_page->mem_page + offset,found->page->mem_page, TPS_SIZE);
    /* Change protection back to none */
    mprotect(found->page->mem_page, TPS_SIZE,PROT_NONE);
    mprotect(new_page->mem_page, TPS_SIZE,PROT_NONE);
    /* Make found tps' page equal to the new page and subtract to ref count */
    found->page = new_page;
    new_page->ref_counter -= 1;
  }
  /* Change protection to write */
  mprotect(found->page->mem_page, TPS_SIZE, PROT_WRITE);
  /* Write from buffer to the found tps' memory page */
  memcpy(found->page->mem_page + offset, buffer + offset, length);
  /* Change protection back to none */
  mprotect(found->page->mem_page, TPS_SIZE, PROT_NONE);
  exit_critical_section();
  return 0;
}
/**************************************************************************/

int tps_clone(pthread_t tid)
{
  tps_t found = NULL;
  /* Check to see if the tid given is in the queue */
  queue_iterate(tps_queue,find_tid,(void*)pthread_self(),(void**)&found);
  /* If it is in the queue, then don't clone */
  if (found) {
    return -1;
  }
  tps_t clone_found = NULL;
  queue_iterate(tps_queue,find_tid,(void*)tid,(void**)&clone_found);
  /* Allocate memory for the clone */
  tps_t clone = malloc(sizeof(tps_t));
  /* Check to see if memory allocation for new clone worked */
  if(!clone){
    return -1;
  }
  enter_critical_section();
  /* Set the clone's tid to be the current thread's tid */
  clone->tid = pthread_self();
  /* Make the clone's page equal to the found's page */
  clone->page = clone_found->page;

  /* Increment the reference counter */
  clone->page->ref_counter += 1;
  exit_critical_section();
  /* Enqueue the clone into the tps queue */
  queue_enqueue(tps_queue, (void*)clone);
  return 0;
}
/**************************************************************************/
