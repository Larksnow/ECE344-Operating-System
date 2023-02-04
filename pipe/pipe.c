#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
int counter = 1;
int max = 0;
int pipnum = 0;

void check_err(int value, char*message){
    if(value!=-1){
        return;
    }
    int err= errno;
    perror(message);
    exit(err);
}

void parent(int fd[pipnum][2], int childpid){
    
    int wait;
    check_err(waitpid(childpid, &wait, 0), "wait");
        assert(WIFEXITED(wait));
    int exit_status = WEXITSTATUS(wait);
    if(exit_status != 0){
        exit(exit_status);
    }
    assert(exit_status == 0);
    if(pipnum != 0){
        if(counter == 1){
            check_err(close(fd[counter-1][1]), "dup2");
        }
        if(counter>1 && counter<max){
            check_err(close(fd[counter-2][0]), "close");
            check_err(close(fd[counter-1][1]), "close");
        }
        if(counter == max){
            check_err(close(fd[counter-2][0]), "close");
        }
    }
    counter++; 
}

void child(int fd[pipnum][2], char* filename){
    if(pipnum != 0){
        if(counter == 1){
            check_err(dup2(fd[counter-1][1], STDOUT_FILENO), "dup2");
            check_err(close(fd[counter-1][1]), "dup2");
        }
        if(counter>1 && counter<max){
            check_err(dup2(fd[counter-2][0], STDIN_FILENO), "dup2");
            check_err(dup2(fd[counter-1][1], STDOUT_FILENO), "dup2");
            check_err(close(fd[counter-2][0]), "close");
            check_err(close(fd[counter-1][1]), "close");
        }
        if(counter == max){
            check_err(dup2(fd[counter-2][0], STDIN_FILENO), "dup22");
            check_err(close(fd[counter-2][0]), "close");
        }
         
    }
    execlp(filename, filename, NULL);
    exit(errno);
}

int main(int argc, char *argv[]) {
    if(argc <2 ){
        return EINVAL;
    }   
    max = argc-1;
    pipnum = argc-2;
    int fd[pipnum][2];
    int pidlist[max];
    if(pipnum > 0){  
        for(int i = 0; i<pipnum; ++i){
            check_err(pipe(fd[i]), "pipe");
        }
    }
    while(counter <= max){       
        int pid = fork(); 
        if(pid>0){
            parent(fd, pid);  
        }
        else{
            child(fd, argv[counter]);
        }
    }
    return 0;
}
