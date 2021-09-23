#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
 
#define R 0
#define W 1
void pipelineprime(int fd);
void redirect(int k,int* pipe);

int main(int argc, char const *argv[]) {
	int write_pipe[2];
	if (pipe(write_pipe) < 0)
		exit(1);
	if (!fork()) {
		// child process
		redirect(R,write_pipe);
		pipelineprime(R);
		exit(0);
	}

	// father process
	redirect(W,write_pipe);
	for (int num = 2; num != 36; ++num)
		write(W,&num,sizeof(int));

	close(W);
	wait((int*)0);
	exit(0);
}

/* match stdin or stdout to pipe fd and close them. */
void redirect(int k,int pipe[]) {
	close(k);
	dup(pipe[k]);
	close(pipe[R]);
	close(pipe[W]);
}

/* a recursive function that will fork a child 
   process to sieve the next prime number. */
/* we guarantee the current process will 
   receive at least one number from it's father by reading fd */
void pipelineprime(int fd) {
	/* stage_prime means other numbers will be sieved by it. */
	int stage_prime,prime;
	/* used to transmit numbers to it's child */
	int write_pipe[2];
	int prime_count = 0;
	/* check if generate a pipe */
	int pipe_flag = 0;				
	read(fd,&stage_prime,sizeof(int));

	printf("prime %d\n",stage_prime);

	/* read until there is nothing to read */
	while (read(fd,&prime,sizeof(int)) != 0 ) {
		if ((prime % stage_prime) != 0) {
			// might find a prime
			++prime_count;
			if (prime_count == 1) {
				/* fork a child process only if we find the first 
				   number that cannot divided by stage_prime. */ 
				if (pipe(write_pipe) < 0)
					exit(1);
				else
					pipe_flag = 1;
				
				if (!fork()) {
					/* child */
					redirect(R,write_pipe);
					pipelineprime(R);
					exit(0);
				}
				/* father */
				redirect(W,write_pipe);
			}
			write(W,&prime,sizeof(int));		
		}
	}
	close(fd);
	if (pipe_flag) {
		close(W);
		wait((int*)0);
	}
}
