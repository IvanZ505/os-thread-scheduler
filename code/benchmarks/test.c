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
int sum(){
	int x = 1000;
	int y = 2000;
	return x*y;
}

int main(int argc, char **argv) {

	for (int i = 0; i < 10; i++) {
		int currThread = worker_create(&i, NULL, sum, NULL);
		printf("thread %d: return %d\n", i, currThread);
	}
	return 0;
}
