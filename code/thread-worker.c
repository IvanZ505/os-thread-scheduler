// File:	thread-worker.c

// List all group member's name:
// username of iLab:
// iLab Server:
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <ucontext.h>
#include <string.h>
#include <limits.h>
#include <stdatomic.h>
#include <valgrind/valgrind.h>


#include "thread-worker.h"

//Global counter for total context switches and 
//average turn around and response time
long tot_cntx_switches=0;
double avg_turn_time=0;
double avg_resp_time=0;


// INITAILIZE ALL YOUR OTHER VARIABLES HERE
// YOUR CODE HERE

// TODO: Check extern stuff later
Node* runq_last;
Node* runq_curr;

// Kinda jank solution but using it so keep conventions we already and to avoid having to rewrite alot of the code.
// Node** runq_last_mlfq[NUMPRIO];
Node* runq_last_mlfq[NUMPRIO];
Node* runq_curr_mlfq[NUMPRIO];
int quantum_counter = 0;

// Saved scheduler contexts
ucontext_t scheduler_ctx, main_ctx;

int scheduler_initialized = 0;
int tid_counter = 1;
int mutex_counter = 0;

struct sigaction sa;
struct itimerval timer;	

// Void ** value_ptr
void **saved_value_ptr;

// Current Thread runtime
struct timeval thread_starttime;
struct timeval thread_endtime;

// scheduler
static void schedule();
void printList(Node* last);

// @TODO: need to add freeing for the TCB blocks and the stacks inside.
// ---Circular Linked List---

// Inserts node to the rear of linked list
int queue(Node** last, Node* tcb_node) {
	if ((*last) == NULL) {
		*last = tcb_node;
		(*last)->next = *last;
		return 0;
	}

	tcb_node->next = (*last)->next;
	(*last)->next = tcb_node;
	(*last) = (*last)->next;

	// (*last) = (*last)->next;
	return 0;
}

// Removes requested node 
int dequeue(Node** last, Node* tcb_node, int freeing) {
    if (*last == NULL) {
        // List is empty, nothing to dequeue
        return -1; // Indicate failure
    }

	// printf("Dequeueing %d\n", tcb_node->block->thread_id);
	// printList(*last);
    Node *current = (*last)->next, *prev = NULL;

    // Case 1: If the node to be removed is the only node in the list
    if (current == tcb_node && current->next == current) {
		// Free TCB block
		if (freeing == 1 && current->block->stack != NULL) {
			free(current->block->stack);
			free(current->block->context);
			free(current->block);
		}
		if(freeing == 1) free(current);
        *last = NULL; // List is now empty
        return 0;     // Indicate success
    }

	// Case 2: There is only 2 nodes in the list
	if (current->next == *last) {
		if (current == tcb_node) {
			(*last)->next = current->next;
			if (freeing == 1 && current->block->stack != NULL) {
				free(current->block->stack);
				free(current->block->context);
				free(current->block);
			}
			if(freeing == 1) free(current);
			return 0; // Indicate success
		}
	}

    // Node to delete is first in queue
	if (current == tcb_node) {
		prev = *last;
	}

    // Traverse the list to find the node to delete
    while(current != *last && current != tcb_node) {
		prev = current;
		current = current->next;
	}

	if (current == tcb_node) {
		// printf("%d\n", prev->block->thread_id);
		// printf("%d\n", current->block->thread_id);
		prev->next = current->next;
		if (freeing == 1 && current->block->stack != NULL) {
			free(current->block->stack);
			free(current->block->context);
			free(current->block);
		}
		if(current == *last) {
			*last = prev;
		}
		if(freeing == 1) free(current);
		return 0; // Indicate success
	}

	// If we reach here, the node was not found in the list
	return -1; // Indicate failure
}

Node* copyNode(Node* source) {
	if (source == NULL) {
		return NULL;
	}

	Node* newNode = (Node*) malloc(sizeof(Node));
	newNode->block = source->block;
	newNode->next = NULL;

	return newNode;
}

void printList(Node* last) {
	if (last == NULL) {
		printf("List is empty\n");
		return;
	}

	Node *current = last->next;
	do {
		printf("%d ", current->block->thread_id);
		if(current->next != last->next) {
			printf("-> ");
		} else {
			printf("↩");
		}
		current = current->next;
	} while (current != last->next);
	printf("\n");
}

