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


#include "thread-worker.h"

#define TIME_QUANTUM 1

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

ucontext_t scheduler_ctx, main_ctx;

int scheduler_initialized = 0;

// scheduler
static void schedule();

// @TODO: need to add freeing for the TCB blocks and the stacks inside.
// ---Circular Linked List---

// Inserts node to the front of linked list
int queue(Node** last, Node* tcb_node) {
	if ((*last) == NULL) {
		*last = tcb_node;
		(*last)->next = *last;
		return 0;
	}

	tcb_node->next = (*last)->next;
	(*last)->next = tcb_node;

	// (*last) = (*last)->next;
	return 0;
}


int dequeue(Node** last, Node* tcb_node) {
    if (last == NULL) {
        // List is empty, nothing to dequeue
        return -1; // Indicate failure
    }

    Node *current = (*last)->next, *prev = NULL;

    // Case 1: If the node to be removed is the only node in the list
    if (current == tcb_node && current->next == current) {
		// Free TCB block
		free(current->block->stack);
		free(current->block);
		free(current);
        *last = NULL; // List is now empty
        return 0;     // Indicate success
    }

    // Traverse the list to find the node to delete
    while(current != *last && current != tcb_node) {
		prev = current;
		current = current->next;
	}

	if (current == tcb_node) {
		prev->next = current->next;
		free(current->block->stack);
		free(current->block);
		free(current);
		return 0; // Indicate success
	}

	// If we reach here, the node was not found in the list
	return -1; // Indicate failure
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
		free(current->block->stack);
		free(current->block);
		free(current);
		current = current->next;
	}
	free((*last)->block->stack);
	free((*last)->block);
	free(*last);
	(*last) = NULL; // List is now empty
	return 0;     // Indicate success
}

/* Handles swapping contexts when a time quantum elapses */
void context_switch(int signum) {
	printf("ALARM\n");
	printf("ALARM\n");
	printf("ALARM\n");
	printf("ALARM\n");
	printf("ALARM\n");
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

	// Initialize timer

	// Copied from given sample, should change a little

	// Use sigaction to register signal handler
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &context_switch;
	sigaction (SIGPROF, &sa, NULL);
	// Create timer struct
	struct itimerval timer;

	// Set up what the timer should reset to after the timer goes off
	timer.it_interval.tv_usec = 0; 
	timer.it_interval.tv_sec = TIME_QUANTUM;

	timer.it_value.tv_usec = 0;
	timer.it_value.tv_sec = TIME_QUANTUM;

	// Set the timer up (start the timer)
	setitimer(ITIMER_PROF, &timer, NULL);

	scheduler_initialized = 1;
}

void worker_wrapper(void * arg){
	printf("did we get this far?: %d\n", runq_curr->block->thread_id);
	if (runq_curr) {
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
	
	tcb *block = (tcb *)malloc(sizeof(tcb));
	ucontext_t cctx;
	block->stack = malloc(STACK_SIZE);
	if (block->stack == NULL) {
    	perror("Failed to allocate stack");
    	return -1;
	}

	if (getcontext(&cctx) < 0){
		perror("getcontext");
		exit(1);
	}

	cctx.uc_stack.ss_sp = block->stack;
	cctx.uc_link=&scheduler_ctx;
	cctx.uc_stack.ss_size=STACK_SIZE;
	cctx.uc_stack.ss_flags=0;
	block->context = &cctx;

	block->status = Ready;
	block->priority = 4;
	block->thread_id = *thread;
	block->function = function;

	makecontext(&cctx, (void *)&worker_wrapper, 1, arg);

	Node* tcb_block = (Node *)malloc(sizeof(Node));
	tcb_block->block = block;
	queue(&runq_last, tcb_block);

	runq_curr = tcb_block;

	if (swapcontext(&main_ctx,runq_curr->block->context) < 0){
		perror("set current context");
		exit(1);
	}
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
};

/* Not sure how to handle the base case, in the sense that when we call
worker_create from else where, we do not have a way to grab that parent thread to
then return or context switch from. This is a kind of jank solution which is marking a specific thread
as the parent/main tread and assuming callers to worker_create are children of this thread */
int main_worker_create(worker_t* thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {
	// Initialize the main context

	if(scheduler_initialized == 0) {
		thread_init();
	}

	if (getcontext(&main_ctx) < 0){
		perror("Getcontext failed");
		exit(1);
	}

	tcb *block = (tcb *)malloc(sizeof(tcb));
	block->stack = malloc(STACK_SIZE);
	if (block->stack == NULL) {
    	perror("Failed to allocate stack");
    	return -1;
	}

	main_ctx.uc_link = NULL;
	main_ctx.uc_stack.ss_sp = malloc(STACK_SIZE);
	main_ctx.uc_stack.ss_size = STACK_SIZE;
	main_ctx.uc_stack.ss_flags = 0;
	block->context = &main_ctx;

	block->status = Ready;
	block->priority = 4;
	block->thread_id = *thread;
	block->function = function;

	makecontext(&main_ctx,(void *)&function, 0);

	Node* tcb_block = (Node *)malloc(sizeof(Node));
	tcb_block->block = block;
	queue(&runq_last, tcb_block);

	runq_curr = tcb_block;

	if (setcontext(&main_ctx) < 0){
		perror("set current context");
		exit(1);
	}
};

#ifdef MLFQ
/* This function gets called only for MLFQ scheduling set the worker priority. */
int worker_setschedprio(worker_t thread, int prio) {


   // Set the priority value to your thread's TCB
   // YOUR CODE HERE
   Node* ptr = runq_last->next;
	while(ptr != runq_last && ptr->block->thread_id != thread) {
		ptr = ptr->next;
	}
	if(ptr->block->thread_id == thread) {
		ptr->block->priority = prio;
		return 0;	// Successfully set
	}
	return -1;	// Failed to set
}
#endif



/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context

	// YOUR CODE HERE
	ucontext_t* cctx;
	getcontext(cctx);
	// Might not work? What happens with the stack pointer?
	runq_curr->block->status = Ready;
	free(runq_curr->block->context);
	runq_curr->block->context = cctx;

	swapcontext(cctx, &scheduler_ctx);
	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	// - de-allocate any dynamic memory created when starting this thread

	// YOUR CODE HERE
};


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
  
	// YOUR CODE HERE
	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//- initialize data structures for this mutex

	// YOUR CODE HERE
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

        // YOUR CODE HERE
        return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	// - de-allocate dynamic memory created in worker_mutex_init

	return 0;
};

/* scheduler */
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

	//temporary round robin scheduler
	runq_curr = runq_curr->next;
	printf("running: %d\n",(runq_curr->block->thread_id));
	setcontext(runq_curr->block->context);
	// swapcontext(&scheduler_ctx,runq_curr->block->context);


// - schedule policy
#ifndef MLFQ
	// Choose PSJF
#else 
	// Choose MLFQ
#endif

}

/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf() {
	// - your own implementation of PSJF
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}


/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
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

