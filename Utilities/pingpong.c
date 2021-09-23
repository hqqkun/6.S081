#include "kernel/types.h"
#include "user/user.h"

#define R 0
#define W 1

int main(int argc, char const *argv[])
{
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
		read(ping[R], (void*)&cc, 1);
		printf("%d: received ping\n", getpid());
		cc = 'C';
		write(pong[W], (void*)&cc, 1);

		close(ping[R]);
		close(pong[W]);

		exit(0);
	}
	// parent process
	char c = 'P';
	write(ping[W], (void*)&c, 1);
	read(pong[R], (void*)&c, 1);
	printf("%d: received pong\n", getpid());

	close(ping[W]);
	close(pong[R]);
	
	exit(0);
}
