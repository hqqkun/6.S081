#include "kernel/types.h"
#include "user/user.h"


int main(int argc, char const *argv[]) {
	if (argc < 2) {
		fprintf(2, "Usage: time\n");
		exit(1);
	}
	
	// xv6 rewrite atoi, so only nonnegative numbers.
	sleep(atoi(argv[1]));
	exit(0);
}
