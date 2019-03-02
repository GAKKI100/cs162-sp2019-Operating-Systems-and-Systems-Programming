#include <stdio.h>
#include <sys/resource.h>

int main() {
    struct rlimit lim; 
    if(getrlimit(RLIMIT_STACK, &lim) < 0){  
        perror("getrlimit");  
    }      
    printf("stack size: %ld\n", lim.rlim_cur);

    if(getrlimit(RLIMIT_NPROC, &lim) < 0){
        perror("getrlimit");        
    }   
    printf("process limit: %ld\n", lim.rlim_cur);

    if(getrlimit(RLIMIT_NOFILE, &lim) < 0){
        perror("getrlimit");        
    }   
    printf("max file descriptors: %ld\n", lim.rlim_cur);
    return 0;
}
