#include <types.h>
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/seek.h>
#include <vfs.h>
#include <vnode.h>
#include <current.h>
#include <thread.h>
#include <proc.h>
//#include <openfile.h>
#include <limits.h>
#include <kern/unistd.h>
#include <kern/stat.h>
#include <kern/iovec.h>
#include <uio.h>
#include <lib.h>
#include <addrspace.h>
#include <kern/wait.h>
#include <synch.h>
#include <vm.h>
#include <mips/trapframe.h>

int sys_fork(struct trapframe *tf, pid_t *retval){

    int err;
    struct proc *p = curproc; // parent process (i.e. calling one)
    struct proc *childproc = NULL;
    struct trapframe *childtf;

    // There are already too many processes on the system
    if(proc_counter >= MAX_PROCESSES){
        err = ENPROC;
        return err;
    }

    // Create and initialize the child process
    // ( PID searching and its proctable insertion )
    childproc = proc_create("child");

    if(childproc == NULL){
        err = ENOMEM;
        return err;
    }

    // TODO: Bloccare proctable con un semaforo ?

    // Parent is the calling process (curproc)
    childproc->p_parentpid = p->p_pid;

    // Copy parent's addrespace
    err = as_copy(p->p_addrspace, &(childproc->p_addrspace));
    if(err){
        return err;
    }

    // Copy parent's trapframe (known the new addrsapce)
    childtf = kmalloc(sizeof(struct trapframe));
    if(childtf == NULL){
        err = ENOMEM;
        return err;
    }
    //memcpy(childtf, tf, sizeof(struct trapframe *));
    //memmove(childtf, tf, sizeof(struct trapframe *));
    //childtf = tf;
    *childtf = *tf;

    // TO DO: Copy parent's filetable

    // Launch the child execution (first thread creation)
    err = thread_fork("child",childproc,(void*)enter_forked_process,
                      childtf,(unsigned long)childproc->p_addrspace);
    if(err){
        return err;
    }

    // In the parent process the child pid is returned
    *retval = childproc->p_pid;

    return 0;
}

int sys__exit(int exitcode){

    int i = 0;

    spinlock_acquire(&curproc->p_lock);

    pid_t pid = curproc->p_pid;
    pid_t ppid = curproc->p_parentpid;

    // TODO: chiusura file se ce n'è aperti

    // Check the presence of curproc in proctable
    while(proctable[i]->p_pid != pid){
        i++;
    }
    if(i == MAX_PROCESSES){
        return 0;
    }

    curproc->exitcode = exitcode; // TODO: _MKWAIT_EXIT(exitcode) ??
    curproc->is_exited = true;

    spinlock_release(&curproc->p_lock);

    if(proctable[ppid]->is_waiting){
        // Signal the semaphore for waitpid
        V(&curproc->p_waitsem);

        // Cause the current thread to exit
        thread_exit(); // is zombie (pointer is NULL but proc is still in memory)
    }else{
        // Cause the current thread to exit
        // proc_remthread(curthread); o qui o dentro thread_exit TODO
        thread_exit(); // is zombie (pointer is NULL but proc is still in memory)

        // Destroy the pid process
        proc_destroy(proctable[pid]);
        // pid is now available
        proctable[pid] = NULL;
    }

    return 0;
}