// Last thing to run for freeing!!
int freeList(Node** last) {
	if ((*last) == NULL) {
		return -1; // Indicate failure
	}

	Node *current = (*last)->next;
	while (current != (*last)) {
		if (current->block->stack != NULL) {
			free(current->block->stack);
			free(current->block->context);
			free(current->block);
		}
		free(current);
		current = current->next;
	}

	if ((*last)->block->stack) {
		free((*last)->block->stack);
		free((*last)->block->context);
		free((*last)->block);
	}
	free(*last);
	(*last) = NULL; // List is now empty
	return 0;     // Indicate success
}

/*
Pause and resume timers for the critical sections so that the program is not contexted switched whilst working
*/
int pause_timer() {
	struct itimerval current_timer;
	// Get the current timer value
	if (getitimer(ITIMER_REAL, &current_timer) == -1) {
        return -1;
    }

	timer = current_timer;

	struct itimerval zero_timer = { 0 };
    if(setitimer(ITIMER_REAL, &zero_timer, &timer) == -1) return -1;

	// printf("Timer paused with time: %ld\n", timer.it_value.tv_usec);
	return 0;
}

int resume_timer() {
    if (timer.it_value.tv_sec == 0 && timer.it_value.tv_usec == 0) {
		return -1;
	}

	if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        return -1;
    }

	// printf("Timer resumed with time: %ld\n", timer.it_value.tv_usec);
	return 0;
}

int reset_timer() {
	// printf("Resetting timer...\n");

    // Reset the timer back to 0 again, as if it's starting over
    timer.it_value.tv_sec = 0;        // initial delay again after reset
    timer.it_value.tv_usec = TIME_QUANTUM;
    timer.it_interval.tv_sec = 0;     // same interval after reset
    timer.it_interval.tv_usec = TIME_QUANTUM;
    setitimer(ITIMER_REAL, &timer, NULL);

	return 0;
}

/* Handles swapping contexts when a time quantum elapses */
void context_switch(int signum) {
	gettimeofday(&thread_endtime, NULL);
	runq_curr->block->total_runtime += (thread_endtime.tv_sec * 1000 + thread_endtime.tv_usec / 1000) - (thread_starttime.tv_sec * 1000 + thread_starttime.tv_usec / 1000);
	// Do we need to add this
	// getcontext(runq_curr->block->context);
	// printf("Timer interrupt switch...\n");
	// Time has elapsed, set elapsed to 1
	tot_cntx_switches++;
	runq_curr->block->elapsed = 1;
	
	swapcontext(runq_curr->block->context, &scheduler_ctx);
}

/* Initializes the library and timer */
void thread_init() {
	// Initialize the scheduler context
	if (getcontext(&scheduler_ctx) < 0){
		perror("getcontext");
		exit(1);
	}
	scheduler_ctx.uc_link = NULL;
	scheduler_ctx.uc_stack.ss_sp = malloc(STACK_SIZE);
	scheduler_ctx.uc_stack.ss_size = STACK_SIZE;
	scheduler_ctx.uc_stack.ss_flags = 0;
	makecontext(&scheduler_ctx, (void *)&schedule, 0);

	VALGRIND_STACK_REGISTER(scheduler_ctx.uc_stack.ss_sp, scheduler_ctx.uc_stack.ss_sp + STACK_SIZE);


	/* Initialize the caller context
	This assumes the caller of the first call to create_worker will be the only
	thread to call create_worker. If a create_worker thread tried to call create_worker,
	it would work in theory but the caller thread's state will not be able to be saved (I think, this shit is hard)
	*/
	if (getcontext(&main_ctx) == -1){
		perror("Getcontext failed");
		exit(1);
	}

	tcb *block = (tcb *)malloc(sizeof(tcb));

	block->context = &main_ctx;
	block->stack = NULL;
	block->status = Ready;
	block->priority = HIGH_PRIO;
	block->thread_id = 1;
	block->elapsed = 0;
	block->ran_first = 1;
	block->total_runtime = 0;
	gettimeofday(&block->start, NULL);
	block->function = NULL;
	block->yielded = 0;

	Node* tcb_block = (Node *)malloc(sizeof(Node));
	tcb_block->block = block;
	queue(&runq_last, tcb_block);
	runq_curr = tcb_block;

	/* For MLFQ runq_last points to current runq priority
	For PSJF, there is only 1 runq and by default set to max priority
	(Priority does not matter for PSJF, but this so keep code consistant
	between MLFQ and PSJF)*/  
	#ifdef MLFQ
		// for (int i = 0; i < NUMPRIO-1; i++) {
		// 	runq_last_mlfq[i] = (Node**)malloc(sizeof(Node*));
		// }
		// runq_last_mlfq[NUMPRIO-1] = &runq_last;
		runq_last_mlfq[NUMPRIO-1] = runq_last;
		runq_curr_mlfq[NUMPRIO-1] = runq_curr;
	#endif

	// Initialize timer

	// Copied from given sample, should change a little

	// Use sigaction to register signal handler
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &context_switch;
	sigaction(SIGALRM, &sa, NULL);
	// Create timer struct

	// Set up what the timer should reset to after the timer goes off
	timer.it_interval.tv_usec = TIME_QUANTUM; 
	timer.it_interval.tv_sec = 0;

	timer.it_value.tv_usec = TIME_QUANTUM;
	timer.it_value.tv_sec = 0;

	// Set the timer up (start the timer)
	setitimer(ITIMER_REAL, &timer, NULL);

	scheduler_initialized = 1;

	gettimeofday(&thread_starttime, NULL);

	
}

