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

#include <types.h>
#include <kern/errno.h>
#include <kern/syscall.h>
#include <lib.h>
#include <mips/trapframe.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>
#include <addrspace.h>
#include <copyinout.h>


/*
 * System call dispatcher.
 *
 * A pointer to the trapframe created during exception entry (in
 * exception-*.S) is passed in.
 *
 * The calling conventions for syscalls are as follows: Like ordinary
 * function calls, the first 4 32-bit arguments are passed in the 4
 * argument registers a0-a3. 64-bit arguments are passed in *aligned*
 * pairs of registers, that is, either a0/a1 or a2/a3. This means that
 * if the first argument is 32-bit and the second is 64-bit, a1 is
 * unused.
 *
 * This much is the same as the calling conventions for ordinary
 * function calls. In addition, the system call number is passed in
 * the v0 register.
 *
 * On successful return, the return value is passed back in the v0
 * register, or v0 and v1 if 64-bit. This is also like an ordinary
 * function call, and additionally the a3 register is also set to 0 to
 * indicate success.
 *
 * On an error return, the error code is passed back in the v0
 * register, and the a3 register is set to 1 to indicate failure.
 * (Userlevel code takes care of storing the error code in errno and
 * returning the value -1 from the actual userlevel syscall function.
 * See src/user/lib/libc/arch/mips/syscalls-mips.S and related files.)
 *
 * Upon syscall return the program counter stored in the trapframe
 * must be incremented by one instruction; otherwise the exception
 * return code will restart the "syscall" instruction and the system
 * call will repeat forever.
 *
 * If you run out of registers (which happens quickly with 64-bit
 * values) further arguments must be fetched from the user-level
 * stack, starting at sp+16 to skip over the slots for the
 * registerized values, with copyin().
 */
