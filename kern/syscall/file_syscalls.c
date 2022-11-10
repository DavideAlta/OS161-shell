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

int sys_open(userptr_t filename, int flags, int *retval){

    struct vnode *vn = NULL;
    struct proc *p = curproc; // useful for debug
    int err = 0;
    int append_mode = 0;
    int fd;
    char *kfilename; // filename string inside kernel space
    struct openfile *of_tmp;
    size_t filename_len=0;
    
    // Check arguments validity :
    
    // - filename was an invalid pointer
    if(filename == NULL){
        err = EFAULT;
        return err;
    }

    // Copy the filename string from user to kernel space to protect it
    filename_len = strlen((char *)filename) + 1;
    kfilename = kmalloc(filename_len);
    err = copyinstr(filename, kfilename, filename_len, NULL);
    if(err){
        return err;
    }

    // - Flags
    switch(flags){
        // TO DO: VERIFICARE TUTTE LE COMBINAZIONI DEI FLAG
        case O_RDONLY: break;
        case O_WRONLY: break;
        case O_RDWR: break;
        // Create the file if it doesn't exist (handled by vfs_open())
        case O_CREAT|O_WRONLY: break;
        case O_CREAT|O_RDWR: break;
        // Create the file if it doesn't exist, fails if doesn't exist (handled by vfs_open())
        case O_CREAT|O_EXCL|O_WRONLY: break;
        case O_CREAT|O_EXCL|O_RDWR: break;
        // Truncate the file to length 0 upon open (handled by vfs_open())
        case O_TRUNC|O_WRONLY: break;
        case O_CREAT|O_TRUNC|O_WRONLY: break;
        case O_TRUNC|O_RDWR: break;
        // Write at the end of the file
        case O_WRONLY|O_APPEND:
            append_mode = 1;
            break;
        case O_RDWR|O_APPEND:
            append_mode = 1;
            break;
        // EINVAL = flags contained invalid values
        default:
            err = EINVAL;
            return err;
            break;
    }

    // Check if the current thread and the associated process are no NULL
    KASSERT(curthread != NULL);
    KASSERT(curproc != NULL);

    // Obtain a vnode object associated to the passed file to open
    err = vfs_open(kfilename, flags, 0664, &vn);
    if(err)
        return err;
    
    // Find the index of the first free slot of the file table

    P(&p->p_sem);

    int i = STDERR_FILENO + 1;
    while((p->p_filetable[i] != NULL) && (i < OPEN_MAX)){
        i++;
    }
    // The process's file table was full
    if(i == OPEN_MAX){
        err = EMFILE;
        return err;
    }else{
        fd = i;
    }

    // Allocate and fill the found openfile struct
    of_tmp = (struct openfile *)kmalloc(sizeof(struct openfile));

    of_tmp->of_offset = 0;
    of_tmp->of_flags = flags;
    of_tmp->of_vnode = vn;
    of_tmp->of_refcount = 1;
    of_tmp->of_sem = sem_create("sem_file",1);

    // If append mode: the offset is equal to the file size
    if(append_mode){
        struct stat statbuf;
        // Get the file size by vnode object
		err = VOP_STAT(of_tmp->of_vnode, &statbuf);
		if (err){
			kfree(of_tmp);
			of_tmp = NULL;
			return err;
		}
		of_tmp->of_offset = statbuf.st_size;
    }

    // Load the full openfile inside the filetable
    p->p_filetable[fd] = of_tmp;
    // Check if the filetable is no more NULL
    KASSERT(p->p_filetable[fd] != NULL);

    V(&p->p_sem);

    // fd (i.e. retval) = Place of openfile inside the file table
    *retval = fd;

    return 0;
}

