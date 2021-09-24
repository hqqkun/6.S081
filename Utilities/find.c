#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

/* buf will store the path of the file. */
static char buf[512] = {0};

static void find(char*, const char*);
static char* fmtname(char*);

int main(int argc, char const *argv[]) {
	if (argc < 2) {
		fprintf(2, "Usage: find filename\n");
		exit(1);
	}

	strcpy(buf, argv[1]);
	find(buf, argv[2]);
	exit(0);
}


static void find(char* path, const char* fileName) {
	char* p;
	int fd;
	struct dirent de;
  	struct stat st;

  	if((fd = open(path, 0)) < 0){
    		fprintf(2, "find: cannot open %s\n", path);
    		return;
  	}
	if(fstat(fd, &st) < 0){
    		fprintf(2, "find: cannot stat %s\n", path);
    		close(fd);
    		return;
  	}

  	switch (st.type) {
  		case T_FILE:
  			if (strcmp(fmtname(path), fileName) == 0) {
  				fprintf(1, "%s\n", path);
  			}
  			break;
  		case T_DIR:
  			p = buf + strlen(buf);
  			*p++ = '/';
  			while (read(fd, &de, sizeof(de)) == sizeof(de)) {
				/* we don't recurse into "." and ".." */
  				if (de.inum == 0) 
  					continue;
  				if (strcmp(de.name, ".") == 0)
  					continue;
  				if (strcmp(de.name, "..") == 0) 
  					continue;
  				int length = strlen(de.name);
  				memmove(p, de.name, length);
  				p[length] = 0;
  				find(buf, fileName);
  			}
  			break;
  	}	
  	close(fd);
  	return;
}

static char* fmtname(char *path) {
	char *p;

  	// Find first character after last slash.
  	for (p = path+strlen(path); p >= path && *p != '/'; p--)
    	;
  	p++;
  	return p;
}
