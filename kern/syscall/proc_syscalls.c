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
#include <openfile.h>
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
    *childtf = *tf;

    // Copy parent's filetable
    for(int i=0;i<OPEN_MAX;i++) {
		if(p->p_filetable[i] != NULL){
			P(p->p_filetable[i]->of_sem);

            p->p_filetable[i]->of_refcount++;
			childproc->p_filetable[i] = p->p_filetable[i];
			
            V(p->p_filetable[i]->of_sem);
		}
	}

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

int sys_getpid(pid_t *retval){

    struct proc *p = curproc;

    P(&p->p_sem);

    *retval = curproc->p_pid;

    V(&p->p_sem);

    return 0;
}

int sys_waitpid(pid_t pid, int *status, int options, pid_t *retval){
    int err;
    struct proc *p = curproc;
    int *kstatus = (int *)kmalloc(sizeof(int));
    struct proc *childp;

    // Get child process from global proctable
    P(&pt_sem);
    childp = proctable[pid];
    V(&pt_sem);

    (void)options;

    // The options argument requested invalid or unsupported options.
    /*if(options != 0){
        err = EINVAL;
        return err;
    }*/

    // The pid argument named a nonexistent process
    if(childp == NULL){
        err = ESRCH;
        return err;
    }

    // The pid argument named a process that was not a child of the current process.
    if(childp->p_parentpid != p->p_pid){
        err = ECHILD;
        return err;
    }

    // proctable[p->p_pid]->is_waiting = true;

    // Parent process waits for child one if it is no exited
    if(!(childp->is_exited)){
        P(&childp->p_waitsem); 
    }   

    *kstatus = childp->exitcode;

    // Remove the current thread
    // (otherwise proc_destroy exits with error numthreads != 0)
    //proc_remthread(curthread);
    // Destroy the pid process
    proc_destroy(childp);

    err = copyout(kstatus, (userptr_t)status, sizeof(int));
    if(err){
        kfree(kstatus);
        return err;
    }

    *retval = pid;

    return 0;
}

int sys__exit(int exitcode){

    //int i = 0;
    struct proc *p = curproc;
    //struct proc **pt = proctable;

    //P(&p->p_sem);

    pid_t pid = p->p_pid;
    //pid_t ppid = p->p_parentpid;

    // TODO: chiusura file se ce n'è aperti

    // Check the presence of curproc in proctable
    //P(&pt_sem);
    /*while((proctable[i]->p_pid != pid) && (i < MAX_PROCESSES)){
        i++;
    }
    if(i == MAX_PROCESSES){
        return 0;
    }*/
    //V(&pt_sem);

    p->exitcode = _MKWAIT_EXIT(exitcode);
    p->is_exited = true;

    //V(&p->p_sem);

    /*if(proctable[ppid]->is_waiting){
        // Signal the semaphore for waitpid
        V(&p->p_waitsem);

    }else{
        // Remove the current thread
        // (otherwise proc_destroy exits with error numthreads != 0)
        proc_remthread(curthread);

        // Destroy the pid process
        proc_destroy(proctable[pid]);

        // pid is now available
        proctable[pid] = NULL;

    }*/
    
    // pid is now available
    //P(&pt_sem);
    //proctable[pid] = NULL;
    //V(&pt_sem);
    V(&p->p_waitsem);
    // Cause the current thread to exit
    //proc_remthread(curthread); //o qui o dentro thread_exit TODO
    //proc_destroy(proctable[pid]);
    proctable[pid] = NULL;
    thread_exit(); // is zombie (pointer is NULL but proc is still in memory)

    return 0;
}