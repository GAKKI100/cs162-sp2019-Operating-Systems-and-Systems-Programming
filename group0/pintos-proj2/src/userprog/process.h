#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

typedef int pid_t;

#define PID_ERROR ((pid_t) -1)
#define PID_INITIALIZING ((pid_t) -2)

pid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/* PCB: see initialization at process_execute(). */
struct process_control_block{
    pid_t pid;  //The pid of process
    const char *cmdline; //The command line of the process being ececuted
    struct list_elem elem; //element for thead.child_list
    bool waiting;  //indicates whether parent process is waiting on this
    bool exited;   //indicates whether the process is done(exited)
    int32_t exitcode;  // the eixt code passed from exit(), when exit = true
    bool orphan;    // indicates whether the parents process has terminates before 
   
    /* synchronization */
    struct semaphore sema_initialization; 
    //the semaphore used between start_process() and process_execute()
    struct semaphore sema_wait;
    //the semaphore used for wait():parent blocks until child exit.
};

struct file_desc{
    int id;
    struct list_elem elem;
    struct file *file;  
};

#endif /* userprog/process.h */
