#include <stddef.h>
#include <stdlib.h>

#include "queue.h"
#include "sem.h"
#include "thread.h"
/***********************************************************/

struct semaphore {
	size_t count;
	queue_t threads;
};
/***********************************************************/

sem_t sem_create(size_t count) {
	sem_t newSem = malloc(sizeof(sem_t));
	if (!newSem) {
		return NULL;
	}
	newSem->count = count;
	newSem->threads = malloc(sizeof(queue_t));
	return newSem;
}
/***********************************************************/

int sem_destroy(sem_t sem) {
    if (sem == NULL || queue_length(sem->threads) > 0) {
      return -1;
    }
	free(sem);
	return 0;
}
/***********************************************************/

int sem_down(sem_t sem) {
      /* Return -1 if sem is NULL */
      if (sem == NULL) {
        return -1;
      }
      enter_critical_section();
      if (sem->count == 0) {
        /* Add the current thread to the blocked queue */
        queue_enqueue(sem->threads, (void*)pthread_self());
        /* The called thread gets blocked until semaphore */
        /* becomes available */
        thread_block();
      }
      sem->count -= 1;
      exit_critical_section();
      return 0;
}
/***********************************************************/

int sem_up(sem_t sem) {
    if (sem == NULL) {
      return -1;
    }
    enter_critical_section();
    if (queue_length(sem->threads) > 0)
    {
      pthread_t *currThread = malloc(sizeof(pthread_t));
      /* Taking thread from the sem queue */
      queue_dequeue(sem->threads, (void**)&currThread);
      thread_unblock((pthread_t)currThread);
    }
    sem->count++;
    exit_critical_section();
    return 0;
}
/***********************************************************/

int sem_getvalue(sem_t sem, int *sval) {
    if (sem == NULL || sval == NULL) {
      return -1;
    }
    if(sem->count > 0) {
      *sval = sem->count;
    } else if (sem->count == 0) {
      *sval = -1 * queue_length(sem->threads);
    }
	return 0;
}
/***********************************************************/
