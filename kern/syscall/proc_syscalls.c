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

    // Child inherits the cwd by parent
    childproc->p_cwd = p->p_cwd;
    strcpy(childproc->p_cwdpath, p->p_cwdpath);

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

    *status = childp->exitcode;
    //TO DO: _MKWAIT_EXIT()

    // Remove the current thread
    // (otherwise proc_destroy exits with error numthreads != 0)
    //proc_remthread(curthread);
    // Destroy the pid process
    proc_destroy(childp);

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

    p->exitcode = exitcode;
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

/*
 * sys_execv() is very similar to runprogram() but with some modifications
 * Since runprogram() loads a program and start running it in usermode
*/

int sys_execv(char *program, char **args){

    int err;
    struct vnode *vn;
    struct addrspace *as;
    vaddr_t entrypoint, stackptr;
    size_t arglen=0;
    int padding=0;

    // Check arguments validity
    if((program == NULL) || (args == NULL)) {
		return EFAULT;
	}

    /* 1. Compute argc */

    // The n. of elements of args[] is unknown but the last argument should be NULL.
    // Compute argc = n. of valid arguments in args[]
    int argc = 0;
    int args_size = 0; // Total size of the argument strings
    while(args[argc] != NULL){
        // Accumulate the size of each args[] element
        arglen = strlen(args[argc])+1;
        args_size += sizeof(char)*(arglen);
        // Compute n. of 0s for padding
        if((arglen%4) != 0){
            padding = 4 - arglen%4;
            args_size += padding;
        }
        // The total size of the argument strings exceeeds ARG_MAX.
        if(args_size > ARG_MAX){
            err = E2BIG;
            return err;
        }
        argc ++;
    }
    // Now argc contains the number of valid arguments inside args[]
    // and arg_size contains the total size of all arguments (also considering padding zeros)

    /* 2. Copy arguments from user space into a kernel buffer. */

    // - Program path
    char *kprogram = kstrdup(program);
    if(kprogram==NULL){
        return ENOMEM;
    }

    // - Single arguments
    args_size += 4*(argc+1); // update the tot size with pointers dimensions
    char *kargs = (char *)kmalloc(args_size); // array of char (= 1 byte) [kernel buffer]
    // The starting position is over the arg pointers (each pointer is a 4bytes integer)
    int cur_pos = 4*(argc+1); // 16 (due to 3 args + 1 null)
    int arg_pointers[argc+1]; // Offsets of the arguments in the buffer
    // Suppose to have these arguments: "foo\0" "hello\0" "1\0"
    for(int i=0;i<argc;i++){
        // Move args to kernel space
        err = copyinstr((const_userptr_t)args[i], &kargs[cur_pos] /*(char*)(kargs+cur_pos)*/, ARG_MAX, &arglen); // arglen = 4 (foo\0) 6 (hello\0) 2 (1\0)
        if(err){
            return err;
        }
        arg_pointers[i] = cur_pos;
        cur_pos += arglen; // cur_pos =  16 > 20 > 26 > 30
        // if the argument string has no exactly 4 bytes '\0'-padding is needed
        if((arglen%4) != 0){
            // n. of bytes to pad at '\0'
            padding = 4 - arglen%4; // padding = 2 > 2
            for(int j=0;j<padding;j++){
                kargs[cur_pos + j] = '\0';
            }
            cur_pos += padding; // cur_pos = 28 > 32
        }
    }
    // Now kargs[] is the copy of args in kernel space with the proper padding

    /* Create a new address space and load the executable into it
     * (same of runprogram) */

    // Open the "program" file
    err = vfs_open(program, O_RDONLY, 0, &vn);
    if(err){
        return err;
    }

    // Create a new address space
	as = as_create();
	if(as == NULL) {
		vfs_close(vn);
		err = ENOMEM;
        return err;
	}

    // Change the current address space and activate it
	proc_setas(as);
	as_activate();

    // Load the executable "program"
	err = load_elf(vn, &entrypoint);
	if (err) {
		vfs_close(vn);
		return err;
	}

    // File is loaded and can be closed
	vfs_close(vn);

    // Define the user stack in the address space
	err = as_define_stack(as, &stackptr);
	if (err) {
		return err;
	}

    // Start position of the stack
    stackptr -= cur_pos;

    // Current position of the stack
    int stackpos;

    // Copy the arguments args in the stack
    for(int i=0;i<argc;i++){
        // Update current position of the stack (arg_pointers behave like offsets for stackptr)
        stackpos = stackptr + arg_pointers[i];
        // Copy i-th element of kargs array into the stack at the computed position
        // (the length, considering the padding, is provided by the distance between 2 offsets)
        copyout(&kargs[i], (userptr_t)stackpos, arg_pointers[i+1]-arg_pointers[i]);
    }

    // Update the pointers array with the starting stack position
    for(int i=0;i<argc;i++){
        arg_pointers[i] = arg_pointers[i] + stackptr;
    }
    // Copy the updated pointers from kernel buffer to user stack
    copyout(arg_pointers, (userptr_t)stackptr, 4*argc);
    // if no work kmalloccare

    // The last pointer is set to NULL
    /*stackptr += 4*argc; 
    *stackptr = NULL;
    stackptr -= 4*argc;*/


    enter_new_process(argc /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,
			  NULL /*userspace addr of environment*/,
			  stackptr, entrypoint);

    // enter_new_process does not return
	panic("enter_new_process in execv returned\n");

    err = EINVAL;

    return err;
}