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

/*
 * Duplicates the currently running process.
 */
int sys_fork(struct trapframe *tf, pid_t *retval){

    int err;
    struct proc *p = curproc; // parent process
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

    // Copy parent's filetable (using each file semaphore)
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

/*
 * Returns the process id of the current process.
 */
int sys_getpid(pid_t *retval){

    struct proc *p = curproc;

    P(&p->p_sem);

    *retval = curproc->p_pid;

    V(&p->p_sem);

    return 0;
}

/*
 * Wait for the process specified by pid to exit,
 * and return an encoded exit status in the integer pointed to by status.
*/
int sys_waitpid(pid_t pid, int *status, int options, pid_t *retval){
    int err;
    struct proc *p = curproc;
    struct proc *childp;

    // Get child process from proctable
    P(&pt_sem);
    childp = proctable[pid];
    V(&pt_sem);

    // The options argument is not required (i.e. should be 0)
    if(options != 0){
        err = EINVAL;
        return err;
    }

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

    // Parent process waits for child one, if it is no exited
    if(!(childp->is_exited)){
        P(&childp->p_waitsem); 
    }   

    *status = childp->exitcode;

    // Destroy the pid process
    proc_destroy(childp);

    *retval = pid;

    return 0;
}

/*
 * Causes the current process to exit.
 */
int sys__exit(int exitcode){

    struct proc *p = curproc;
    pid_t pid = p->p_pid;

    // Store the passed exit code (readable by sys_waitpid)
    p->exitcode = exitcode;

    // Current process is ready to exit
    p->is_exited = true;

    // Process table is free
    proctable[pid] = NULL;

    // Signal sys_waitpid() calling process that its child is exited
    V(&p->p_waitsem);

    // Causes the current thread to exit by detaching it from process.
    // The process is made zombie (waiting for its removal but still in memory)
    thread_exit();

    return 0;
}

/*
 * Replaces the currently executing program with a newly loaded program image.
 * Note: It is very similar to runprogram() (that loads a program and start running it in usermode)
 *       but with some modifications about the arguments passing
 */
int sys_execv(char *program, char **args){

    int err, argc=0, padding=0;
    int args_size=0; // Total size of kernel buffer (args size + pointers size)
    struct vnode *vn;
    struct addrspace *as;
    vaddr_t entrypoint, stackptr;
    size_t arglen=0;
    char **kargs; // Kernel buffer
    char *kargs_ptr; // To move inside kernel buffer (char by char)
    char *kargs_ptr_start; // Starting position of arguments inside kernel buffer

    // Check arguments validity
    if((program == NULL) || (args == NULL)) {
		return EFAULT;
	}

    /* 1. Compute argc */

    // The n. of elements of args[] is unknown but the last argument should be NULL.
    // Compute argc = n. of valid arguments in args[]
    while(args[argc] != NULL){
        // Accumulate the size of each args[] element
        arglen = strlen(args[argc])+1;
        args_size += sizeof(char)*(arglen);
        // Compute n. of 0s for padding (if needed)
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

    // - Single arguments with their pointers

    // Update the tot size to can insert the pointers
    args_size = args_size + sizeof(char *)*(argc+1);

    // Allocate the kernel buffer (choose a pointer inside kernel heap)
    kargs = (char **)kmalloc(args_size);

    // The starting position is over the arg pointers
    kargs_ptr_start = (char *)(kargs + argc + 1); // e.g. A + 16

    // Initialize the pointer to move inside kernel buffer
    kargs_ptr = kargs_ptr_start;

    // Fill the kernel buffer
    for(int i=0;i<argc;i++){ // e.g. the args are "foo\0" "hello!\0" "1\0"
        
        // - Copy the arguments pointers into kernel buffer
        kargs[i] = kargs_ptr;

        // - Copy the arguments into kernel buffer
        err = copyinstr((const_userptr_t)args[i], kargs_ptr, ARG_MAX, &arglen); // e.g. arglen = 4, 7, 2
        if(err){
            return err;
        }

        // Move pointer on the copied argument
        kargs_ptr += arglen;

        // If the argument string has no exactly 4 bytes 0-padding is needed
        if((arglen%4) != 0){
            // n. of bytes to pad
            padding = 4 - arglen%4;
            for(int j=0;j<padding;j++){
                // Move on the pointer by following the 0s added
                kargs_ptr += j; 
                // Add the \0
                memcpy(kargs_ptr,"\0",1);
                //strcat(kargs_ptr,"\0"); // concatenate a \0 char
            }
        }
    }
    // Now kargs[] is the copy of args in kernel space with the proper padding
    // and kargs_ptr points to the last char of kernel buffer

    // Set the last pointer to null
    kargs[argc] = NULL;

    /* 3. Create a new address space and load the executable into it
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

    /* 4. Copy the kernel buffer into stack (remember to update the pointers) */

    // Start position of the stack (= allocate into stack the same size of kernel buffer to copy)
    stackptr -= args_size;

    // Update the pointers in the kernel buffer with the starting stack position
    for(int i=0;i<argc;i++){
        kargs[i] = kargs[i] - kargs_ptr_start + (char *)stackptr + sizeof(char*)*(argc+1);
    }

    // Copy the whole kernel buffer into user stack
    copyout(kargs, (userptr_t)stackptr, args_size);

    enter_new_process(argc /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,
			  NULL /*userspace addr of environment*/,
			  stackptr, entrypoint);

    // enter_new_process does not return
	panic("enter_new_process in execv returned\n");

    err = EINVAL;

    return err;
}