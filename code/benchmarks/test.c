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

void foo(){
	while (1) {
		printf("foo\n");
	}
}

void bar(){
	while (1) {
		printf("bar\n");
	}
}

int main(int argc, char **argv) {

	int fooThread = worker_create(&i, NULL, &foo, NULL);
	int barThread = worker_create(&j, NULL, &bar, NULL);

	while(1) {
		printf("main\n");
	}
	return 0;
}
