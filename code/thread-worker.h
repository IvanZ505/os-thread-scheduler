// File:	worker_t.h

// List all group member's name:
//Pedro Torres (pt397) and Ivan Zheng (iz60)
// iLab Server: vi.cs.rutgers.edu, ilab1.cs.rutgers.edu

#ifndef WORKER_T_H
#define WORKER_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_WORKERS macro */
#define USE_WORKERS 1
#define STACK_SIZE SIGSTKSZ

#define TIME_QUANTUM 10000


/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <stdatomic.h>
#include <sys/time.h>

typedef uint worker_t;

enum status {
	Running,
	Ready,
	Blocked,
	Terminated,
	Yielding
};

typedef struct TCB {
	/* add important states in a thread control block */
	// thread Id
	// thread status
	// thread context
	// thread stack
	// thread priority
	// And more ...

	// YOUR CODE HERE
	int thread_id;
	enum status status;
	ucontext_t* context;
	char* stack;
	int priority;
	// For the implementation of PSJF
	int elapsed;
	int ran_first;
	// long int total_runtime;
	struct timeval start;
	// For the implementation of MLFQ
	int yielded;
	struct timeval runtime;
	int quantum_used;
	void *(*function)(void*); 
} tcb; 	


/* Priority definitions */
#define NUMPRIO 4

#define HIGH_PRIO 3
#define MEDIUM_PRIO 2
#define DEFAULT_PRIO 1
#define LOW_PRIO 0

// Refresh Quantum is how many time the Quantum to refresh the MLFQ's queues
#define REFRESH_QUANTUM 50000


/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

// YOUR CODE HERE

typedef struct Node {
	tcb* block;
	struct Node* next;
} Node;

int queue(Node** last, Node* tcb_node);

int dequeue(Node** last, Node* tcb_node, int freeing);

void printList(Node* last);

int freeList(Node** last);

/* Function Declarations: */

/* mutex struct definition */
typedef struct worker_mutex_t {
	/* add something here */

	// YOUR CODE HERE
	atomic_int locked;             // 0 for unlocked, 1 for locked
    worker_t owner;
	Node** queue;            // Do we need a queue?
    unsigned int id;       
} worker_mutex_t;

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* create a main parent thread */
int main_worker_create(worker_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level worker threads voluntarily */
int worker_yield();

/* terminate a thread */
void worker_exit(void *value_ptr);

/* wait for thread termination */
int worker_join(worker_t thread, void **value_ptr);

/* initial the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex);

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex);

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex);


/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void);

#ifdef USE_WORKERS
#define pthread_t worker_t
#define pthread_mutex_t worker_mutex_t
#define pthread_create worker_create
#define pthread_exit worker_exit
#define pthread_join worker_join
#define pthread_mutex_init worker_mutex_init
#define pthread_mutex_lock worker_mutex_lock
#define pthread_mutex_unlock worker_mutex_unlock
#define pthread_mutex_destroy worker_mutex_destroy
#define pthread_setschedprio worker_setschedprio
#endif

#endif
