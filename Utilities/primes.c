#include "kernel/types.h"
#include "user/user.h"

#define R 0
#define W 1

void reDirect(int direction, int *pipe) {
	close(direction);
	dup(pipe[direction]);
	close(pipe[R]);
	close(pipe[W]);
}

void pipelinePrime(int fd);

int main(void) {
	// process gen 2 ~ 35
	int primePipe[2];
	if (pipe(primePipe) < 0)
		exit(1);

	int childPid = fork();
	if (childPid < 0)
		exit(1);

	if (childPid) {
		// parent process
		reDirect(W, primePipe);
		for (int i = 2; i != 36; ++i)
			write(W, (void*)&i, sizeof(int));

		close(W);
		wait((int*)0);
		exit(0);
	}
	else {
		// child process
		reDirect(R, primePipe);
		pipelinePrime(R);
		exit(0);
	}
}

void pipelinePrime(int fd) {
	// read from fd
	int sieve, readNum;
	int primePipe[2];
	int primeCount = 0;
	int pipeCreateFlag = 0;

	read(fd, (void*)&sieve, sizeof(int));
	fprintf(2, "prime %d\n", sieve);

	while (read(fd, (void*)&readNum, sizeof(int)) != 0) {
		if (readNum % sieve != 0) {
			++primeCount;
			if (primeCount == 1) {
				if (pipe(primePipe) < 0)
					exit(1);

				pipeCreateFlag = 1;

				int pid = fork();
				if (pid < 0)
					exit(1);
				else if (!pid) {
					reDirect(R, primePipe);
					pipelinePrime(R);
					exit(0);
				}
				else {
					reDirect(W, primePipe);
				}
			}
			write(W, (void*)&readNum, sizeof(int));
		}
	}
	close(fd);
	if (pipeCreateFlag) {
		close(W);
		wait((int*)0);
	}
}