void worker_wrapper(void * arg){
	// printf("start of worker wrapper: id %d\n", runq_curr->block->thread_id);
	if (runq_curr && (runq_curr->block->status != Terminated)) {
		void* r = runq_curr->block->function(arg);
	}

	/* will be used for thread_join I think, scheduler should just
	skip any threads marked as terminated*/
	runq_curr->block->status = Terminated;

	// Goes back to scheduler
	context_switch(0);

	// Technically we clean up only after worker_join or worker_exit, but I am not too sure.
}

/* create a new thread */
int worker_create(worker_t* thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {
       // - create Thread Control Block (TCB)
       // - create and initialize the context of this worker thread
       // - allocate space of stack for this thread to run
       // after everything is set, push this thread into run queue and 
       // - make it ready for the execution.

       // YOUR CODE HERE	
	if(scheduler_initialized == 0) {
		thread_init();
	}
	
	tid_counter++;
	tcb *block = (tcb *)malloc(sizeof(tcb));
	ucontext_t *cctx = malloc(sizeof(ucontext_t));
	block->stack = malloc(STACK_SIZE);
	// REMEMBER TO REMOVE THIS LATER
	VALGRIND_STACK_REGISTER(block->stack, block->stack + STACK_SIZE);
	if (block->stack == NULL) {
    	perror("Failed to allocate stack");
    	return -1;
	}

	if(cctx == NULL){
    	perror("Failed to allocate context");
	}

	if (getcontext(cctx) == -1){
		perror("error fetching context");
		exit(1);
	}

	// Set up the context
	cctx->uc_stack.ss_sp = block->stack;
	cctx->uc_link=NULL;
	cctx->uc_stack.ss_size=STACK_SIZE;
	cctx->uc_stack.ss_flags=0;
	block->context = cctx;

	// Set up the TCB
	*thread = tid_counter;
	block->status = Ready;
	block->priority = HIGH_PRIO;
	block->elapsed = 0;
	block->ran_first = 0;
	block->total_runtime = 0;
	gettimeofday(&block->start, NULL);
	block->thread_id = *thread;
	block->function = function;
	block->yielded = 0;


	makecontext(cctx, (void *)&worker_wrapper, 1, arg);

	Node* tcb_block = (Node *)malloc(sizeof(Node));
	tcb_block->block = block;
	queue(&runq_last, tcb_block);

	// Used to test out LL, remember to delete or move else where
	// printList(runq_last);

	// if (tcb_block->block->thread_id == 5) {
	// 	dequeue(&runq_last, runq_last->next->next);
	// 	printf("\n");
	// 	printList(runq_last);
	// } else if (tcb_block->block->thread_id == 9) {
	// 	freeList(&runq_last);
	// }
    // return 0;
	return *thread;
};

#ifdef MLFQ
/* This function gets called only for MLFQ scheduling set the worker priority. */
// int worker_setschedprio(worker_t thread, int prio) {


//    // Set the priority value to your thread's TCB
//    // YOUR CODE HERE
// 	for (int i = 0; i < NUMPRIO; i++) {
// 		if ((*runq_last_mlfq[i]) != NULL) {
// 			Node* ptr = (*runq_last_mlfq[i])->next;
// 			while(ptr != (*runq_last_mlfq[i]) && ptr->block->thread_id != thread) {
// 				ptr = ptr->next;
// 			}
// 			if(ptr->block->thread_id == thread) {
// 				if (ptr->block->priority != prio) {
// 					Node* copy = copyNode(ptr);
// 					queue(runq_last_mlfq[prio], copy);
// 					dequeue(runq_last_mlfq[i], ptr, 0);
// 					ptr->block->priority = prio;
// 				}

// 				return 0;
// 			}
// 		}
// 	}
// 	return -1;
// }


int worker_setschedprio(worker_t thread, int prio) {


   // Set the priority value to your thread's TCB
   // YOUR CODE HERE
	for (int i = 0; i < NUMPRIO; i++) {
		if ((runq_last_mlfq[i]) != NULL) {
			Node* ptr = (runq_last_mlfq[i])->next;
			while(ptr != (runq_last_mlfq[i]) && ptr->block->thread_id != thread) {
				ptr = ptr->next;
			}
			if(ptr->block->thread_id == thread) {
				if (ptr->block->priority != prio) {
					Node* copy = copyNode(ptr);
					queue(&runq_last_mlfq[prio], copy);
					dequeue(&runq_last_mlfq[i], ptr, 0);
					ptr->block->priority = prio;
				}

				return 0;
			}
		}
	}
	return -1;
}

#endif



/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context

	// YOUR CODE HERE
	runq_curr->block->status = Ready;
	gettimeofday(&thread_endtime, NULL);
	runq_curr->block->total_runtime += (thread_endtime.tv_sec * 1000 + thread_endtime.tv_usec / 1000) - (thread_starttime.tv_sec * 1000 + thread_starttime.tv_usec / 1000);
	runq_curr->block->yielded = 1;
	// Save the context
	swapcontext(runq_curr->block->context, &scheduler_ctx);
	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	// - de-allocate any dynamic memory created when starting this thread
	// Swap context first? Before freeing???
	pause_timer();
	runq_curr->block->status = Terminated;
	
	// set value pointer... TF is this??? do i just set it to 1?
	if(value_ptr != NULL) {
		saved_value_ptr = &value_ptr;
	}

	struct timeval end;
	gettimeofday(&end, NULL);

	// Calculate turnaround time
	long int et = end.tv_sec * 1000 + end.tv_usec / 1000;
	long int st = runq_curr->block->start.tv_sec * 1000 + runq_curr->block->start.tv_usec / 1000;
	long int response_time = et - st;
	// printf("Current turnaround time: %ld\n", response_time);
	// Calculate my averages
	avg_turn_time = avg_turn_time + ((response_time - avg_turn_time) / (tid_counter-1));

	gettimeofday(&thread_endtime, NULL);
	runq_curr->block->total_runtime += (thread_endtime.tv_sec * 1000 + thread_endtime.tv_usec / 1000) - (thread_starttime.tv_sec * 1000 + thread_starttime.tv_usec / 1000);
	resume_timer();
	swapcontext(runq_curr->block->context, &scheduler_ctx);
}


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
  
	// YOUR CODE HERE
	Node* curr = runq_last->next;

	// Traverse the run queue to find the thread with the matching thread_id
	while (curr != runq_last && curr->block->thread_id != thread) {
		curr = curr->next;
	}

	if (curr->block->thread_id != thread) {
		// printList(runq_last);
		// printf("thread %d not found\n", thread);
		return -1; // Thread not found
	}
	
	// printf("Thread ID: %d\n", curr->block->thread_id);

	// Wait until the thread terminates
	while (curr->block->status != Terminated) {
		// printf("Waiting for: %d\n", curr->block->thread_id);
		runq_curr->block->status = Yielding;
		worker_yield(); // Yield CPU while waiting
	}

	// Deallocate the thread's resources
	// free(curr->block->stack);
	// free(curr->block->context);
	// free(curr->block);

	// Set the value pointer
	if (value_ptr != NULL) {
		*value_ptr = *saved_value_ptr;
	}

	gettimeofday(&thread_endtime, NULL);
	runq_curr->block->total_runtime += (thread_endtime.tv_sec * 1000 + thread_endtime.tv_usec / 1000) - (thread_starttime.tv_sec * 1000 + thread_starttime.tv_usec / 1000);
	// Remove from the run queue
	dequeue(&runq_last, curr, 1);
	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//- initialize data structures for this mutex

	// YOUR CODE HERE
	if (scheduler_initialized != 1){
		thread_init();
	}

	if (!mutex) return -1;

	pause_timer();
	atomic_store(&(mutex->locked), 0);
	mutex->owner = 0;
	mutex->queue = malloc(sizeof(Node *));
	*(mutex->queue) = NULL;
	mutex->id = ++mutex_counter;
	// printf("Mutex %d initialized\n", mutex->id);
	resume_timer();
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

        // YOUR CODE HERE
		// printf("Inside mutex lock with lock: %d\n", mutex->id);
		if (!mutex || mutex->id == 0) return -1;

		worker_t thread_id = runq_curr->block->thread_id;

		pause_timer();
		while (atomic_exchange(&(mutex->locked), 1) == 1) {
			if (mutex->owner == thread_id) {
				resume_timer();
           		return -1;  // Deadlock
        	}
			runq_curr->block->status = Blocked;

			// Copy the runq_curr to the mutex queue
			Node* new_node = (Node *)malloc(sizeof(Node));
			new_node->block = runq_curr->block;
			// dequeue(&runq_last, runq_curr, 0);  // Remove the thread from the run queue, but do not deallocate the resources
        	queue(mutex->queue, new_node);

			// printf("Thread %d is blocked by mutex %d\n", thread_id, mutex->id);
			gettimeofday(&thread_endtime, NULL);
			runq_curr->block->total_runtime += (thread_endtime.tv_sec * 1000 + thread_endtime.tv_usec / 1000) - (thread_starttime.tv_sec * 1000 + thread_starttime.tv_usec / 1000);
			resume_timer();
			swapcontext(runq_curr->block->context, &scheduler_ctx);
		}

		 // Successfully acquired lcok
		// printf("Thread %d acquired mutex %d\n", thread_id, mutex->id);
    	mutex->owner = thread_id;
		resume_timer();
        return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	// Check if mutex is initialized
    if (!mutex || mutex->id == 0) return -1;

    // Only the owner can unlock
    if (mutex->owner != runq_curr->block->thread_id) return -1;

	pause_timer();
    // Release the lock
	// printf("Thread %d released mutex %d\n", runq_curr->block->thread_id, mutex->id);

	atomic_store(&(mutex->locked), 0);
    mutex->owner = 0;

	// Check if there are any threads waiting in the mutex's queue
    if (mutex->queue == NULL || *(mutex->queue) == NULL) {
        resume_timer();
        return 0;  // No threads waiting, nothing to do
    }
    // If threads are waiting, move them back to the run queue
    Node* waiting_thread = *(mutex->queue);
	
	waiting_thread = waiting_thread->next;
	worker_t thread = waiting_thread->block->thread_id;
	// printf("next thread: %d, %d\n", waiting_thread->block->thread_id, thread);
    if (waiting_thread != NULL) {
        // Dequeue the first thread from the mutex queue, do not deallocate
        dequeue(mutex->queue, waiting_thread, 0);
		free(waiting_thread);
        // Change runq status to Ready
		Node* ptr = runq_last->next;
		while(ptr != runq_last && ptr->block->thread_id != thread) {
			ptr = ptr->next;
		}
		if(ptr->block->thread_id == thread) {
			// printf("Thread %d is ready\n", thread);
			ptr->block->status = Ready;
			resume_timer();
			return 0;	// Successfully set
		}
    }
	resume_timer();
	return -1;
}


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	// - de-allocate dynamic memory created in worker_mutex_init
	if(mutex->queue != NULL) freeList(mutex->queue);
	free(mutex->queue);

	mutex->id = 0;

	return 0;
};

