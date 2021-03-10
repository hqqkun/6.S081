#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
 
#define R 0
#define W 1

int main(int argc,char* argv[])
{
	int child_read[2];
	int parent_read[2];
	char buffer[2];
	
	if(pipe(child_read) < 0 ){
		fprintf(2,"error.");
		exit(1);
	}
	else if(pipe(parent_read) < 0){
		fprintf(2,"error.");
		exit(1);
	}
	else{
		int temp = fork();
		if(temp < 0)
			exit(1);
		else if(!temp){
		/* child process read and write to parent
		   close none use fd, match stdin to child_read */
		close(R);
		dup(child_read[R]);
		close(child_read[R]);
		close(child_read[W]);
		close(parent_read[R]);

		read(R,buffer,1);
		printf("%d: received ping\n",getpid());
		write(parent_read[W],buffer,1);

		close(R);
		close(parent_read[W]);
		exit(0);
		}
		
		else{
		/* father write and read from child 
		   close none use fd, match stdin to parent_read */ 
		close(R);
		dup(parent_read[R]);
		close(parent_read[R]);
		close(parent_read[W]);
		close(child_read[R]);
		
		write(child_read[W],buffer,1);
		read(R,buffer,1);
		printf("%d: received pong\n",getpid());

		close(R);
		close(child_read[W]);
		exit(0);
		}
	}
}
		
		
