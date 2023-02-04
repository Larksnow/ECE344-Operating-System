//Fan Weite #1006761933
#include "common.h"
#include <assert.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>

void
usage()
{
	fprintf(stderr, "Usage: cpr srcdir dstdir\n");
	exit(1);
}

void filecp(char*src, char* dest){
    int fread,fwrite,sb;
    struct stat ss;
    fread = open(src, O_RDONLY);
    if(fread < 0){
        syserror(open, src);
    }
    sb = stat(src,&ss);
    if(sb < 0){
        syserror(stat,src);
    } 
    if((ss.st_mode & S_IFMT) == S_IFREG){
        fwrite = creat(dest, ss.st_mode & 00777);//get file type
        if(fwrite < 0){
            syserror(creat, dest);
        } 
        char buf[4096];
        int ret;
        while((ret=read(fread,buf,4096))){//write file into new file
            if(ret<0){
                syserror(read,src);
            } 
            int w = write(fwrite,buf,ret);
            if(w<0){
                syserror(write,dest);
            }
        }
    }
    if(close(fread)!= 0){
        syserror(close,src);
    }
    if(close(fwrite)!= 0){
        syserror(close,dest);
    }
}

void dircp(char *src, char *dest) {
    DIR *pDir = opendir(src); 
    char slash = '/';
    struct stat info;              

    if(!pDir){
        syserror(opendir, src);
    }            
        char Path1[4096], Path2[4096];
        struct dirent *ss;
        ss = readdir(pDir);
        if(ss == NULL){
            syserror(readdir,src);
        }
        while(ss!=NULL){
        if((strcmp(ss->d_name, ".")!=0)&&(strcmp(ss->d_name,"..")!=0)){
            strcpy(Path1, src); 
            strncat(Path1, &slash,1);//add slash at the end of pathname
            strcat(Path1, ss->d_name); //add file/director name at the end of pathname
            if(stat(Path1, &info)<0){
                syserror(stat,Path1);  
            }
            strcpy(Path2, dest);
            strncat(Path2, &slash,1);//add slash at the end of pathname
            strcat(Path2, ss->d_name); //add file/director name at the end of pathname
            if(S_ISDIR(info.st_mode)){  //dealing with a directory
                int m = mkdir(Path2, 00700);//create a directory
                if(m<0){
                    syserror(mkdir, Path2);
                }
                dircp(Path1,Path2);//call the recursion function again  
            } 
            else if(S_ISREG(info.st_mode)) { //find a regular file
                filecp(Path1,Path2);//copy the file
            }
        }
        ss=readdir(pDir);//read the rest file in the current directory
        }
        stat(src,&info);
        chmod(dest,info.st_mode);//extract permissions of the source file
    }

int
main(int argc, char *argv[])
{
	if (argc != 3) {
            usage();
	}        

	char path_src[4096];
        char path_dest[4096];
        strcpy(path_src, argv[1]);
        strcpy(path_dest, argv[2]);
        int directory;
        directory = mkdir(argv[2], 00700);//create root directory for the copy
        if(directory < 0){
            syserror(mkdir, argv[2]);
        }
        dircp(path_src, path_dest);
	return 0;
}
