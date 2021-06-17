## REPORT.md

## Semaphores

# Implementation
For our semaphore structure, we added a count variable for number of resources as well as a queue of threads.

In sem_create(), we malloc the new semaphore and the new threads in that semaphore.

We initially had a lock variable but found it was not needed.

In sem_create(), we allocate memory for both a new semaphore and the queue of threads.

For sem_down, and sem_up, we enter the critical section before our if statements because we don't want any interrupts within those function that can potentially mess up are results. We use exit_critical_section() in front of every return statements so we don't have a case where we leave the function and still be in the critical section.

# Debugging
Initially we had a bug where sem_count stops at a random number between 0 and 19, which was a result of a deadlock. The line where we increment the count was inside an if statement, so we had to drag it out then it fixed all three test cases at once.

## TPS

# Implementation
We created typedef structures for PAGE and TPS since we knew we would be using these a lot and wanted to avoid unnecessary long code.

We created a find_tid function in order to call it when we use the queue iterate function so that we can find the TID need.

In the segv_handler function, we add an attribute unused for the context variable because it is unused. We found that we needed to compate p_fault to each page memory of each TPS so we needed a second helper function to compare pages rather than the TID.

In our tps_create() function, we call the queue_iterate to iterate through the queue given in order to find the current thread's tid in the queue. And we set the tid to the found variable and added a check so that we returned -1 if the tid already existed in the queue. Next, we malloc both a new TPS and the new page of the TPS. We set the tid to that of the current thread, which we get by calling pthread_self(). We utilize mmap since it let's us set the protection types to add MAP_PRIVATE because it makes the updates to the mapping invisible to other processes that are mapping the same region. We set the reference counter of the page set to 1. This counter is used to calculate how many TPS' are pointed to a certain page.

For our tps_destroy(), we call queue_iterate to find the current thread's TID to delete it. Then we employ  queue_delete to delete that TPS. In addition, we use munmap to free that TPS' memory page. It is necessary to free the TPS' memory page because otherwise, if we just freed the TPS, its memory page would still exist. Then we free the TPS' memory. Also, we decrement the reference count since we got rid of the TPS' page.


For our tps_read(), we enter critical section in order to memcpy the Copy the found tps' memory page plus offset to the buffer. We also add protections to PROT_READ before we memcpy and change them back to PROT_NONE after we memcpy. Finally, we exit critical section.

In our tps_write() function has an if statement to check if the reference count is more than 1 so that we can allocate memory for a new page and memory page. We change protections to PROT_READ before we memcpy from the  memory page of found tps to the new page's memory page plus the offset. Then we return the protection to PROT_NONE. Finally, we keep count of the reference count accordingly when we make the TPS' page equal to the new page. If the reference counter is not higher than 1, we simply change the protections and memcpy as aforementioned without creating memory for the new page or memory page. Additionally, we continue to enter and exit critical section when memcopying and changing protections.

In our tps_clone() function, we check to if the TPS' TID is in the queue and if it isn't we create two TPS' called clone and clone_found and we make the clone's tid equal to the current thread's TID. Further, we set the clone's page equal to the clone found's page. Finally we enqueue the clone into the tps queue.


## Testing TPS

For testing, we either had to see if it failed due to the tester or because of tps.c. We used the debugger from csif and added breaks each function of our tps. Then we ran the tps simple test function in order to see where the mistakes happened. At first we used a single test file, but during the course of making the test function, we spotted a race condition inside one of the clone testers. The race condition was found to be from previous tests so we fixed it by destroying the TPS's inside the previous tests after the assert/pass notification.

For the function find_tid, we corrected data and arg's typecast because it wasn't finding the tid.

For the tps_create, we made the new TPS' TID equal to the current thread's TID because it wasn't creating the tps. Additionally, we were getting a segmentation fault and we resolved the issue by adding the MAP_PRIVATE flag to mmap in order to allocate the new page's memory page because when used with MAP_ANONYMOUS, MAP_PRIVATE is useful so that in allocating memory because it doesn't allow the region to be shared by forked processes. Furthermore, we used to have the whole function in the critical section, but that led to issues with accessing the address.For out TPS test file, we first tried to create a thread with a preexisting TID. We tested this by calling tps_create() twice, since the second time will be under the same TID. When we call it the second time, we check it by seeing if it returns -1.

When testing destroy, we have two functions: One that writes, reads, then destroy, making sure tps_destroy succeeds and the other one that just calls tps_destroy(), which shouldn't destroy since the TPS doesn't exist. The first function exposed a bug in tps_destroy() where the block wasn't actually destroyed. This was a result of a dangling pointer that should be set to NULL after the block is freed.

For testing the offset, we found that an offset of 0 worked, but it doesn't work on any other offset. We checked the tester to see if anything there is wrong. At first we thought out tester was off, but adjusted the offset in the tps_simple and found the bug was in the program.

For tps_clone(), we started out with the simple test where we tried to clone the current TID, which worked really fast. When we got to the complex example in the Project3 discussion slide 10, we have thread A write something then cloned it then thread B reads it. Thread B read what thread A wrote just fine, but thread A, being the one that *originally* wrote it, read it as a fancy character. Later realised I forgot to add read inside thread A.

## Sources
Office hours for concepts
Tutors Elias, Eric, and Sean in Kemper 75 helped explained concepts and some debugging.
