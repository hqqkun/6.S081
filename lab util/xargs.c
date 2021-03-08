#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"
 
#define MaxBufLen  0x400

int main(int argc, char* argv[])
{
	/* record argv */
	char* argv_array[MAXARG];
	char* buffer = (char*)malloc(MaxBufLen * sizeof(char));
	memset(buffer,0,MaxBufLen);

	char c = 0;

	/* the argvs behind xargs are the command, so 
	   don't change them when processing each lines. */
	const int cmd_cnt = argc - 1;

	/* start_index and current_index refer to string in buffer. */
	int argv_cnt, start_index, current_index;
	argv_cnt = start_index = current_index = 0;

	/* ignore the "xargs" itself */
	for(int i = 1;i != argc; ++i)
		argv_array[i - 1] = argv[i];
	argv_cnt = cmd_cnt;

	/* read one byte a time */
	while(read(0,&c,sizeof(char)) != 0){
		if(current_index >= MaxBufLen){
			fprintf(2, "command is too long.\n");
			exit(1);
		}
		if(c == ' ' || c == '\n'){
			/* finish a argv, so we copy the pointer
			    this is very tricky, but I don't know whether blank space should be checked. */
			buffer[current_index++] = 0;
			argv_array[argv_cnt++] = buffer + start_index;
			start_index = current_index;

			if(c == ' ')
				continue;

			/* finish a line, then do fork and exec */
			argv_array[argv_cnt] = 0;
			if(!fork()){
				/* child process */
				exec(argv_array[0],argv_array);
				fprintf(2, "exec error!\n");
				exit(1);
			}
			wait((int*)0);
			/* by now, eliminated a line */ 
			current_index = start_index = 0;
			argv_cnt = cmd_cnt;
		}
		else
			buffer[current_index++] = c;

	}
	/* Do not forget to free */
	free(buffer);
	exit(0);
}