void
syscall(struct trapframe *tf)
{
	int callno;
	int32_t retval;
	int64_t arg64, retval64;
	int extrarg; // to get a parameter from userl-level stack
	int err;

	bool is_retval64 = false; // if the retval is on 64 bit

	KASSERT(curthread != NULL);
	KASSERT(curthread->t_curspl == 0);
	KASSERT(curthread->t_iplhigh_count == 0);

	callno = tf->tf_v0;

	/*
	 * Initialize retval to 0. Many of the system calls don't
	 * really return a value, just 0 for success and -1 on
	 * error. Since retval is the value returned on success,
	 * initialize it to 0 by default; thus it's not necessary to
	 * deal with it except for calls that return other values,
	 * like write.
	 */

	retval = 0;

	switch (callno) {
	    case SYS_reboot:
		err = sys_reboot(tf->tf_a0);
		break;

	    case SYS___time:
		err = sys___time((userptr_t)tf->tf_a0,
				 (userptr_t)tf->tf_a1);
		break;

	    /* Add stuff here */

		case SYS_fork:
		err = sys_fork(tf,			// trapframe of calling process
						&retval);	// retval: child pid in parent and 0 in child
		break;

		case SYS_getpid:
		err = sys_getpid(&retval);	// retval: current process pid
		break;

		case SYS_waitpid:
		err = sys_waitpid((pid_t)tf->tf_a0,	// pid of process to wait for
						  (int *)tf->tf_a1,			// exit status of the waited process
						  (int)tf->tf_a2,	// options (should be 0, not implemented)
						  &retval);			// retval: pid of process to wait for (same of 1st arg)
		break;

		case SYS__exit:
		err = sys__exit((int)tf->tf_a0);  // exit code
		// retval is 0 (default)
	    break;

		case SYS_open:
		err = sys_open((userptr_t)tf->tf_a0,	// filename
					   (int)tf->tf_a1,			// flags
					   &retval);				// retval = file descriptor	
		break;

		case SYS_write:
		err = sys_write((int)tf->tf_a0,			// fd
						(userptr_t)tf->tf_a1,	// buf to write on file
						(size_t)tf->tf_a2,		// buf len
						&retval);				// retval = written nbytes
		break;

		case SYS_read:
		err = sys_read((int)tf->tf_a0,			// fd
					   (userptr_t)tf->tf_a1,	// buf
					   (size_t)tf->tf_a2,		// size (in byte)
				   	   &retval);				// retval = read nbytes
		break;

		case SYS_close:
		err = sys_close((int)tf->tf_a0);
		break;

		case SYS_lseek:
		/* If you run out of registers (which happens quickly with 64-bit
 		* values, i.e. pos argument) further arguments must be fetched 
		* from the user-level stack, starting at sp+16 to skip over the
		* slots for the registerized values, with copyin()*/
		err = copyin((userptr_t)(tf->tf_sp + 16), &extrarg, sizeof(int));
		/* 64-bit arguments are passed in *aligned*
 		 * pairs of registers, that is, either a0/a1 or a2/a3. This means that
 		 * if the first argument is 32-bit and the second is 64-bit, a1 is
 		 * unused.*/
		arg64 = tf->tf_a2;
		arg64 = arg64 << 32; // tf_a2 on MSBs
		arg64 = arg64 | tf->tf_a3; // tf->tf_a3 on LSBs
		if(!err){
			err = sys_lseek((int)tf->tf_a0,			// fd
							(off_t)arg64,			// offset of the pointer in bytes (could be neg)
							extrarg,				// where the seek should be performed (flag)
							&retval64);				// retval = offset of the pointers
			is_retval64 = true;
		}
		break;

		case SYS_dup2:
		err = sys_dup2((int) tf->tf_a0,				// old fd
				   	   (int) tf->tf_a1,				// new fd
				   	   &retval);					// retval = new fd
		break;

		case SYS_chdir:
		err = sys_chdir((userptr_t)tf->tf_a0,		// pathname
						&retval);
		break;

		case SYS___getcwd:
		err = sys___getcwd((userptr_t)tf->tf_a0,	// buf: name of current dir
						   (size_t)tf->tf_a1,		// buflen: len of buf
						   &retval);				// retval: actual len of current dir 
		break;

		case SYS_execv:
		err = sys_execv((char *)tf->tf_a0,
						(char **)tf->tf_a1);
		// retval is 0 (default)				
		break;
	
		default:
		kprintf("Unknown syscall %d\n", callno);
		err = ENOSYS;
		break;
	}


	if (err) {
		/*
		 * Return the error code. This gets converted at
		 * userlevel to a return value of -1 and the error
		 * code in errno.
		 */
		tf->tf_v0 = err;
		tf->tf_a3 = 1;      /* signal an error */
	}
	else {
		/* Success. */
		if(is_retval64){
			tf->tf_v0 = (int32_t)(retval64 >> 32); // get 32 MSBs
			tf->tf_v1 = (int32_t)retval64; // get 32 LSBs
		}else{
			tf->tf_v0 = retval;
		}
		tf->tf_a3 = 0;      /* signal no error */
	}

	/*
	 * Now, advance the program counter, to avoid restarting
	 * the syscall over and over again.
	 */

	tf->tf_epc += 4;

	/* Make sure the syscall code didn't forget to lower spl */
	KASSERT(curthread->t_curspl == 0);
	/* ...or leak any spinlocks */
	KASSERT(curthread->t_iplhigh_count == 0);
}

/*
 * Enter user mode for a newly forked process.
 *
 * This function is provided as a reminder. You need to write
 * both it and the code that calls it.
 *
 * Thus, you can trash it and do things another way if you prefer.
 */
void
enter_forked_process(struct trapframe *tf, unsigned long addrspace)
{
	// Copy trapframe inside kernel stack
	struct trapframe childtf = *tf;

	struct proc *p = curproc;


	(void)addrspace; // not used argument

	// Activate the address space
	as_activate();

	(void)p;

	childtf.tf_v0 = 0;		// retval of the child process
	childtf.tf_a3 = 0;		// no error
	childtf.tf_epc += 4;	//child process begins by the next instruction

	mips_usermode(&childtf);
}
