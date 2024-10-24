#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "../thread-worker.h"

/* A scratch program template on which to call and
 * test thread-worker library functions as you implement
 * them.
 *
 * You can modify and use this program as much as possible.
 * This will not be graded.
 */

// int i = 1;
// int j = 2;
// int k = 3;
int val = 0;

//Simple two thread test
// void foo(){
// 	while (1) {
// 		printf("foo\n");
// 	}
// }

// void bar(){
// 	while (1) {
// 		printf("bar\n");
// 	}
// }


// void bar(){
// 	int l = 0;
// 	while (l < 100) {
// 		printf("%d ", l++);
// 	}
// 	printf("aaaa\n");
// 	worker_exit(&val);
// }

// // Worker yield test
// void foo(){
// 	printf("Waiting for bar to finish\n");
// 	if(worker_join(j, NULL) == 0) {
// 		printf("bar finished\n");
// 	}
// 	worker_exit(&val);
// }


// int main(int argc, char **argv) {
// 	int fooThread = worker_create(&i, NULL, &foo, NULL);
// 	int barThread = worker_create(&j, NULL, &bar, NULL);

// 	if (worker_join(i, NULL) == -1) {
// 		printf("worker join error\n");
// 	}

// 	return 0;
// }

// pthread_t t1, t2;
// worker_mutex_t* i;

// void* print_periodically(void* arg) {
//     worker_mutex_lock(&i);

//     for (int i = 0; i < 6; i++) {
//         sleep(1);
//         printf("Printing some text, time = %d\n", i+1);
//     }
//     int* result = (int*) malloc(sizeof(int));
//     *result = 12;
//     worker_mutex_unlock(&i);
//     printf("exiting thread 2\n");
//     pthread_exit(result);
// }

// void* print_periodically_again(void* arg) {
//     worker_mutex_lock(&i);
//     for (int i = 0; i < 3; i++) {
//         sleep(2);
//         printf("Printing some more text, time = %d\n", 2*(i+1));
//     }
//     worker_mutex_unlock(&i);
//     printf("exiting thread 3\n");
//     pthread_exit(NULL);
// }

// int main(int argc, char **argv) {
//     worker_mutex_init(&i, NULL);

//     pthread_create(&t1, NULL, &print_periodically, NULL);
//     pthread_create(&t2, NULL, &print_periodically_again, NULL);

//     printf("Our two threads are %d and %d\n", t1, t2);

//     void* status;
//     printf("joining thread %d\n", t1);
//     pthread_join(t1, &status);
//     printf("retval is %d\n", *((int*) status));
//     free(status);

//     printf("joining thread %d\n", t2);
//     pthread_join(t2, NULL);

//     printf("destroying mutex\n");
//     worker_mutex_destroy(&i);

//     print_app_stats();
//     return 0;
// }

int foo() {
    while (1) {
        printf("foo\n");
    }
}

int bar() {
    while (1) {
        
    }
}

int threads[8] = {2,3,4,5,6,7,8,9};

int main() {
    for (int i = 1; i < 9; i++) {
        printf("thread %d created\n", i+1);
        pthread_create(threads+i-1, NULL, &foo, NULL);
    }

    while (1) {
        
    }
}