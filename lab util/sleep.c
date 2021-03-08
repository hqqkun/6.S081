#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
   
int main(int argc,char* argv[])
{
	if(argc == 1){
		fprintf(2,"you need to tell a time to sleep.\n");
		exit(1);
		}
	else if(argc >= 3){
		fprintf(2,"too much arguments.\n");
		exit(1);
		}
		
	else{
		/* change string into integer */
		sleep(atoi(argv[1]));
		exit(0);
		}
}
		