/* scheduler */
static void sched_psjf();
static void sched_mlfq();

static void schedule() {
	// - every time a timer interrupt occurs, your worker thread library 
	// should be contexted switched from a thread context to this 
	// schedule() function

	// - invoke scheduling algorithms according to the policy (PSJF or MLFQ)

	// if (sched == PSJF)
	//		sched_psjf();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	// YOUR CODE HERE

	while (1) {

		// Temporary round robin scheduler

		// printList(runq_last);
		// runq_curr = runq_curr->next;
		// // printf("Switched to thread: %d with status %d\n", runq_curr->block->thread_id, runq_curr->block->status);
		// if(!(runq_curr->block->status == Terminated) && !(runq_curr->block->status == Blocked)) {
		// 	runq_curr->block->status = Running;
		// 	// printList(runq_last);
		// 	swapcontext(&scheduler_ctx, runq_curr->block->context);
		// }
		
// - schedule policy
#ifndef MLFQ
	// Choose PSJF
	sched_psjf();
#else 
	// Choose MLFQ
	sched_mlfq();
#endif
	}
	// freeList(&runq_last);
}

/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf() {
	// - your own implementation of PSJF
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
	// Being brought back here because its yieldinggggg, handle the yielding!!!
	pause_timer();
	if(runq_curr->block->elapsed == 1 || runq_curr->block->status == Yielding) {
		runq_curr->block->elapsed = 0;

		// Move to the end of the queue.
		// printf("Thread %d has elapsed\n", runq_curr->block->thread_id);
		// printf("Moving thread %d to the end of the queue\n", runq_curr->block->thread_id);
		// printList(runq_last);
		Node* prev = runq_curr;
		while(prev->next != runq_curr) {
			prev = prev->next;
		}
		// If the thread has not finished, move it to the end of the queue
		if(runq_curr != runq_last && runq_curr->next != runq_last) {
			runq_curr->block->status = Ready;
			prev->next = runq_curr->next;
			runq_curr->next = runq_last->next;
			runq_last->next = runq_curr;
			runq_last = runq_curr;
			runq_curr = runq_last->next;
		}
		else {
			runq_last = runq_curr;
			runq_curr = runq_curr->next;
		}
		// printf("Moved thread %d to the end of the queue new Q \n", runq_curr->block->thread_id);
		// printList(runq_last);
	}

	// Run through loop and find the shortest runtime thread whose status is not Terminated or Blocked
	Node* ptr = runq_curr;
	long int shortest = LONG_MAX;
	Node* shortest_thread;

	if(ptr->block->total_runtime < shortest && ptr->block->status != Terminated && ptr->block->status != Blocked) {
		shortest = ptr->block->total_runtime;
		shortest_thread = ptr;
	}

	while(ptr->next != runq_curr) {
		if((ptr->block->total_runtime < shortest) && (ptr->block->status != Terminated) && (ptr->block->status != Blocked)) {
			shortest = ptr->block->total_runtime;
			shortest_thread = ptr;
		}
		ptr = ptr->next;
	}
	
	// Check if the last node is the shortest
	if(runq_curr->block->total_runtime < shortest && runq_curr->block->status != Terminated && runq_curr->block->status != Blocked) {
		shortest = runq_curr->block->total_runtime;
		shortest_thread = ptr;
	}

	// printf("The shortest thread is %d with runtime %ld with status: %d\n", shortest_thread->block->thread_id, shortest_thread->block->total_runtime, shortest_thread->block->status);
	
	if(shortest_thread->block->status == Terminated || shortest_thread->block->status == Blocked) {
		// printf("Thread %d is terminated or blocked\n", shortest_thread->block->thread_id);
		runq_curr = runq_curr->next;
	} else {
		runq_curr = shortest_thread;
	}
	// printf("Switched to thread: %d with status %d\n", runq_curr->block->thread_id, runq_curr->block->status);

	if(!(runq_curr->block->status == Terminated) && !(runq_curr->block->status == Blocked)) {
		runq_curr->block->status = Running;
		// printList(runq_last);
		// reset_timer();
		// printList(runq_last);

		if(runq_curr->block->ran_first == 0) {
			runq_curr->block->ran_first = 1;
			struct timeval enda;
			gettimeofday(&enda, NULL);

			// Calculate response time
			long int et = enda.tv_sec * 1000 + enda.tv_usec / 1000;
			long int st = runq_curr->block->start.tv_sec * 1000 + runq_curr->block->start.tv_usec / 1000;
			long int response_time = et - st;
			// printf("Current response time is %ld - %ld = %ld\n",et, st, response_time);

			// Calculate my averages
			if(tid_counter == 1) {
				avg_resp_time = response_time;
			} else if(tid_counter == 2) {
				avg_resp_time = (avg_resp_time + response_time) / 2;
			} else {
				avg_resp_time = ((avg_resp_time * (tid_counter - 1)) + response_time) / tid_counter;
			}
		}
		gettimeofday(&thread_starttime, NULL);
		resume_timer();

		swapcontext(&scheduler_ctx, runq_curr->block->context);
	}
}

