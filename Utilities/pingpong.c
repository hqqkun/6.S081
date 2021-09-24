#include "kernel/types.h"
#include "user/user.h"

#define R 0
#define W 1


static inline void reDirect(int direction, int* pipe) {
	close(direction);
	dup(pipe[direction]);
	close(pipe[R]);
	close(pipe[W]);
}

int main(int argc, char const *argv[]) {
	int ping[2];
	int pong[2];
	if (pipe(ping) != 0) {
		exit(1);
	}
	if (pipe(pong) != 0) {
		exit(1);
	} 
	int childId = fork();
	if (childId < 0) {
		exit(1);
	}
	if (!childId) {
		// child process
		char cc;

		reDirect(R, ping);
		reDirect(W, pong);
		
		read(R, (void*)&cc, 1);
		fprintf(2, "%d: received ping\n", getpid());
		cc = 'C';

		write(W, (void*)&cc, 1);

		close(R);
		close(W);
		exit(0);
	}
	// parent process
	char c = 'P';

	reDirect(W, ping);
	reDirect(R, pong);

	write(W, (void*)&c, 1);
	read(R, (void*)&c, 1);
	fprintf(2, "%d: received pong\n", getpid());

	wait(0);
	close(R);
	close(W);
	exit(0);
}
