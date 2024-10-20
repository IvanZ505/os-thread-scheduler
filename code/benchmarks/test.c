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

int i = 1;
int j = 2;
int k = 3;
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


void bar(){
	int l = 0;
	while (l < 100) {
		printf("%d ", l++);
	}
	printf("aaaa\n");
	worker_exit(&val);
}

// Worker yield test
void foo(){
	printf("Waiting for bar to finish\n");
	if(worker_join(j, NULL) == 0) {
		printf("bar finished\n");
	}
	worker_exit(&val);
}


int main(int argc, char **argv) {
	int fooThread = worker_create(&i, NULL, &foo, NULL);
	int barThread = worker_create(&j, NULL, &bar, NULL);

	if (worker_join(i, NULL) == -1) {
		printf("worker join error\n");
	}

	return 0;
}
