#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

#define MaxPathSize 512
#define MaxNameSize 64

static char*
fmt_name(const char *path)
{
	/* 1 for '\0' */
  	static char buf[MaxNameSize + 1];
  	int length;
  	const char *p;

  	// Find first character after last slash.
  	for(p = path + strlen(path); p >= path && *p != '/'; --p)
    	;
  	++p;

  	length = strlen(p);
  	if(length > MaxNameSize)
    	return (char*)0;
	// 1  for '\0' 
  	memmove(buf, p, length + 1);
  	return buf;
}

void 
find(const char* path,const char* filename)
{
	char* buf = (char*)malloc(MaxPathSize * sizeof(char));
	char* p;
	int fd;
	/* the total path length */ 
	int length;	
	struct dirent de;
	struct stat st;

	if((fd = open(path,0)) < 0){
		fprintf(2, "find: cannot open %s\n",path);
		return;
	}

	if(fstat(fd,&st) < 0){
		fprintf(2, "find: cannot stat %s\n",path);
		close(fd);
		return;
	}
	switch(st.type){
		case T_FILE:
			/* check the current filename with the specific file. */
			if(!strcmp(fmt_name(path),filename))
				printf("%s\n",path);
			break;

		case T_DIR:
			/* check the current directory */
			/* 1 for '/' and 1 for '\0' */
			if(strlen(path) + 1 + MaxNameSize + 1 > (MaxPathSize * sizeof(char))){
				printf("find: path too long.\n");
				break;
			}

			strcpy(buf,path);
			p = buf + strlen(buf);
			*p++ = '/';

			while(read(fd,&de,sizeof(de)) == sizeof(de)){
				/* we don't recurse into "." and ".." */
				if(de.inum == 0 || !strcmp(de.name,".") || !strcmp(de.name,".."))
					continue;
				length = strlen(de.name);

				memmove(p,de.name,length);
				p[length] = 0;
				if(stat(buf,&st) < 0){
					printf("find: cannot stat %s\n",buf);
					continue;
				}
				find(buf,filename);
			}
			break;

	}
	free(buf);
	close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc != 3){
  	fprintf(2, "format:<path> <filename>\n");
    exit(1);
  }
  find(argv[1],argv[2]);
  exit(0);
}
