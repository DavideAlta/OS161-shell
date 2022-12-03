/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than runprogram() does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/unistd.h> 
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <openfile.h>
#include <copyinout.h>
#include <limits.h>

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, char** args)
{

	/*
	 * from menu.c we know that the size of args is 2
	 * 		args[0] = progname 
	 * 		args[1] = useful arg
    */

	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
	struct proc *p = curproc;
	int err;

	// Variables for args management 

	size_t arglen = 0;
	int args_size = 0;
	int padding = 0;
	char **kargs;
	char *karg1;

	/* 1. Compute the argument size (considering the padding) */
	arglen = strlen(args[1])+1;
	args_size = sizeof(char)*(arglen);
	// Compute n. of 0s for padding (if needed)
	if((arglen%4) != 0){
		padding = 4 - arglen%4;
		arglen += padding;
	}
	// The total size of the argument strings exceeeds ARG_MAX.
	if(args_size > ARG_MAX){
		err = E2BIG;
		return err;
	}

	/* 2. Copy from user to kernel */

	// - The single argument
	karg1 = kstrdup(args[1]);

	// - The program name
	char *kprogram = kstrdup(progname);
	if(kprogram==NULL){
        return ENOMEM;
    }

	/* 3. Create a new address space and load the executable into it */

	/* Open the file. */
	result = vfs_open(kprogram, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new process. */
	KASSERT(proc_getas() == NULL);

	/* Create a new address space. */
	as = as_create();
	if (as == NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	proc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}

	/* 4. Copy the argument from kernel to user stack */

	stackptr -= arglen; // computed at step 1

	kargs = (char**)(stackptr - 2*sizeof(char*));
	memcpy((char*)stackptr, karg1, arglen);
	kargs[0] = (char*)stackptr;
	kargs[1] = NULL;

	// Open the console files: STDIN, STDOUT and STDERR
	console_init(p);
	if(result){
		return result;
	}
	
	/* Warp to user mode. */
	enter_new_process(1 /*argc*/, (userptr_t)kargs /*userspace addr of argv*/,
			  NULL /*userspace addr of environment*/,
			  (vaddr_t)kargs, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

/*
 * Open the console files: STDIN, STDOUT and STDERR
 */
int
console_init(struct proc *proc)
{
	struct vnode *v_stdin, *v_stdout, *v_stderr;
	int result;
	char *kconsole = (char *)kmalloc(5);
	struct openfile *of_tmp;
	of_tmp = kmalloc(sizeof(struct openfile));

	strcpy(kconsole,"con:");

	/* Load STDIN openfile inside the file table */	
	result = vfs_open(kconsole, O_RDONLY, 0664, &v_stdin);
	if (result) {
		return result;
	}
	of_tmp->of_vnode = v_stdin;
	of_tmp->of_flags = O_RDONLY;
	of_tmp->of_offset = 0;
    of_tmp->of_refcount = 1;
    of_tmp->of_sem = sem_create("sem_stdin",1);

	proc->p_filetable[STDIN_FILENO] = of_tmp;

	of_tmp = NULL;
	of_tmp = kmalloc(sizeof(struct openfile));

	strcpy(kconsole,"con:"); // because vfs_open modify the name string

	/* Load STDOUT openfile inside the file table */	
	result = vfs_open(kconsole, O_WRONLY, 0664, &v_stdout);
	if (result) {
		return result;
	}
	of_tmp->of_vnode = v_stdout;
	of_tmp->of_flags = O_WRONLY;
	of_tmp->of_offset = 0;
    of_tmp->of_refcount = 1;
    of_tmp->of_sem = sem_create("sem_stdout",1);

	proc->p_filetable[STDOUT_FILENO] = of_tmp;

	of_tmp = NULL;
	of_tmp = kmalloc(sizeof(struct openfile));

	strcpy(kconsole,"con:");

	/* Load STDERR openfile inside the file table */	
	result = vfs_open(kconsole, O_WRONLY, 0664, &v_stderr);
	if (result) {
		return result;
	}
	of_tmp->of_vnode = v_stderr;
	of_tmp->of_flags = O_WRONLY;
	of_tmp->of_offset = 0;
    of_tmp->of_refcount = 1;
    of_tmp->of_sem = sem_create("sem_stderr",1);

	proc->p_filetable[STDERR_FILENO] = of_tmp;

	return 0;
}