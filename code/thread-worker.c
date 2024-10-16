// File:	thread-worker.c

// List all group member's name:
// username of iLab:
// iLab Server:

#include "thread-worker.h"

//Global counter for total context switches and 
//average turn around and response time
long tot_cntx_switches=0;
double avg_turn_time=0;
double avg_resp_time=0;


// INITAILIZE ALL YOUR OTHER VARIABLES HERE
// YOUR CODE HERE
extern Node* head;
extern Node* curr;
extern ucontext_t scheduler_ctx, main_ctx;

// @TODO: need to add freeing for the TCB blocks and the stacks inside.

int queue(Node* tcb_block) {
	if (head == NULL) {
		head = tcb_block;
		head->next = head;
		return 0;
	}

	tcb_block->next = head->next;
	head.next = tcb_block;

	head = head->next;
	return 0;
}


int dequeue(Node* tcb_block) {
    if (*head == NULL) {
        // List is empty, nothing to dequeue
        return -1; // Indicate failure
    }

    Node *current = *head, *prev = NULL;

    // Case 1: If the node to be removed is the only node in the list
    if (current == tcb_block && current->next == current) {
		// Free TCB block
        free(current);
        *head = NULL; // List is now empty
        return 0;     // Indicate success
    }

    // Traverse the list to find the node to delete
    while(current != head && current != tcb_block) {
		prev = current;
		current = current->next;
	}

	if (current == tcb_block) {
		prev->next = current->next;
		free(current);
		return 0; // Indicate success
	}

	// If we reach here, the node was not found in the list
	return -1; // Indicate failure
}

void printList() {
	if (*head == NULL) {
		printf("List is empty\n");
		return;
	}

	Node *current = head;
	while (current != head) {
		printf("%d ", current->block->thread_id);
		if(current->next != head) {
			printf("-> ");
		}
		current = current->next;
	}
	printf("\n");
}

// Last thing to run for freeing!!
int freeList() {
	if (*head == NULL) {
		return -1; // Indicate failure
	}

	Node *current = head->next;
	while (current != head) {
		free(current);
		current = current->next;
	}

	free(head);
	*head = NULL; // List is now empty
	return 0;     // Indicate success
}

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {

       // - create Thread Control Block (TCB)
       // - create and initialize the context of this worker thread
       // - allocate space of stack for this thread to run
       // after everything is set, push this thread into run queue and 
       // - make it ready for the execution.

       // YOUR CODE HERE

	if(schedule_ctx == NULL) {
		// Initialize the main context
		main_ctx.uc_link = NULL;
		main_ctx.uc_stack.ss_sp = malloc(STACK_SIZE);
		main_ctx.uc_stack.ss_size = STACK_SIZE;
		main_ctx.uc_stack.ss_flags = 0;
		makecontext(&main_ctx, (void (*)(void))main, 0);

		// Initialize the scheduler context
		scheduler_ctx.uc_link = NULL;
		scheduler_ctx.uc_stack.ss_sp = malloc(STACK_SIZE);
		scheduler_ctx.uc_stack.ss_size = STACK_SIZE;
		scheduler_ctx.uc_stack.ss_flags = 0;
		makecontext(&scheduler_ctx, (void (*)(void))schedule, 0);
	}
	
	TCB *block = (TCB *)malloc(sizeof(TCB));
	ucontext_t cctx;
	block->stack = malloc(STACK_SIZE);
	if (block->stack == NULL) {
    	perror("Failed to allocate stack");
    	return -1;
	}
	cctx.uc_stack.ss_sp = block->stack;
	cctx.uc_link=&scheduler_ctx;
	cctx.uc_stack.ss_size=STACK_SIZE;
	cctx.uc_stack.ss_flags=0;
	block->context = &cctx;

	block->status = enum status Ready;
	block->priority = 4;
	block->thread_id = *thread;
	makecontext(&block->context, (void (*)(void))function, 1, arg);
	if (getcontext(&block->context) < 0) {
		perror("getcontext failed");
    	return -1;
	}

	Node* tcb_block = (Node *)malloc(sizeof(Node));
	tcb_block->block = block;
	queue(tcb_block);
    return 0;
};


#ifdef MLFQ
/* This function gets called only for MLFQ scheduling set the worker priority. */
int worker_setschedprio(worker_t thread, int prio) {


   // Set the priority value to your thread's TCB
   // YOUR CODE HERE
   Node* ptr = head.next;
	while(ptr != head && ptr->block->thread_id != thread) {
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
	curr->block->status = enum status Ready;
	free(curr->block->context);
	curr->block->context = cctx;

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