int sys_write(int fd, userptr_t buf, size_t buflen, int *retval){
    
    int err;
    //char kbuf[PATH_MAX];
    int offset; // tmp
    struct iovec iov;
    struct uio u;
    struct openfile *of; // tmp
    struct proc *p = curproc; // tmp

    KASSERT(curthread != NULL);
    KASSERT(curproc != NULL );

    // Copy the buffer from user to kernel space
    /*err = copyinstr(buf, kbuf, sizeof(kbuf), NULL);
    if(err){
        kfree(kbuf);
        return err;
    }*/

    of = p->p_filetable[fd];

    // Synchronization of writing operations (the file must be locked during writing)
    P(of->of_sem);

    // Check arguments validity :

    // - fd is not a valid file descriptor, or was not opened for writing
    if(fd < 0 || fd >= OPEN_MAX || of == NULL ||
       (!(((of->of_flags&O_WRONLY) == O_WRONLY) ||
       ((of->of_flags&O_RDWR) == O_RDWR)))){
        err = EBADF;
        return err;
    }

    // - Part or all of the address space pointed to by buf is invalid
    if(buf == NULL){
        err = EFAULT;
        return err;
    }

    offset = of->of_offset;

    // Setup the uio record (use a proper function to init all fields)
	uio_uinit(&iov, &u, buf, buflen+1, (off_t)offset, UIO_WRITE);
    u.uio_space = p->p_addrspace;
	u.uio_segflg = UIO_USERSPACE; // for user space address

    /*VOP_WRITE - Write data from uio to file at offset specified
                  in the uio, updating uio_resid to reflect the
                  amount written, and updating uio_offset to match.*/
	err = VOP_WRITE(of->of_vnode, &u);
	if (err){
		return err;
    }

    // offset update
    of->of_offset = u.uio_offset;
    // of->of_offset += u.uio_resid;

    // Synchronization of writing operations
    V(of->of_sem);

    // uio_resid is the amount written => retval
    *retval = u.uio_resid;

    return 0;
}

int sys_read(int fd, userptr_t buf, size_t size, int *retval)
{
    int err;
    int offset; // temporary variable to store the openfile field
    struct iovec iov;
    struct uio u;
    struct openfile *of; // tmp
    struct proc *p = curproc; // tmp

    KASSERT(curthread != NULL);
    KASSERT(curproc != NULL );

    of = p->p_filetable[fd];

    // Synchronization of reading operations (the file must be locked during reading)
    P(of->of_sem);
    
    // Check arguments validity :

    // fd is not a valid file descriptor, or was not opened for reading
    if(fd < 0 || fd >= OPEN_MAX || of == NULL ||
       (!(((of->of_flags&O_RDONLY) == O_RDONLY) ||
       ((of->of_flags&O_RDWR) == O_RDWR)))){
        err = EBADF;
        return err;
    }

    // - Part or all of the address space pointed to by buf is invalid
    if(buf == NULL){
        err = EFAULT;
        return err;
    }

    offset = of->of_offset;

    // Setup the uio record (use a proper function to init all fields)
	uio_uinit(&iov, &u, buf, size, offset, UIO_READ);
    u.uio_space = p->p_addrspace;
	u.uio_segflg = UIO_USERSPACE; // for user space address

    /* VOP_READ - Read data from file to uio, at offset specified
                  in the uio, updating uio_resid to reflect the
                  amount read, and updating uio_offset to match.*/
	err = VOP_READ(of->of_vnode, &u);
	if (err) {
		return err;
	}

    // uio_resid is the remaining byte to read => retval (the read bytes) is size-resid
    *retval = size - u.uio_resid;
    // offset update
    of->of_offset = u.uio_offset;

    // Synchronization of reading operations
    V(of->of_sem);

    return 0;
}

int sys_close(int fd){

    int err;
    struct openfile *of; // tmp
    struct proc *p = curproc; // tmp

    KASSERT(curthread != NULL);
    KASSERT(curproc != NULL );
    
    of = p->p_filetable[fd];
    
    P(of->of_sem);

    // Check arguments validity:

    // - fd is not a valid file handle
    if(fd < 0 || fd >= OPEN_MAX || of == NULL){
        err = EBADF;
        return err;
    } 

    // Delete the openfile structure only if it is the last open
    
    of->of_refcount--;

    if(of->of_refcount == 0){ // Last open => openfile removal
        vfs_close(of->of_vnode);
        V(of->of_sem);
        sem_destroy(of->of_sem);
        kfree(of);

    }else{ // No last open (no removal)
        V(of->of_sem);
    }

    curproc->p_filetable[fd] = NULL;

    return 0;
}