int time_diff(struct timeval* start) {
	struct timeval currTime;

    gettimeofday(&currTime, NULL);

    long sec = currTime.tv_sec - start->tv_sec;
    long usec = currTime.tv_usec - start->tv_usec;

    if (usec < 0) {
        sec--;
        usec += 1000000;
    }
	return sec * 1000000 + usec;
}

int refresh_queue(Node** src, Node** dest) {
	while (*src != NULL) {
		Node* copy = copyNode(*src);
		copy->block->priority = HIGH_PRIO;
		dequeue(src, *src, 0);
		queue(dest, copy);
	}
	*src = NULL;
	printf("im tired\n");
	printList(*src);
	printList(*dest);
	printf("\n");

	return 0;
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	if (tot_cntx_switches % REFRESH_QUANTUM != 0) {
		struct timeval currTime;
		int currPrio = runq_curr->block->priority;

		// Important as our queue functions alter the runq_last input
		runq_last_mlfq[currPrio] = runq_last;
		// runq_curr_mlfq[currPrio] = runq_curr;

		for (int i = 0; i < NUMPRIO; i++) {
			printf("prio = %d: ", i);
			printList(runq_last_mlfq[i]);
		}
		if (runq_curr->block->yielded == 1) {
			printf("yielded!!");
			// Yielded, we have to check if it has ran more than the time quantum
			runq_curr->block->total_runtime += time_diff(&(runq_curr->block->runtime));
			if (runq_curr->block->total_runtime >= TIME_QUANTUM && currPrio > LOW_PRIO) {
				runq_curr->block->total_runtime = 0;
				worker_setschedprio(runq_curr->block->thread_id, currPrio-1);
			}
		} else {
			// Used full time quantum
			if (currPrio > LOW_PRIO) {	  
				printf("setting prio at thread %d to %d\n", runq_curr->block->thread_id, currPrio-1);

				worker_setschedprio(runq_curr->block->thread_id, currPrio-1);

				// Sets new current thread for current priority runq
				if (!runq_curr_mlfq[currPrio] || runq_curr_mlfq[currPrio] == runq_curr_mlfq[currPrio]->next) {
					runq_curr_mlfq[currPrio] = runq_last_mlfq[currPrio];
				}
			}
		}

		// Selects thread and maintains RR among the queue lines
		Node* ptr;
		Node* last_node;
		for (int i = NUMPRIO-1; i >= 0; i--) {
			if (runq_last_mlfq[i]) {
				last_node = ptr;
				if (runq_curr_mlfq[i]) {
					ptr = runq_curr_mlfq[i]->next;
				} else {
					ptr = runq_last_mlfq[i]->next;				
				}
				do {
					if(!(ptr->block->status == Terminated) && !(ptr->block->status == Blocked)) {
						runq_curr_mlfq[i] = ptr;
						runq_curr = ptr;
						runq_last = runq_last_mlfq[i];

						runq_curr->block->status = Running;
						runq_curr->block->yielded = 0;
						// Kinda jank, I should find better solution
						i = 0;
						break;
					}
					ptr = ptr->next;				
				} while (ptr != last_node);
			}
		}
	} else {
		// Refreshes queue by moving all the jobs back to the top queue every REFRESH_QUANTUM times the TIME_QUANTUM
		printf("Its time to refresh\n");
		for (int i = NUMPRIO-2; i >= 0; i--) {
			if(runq_last_mlfq[i]) {
				printf("\nrefreshing prio %d\n", i);
				refresh_queue(&(runq_last_mlfq[i]), &runq_last_mlfq[NUMPRIO-1]);
				runq_curr_mlfq[i] = NULL;
				runq_last_mlfq[i] = NULL;
			}
		}

		// Sets new runq_curr and runq_last
		if (runq_curr_mlfq[NUMPRIO-1]){
			runq_curr = runq_curr_mlfq[NUMPRIO-1];
		} else {
			runq_curr = runq_last_mlfq[NUMPRIO-1]->next;
		}
		runq_last = runq_last_mlfq[NUMPRIO-1];
	}
	printf("running thread %d\n", runq_curr->block->thread_id);
	gettimeofday(&(runq_curr->block->runtime), NULL);
	swapcontext(&scheduler_ctx, runq_curr->block->context);
}

//DO NOT MODIFY THIS FUNCTION
/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void) {

       fprintf(stderr, "Total context switches %ld \n", tot_cntx_switches);
       fprintf(stderr, "Average turnaround time %lf \n", avg_turn_time);
       fprintf(stderr, "Average response time  %lf \n", avg_resp_time);
}


// Feel free to add any other functions you need

// YOUR CODE HERE

