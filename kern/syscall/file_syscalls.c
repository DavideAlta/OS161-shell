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

    int fd, err = 0, append_mode = 0;
    struct vnode *vn = NULL;
    struct proc *p = curproc;
    struct openfile *of_tmp;

    char *kfilename; // filename string inside kernel space

    // Check if the current thread and the associated process are no NULL
    KASSERT(curthread != NULL);
    KASSERT(curproc != NULL);
    
    // Check arguments validity :
    
    // - filename was an invalid pointer
    if(filename == NULL){
        err = EFAULT;
        return err;
    }

    // Copy the filename string from user to kernel space to protect it
    kfilename = kstrdup((char *)filename);
    if(kfilename==NULL){
        return ENOMEM;
    }

    // - Flags (all handled by vfs_open except for O_APPEND)
    switch(flags){
        case O_RDONLY: break;
        case O_WRONLY: break;
        case O_RDWR: break;
        // Create the file if it doesn't exist
        case O_CREAT|O_WRONLY: break;
        case O_CREAT|O_RDWR: break;
        // Create the file if it doesn't exist, fails if already exist
        case O_CREAT|O_EXCL|O_WRONLY: break;
        case O_CREAT|O_EXCL|O_RDWR: break;
        // Truncate the file to length 0 upon open
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

    // Obtain a vnode object associated to the passed file to open
    err = vfs_open(kfilename, flags, 0664, &vn);
    if(err)
        return err;

    // Process's semaphore to protect the process's file table
    P(&p->p_sem);

    // Search the first free slot of the file table, it will be the file descriptor
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

    // Allocate and fill properly the openfile struct
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

    // Load the filled openfile inside the filetable at the found position
    p->p_filetable[fd] = of_tmp;
    // Check if the position is no more NULL
    KASSERT(p->p_filetable[fd] != NULL);

    V(&p->p_sem);

    // Return the place of openfile inside the file table (file descriptor)
    *retval = fd;

    return 0;
}

int sys_write(int fd, userptr_t buf, size_t buflen, int *retval){
    
    int err, offset;
    struct iovec iov;
    struct uio u;
    struct openfile *of;
    struct proc *p = curproc;

    KASSERT(curthread != NULL);
    KASSERT(curproc != NULL );

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

    // Store the offset before writing
    offset = of->of_offset;

    // Setup the uio record (use a proper function to init all fields)
	uio_uinit(&iov, &u, buf, buflen, (off_t)offset, UIO_WRITE);
    u.uio_space = p->p_addrspace;
	u.uio_segflg = UIO_USERSPACE; // for user space address

    // Write data from uio to file at offset specified in the uio
    // updating uio_offset that will be at EOF position
	err = VOP_WRITE(of->of_vnode, &u);
	if (err){
		return err;
    }

    // Offset update
    of->of_offset = u.uio_offset;

    // Synchronization of writing operations
    V(of->of_sem);

    // write returns the amount actually written =
    // = offset after the writing - offset before writing (-1 due to EOF)
    *retval = u.uio_offset - offset - 1;

    return 0;
}

int sys_read(int fd, userptr_t buf, size_t size, int *retval)
{
    int err, offset;
    struct iovec iov;
    struct uio u;
    struct openfile *of;
    struct proc *p = curproc;

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

    // Store the offset before reading
    offset = of->of_offset;

    // Setup the uio record (use a proper function to init all fields)
	uio_uinit(&iov, &u, buf, size, offset, UIO_READ);
    u.uio_space = p->p_addrspace;
	u.uio_segflg = UIO_USERSPACE; // for user space address

    // Read data from file to uio at offset specified in the uio
    // updating uio_resid to reflect the amount read and updating uio_offset to match.
	err = VOP_READ(of->of_vnode, &u);
	if (err) {
		return err;
	}

    // read returns the actually read bytes =
    // = size - uio_resid (the remaining byte to read)
    *retval = size - u.uio_resid;
    
    // Offset update
    of->of_offset = u.uio_offset;

    // Synchronization of reading operations
    V(of->of_sem);

    return 0;
}

int sys_close(int fd){

    int err;
    struct openfile *of;
    struct proc *p = curproc;

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

    // Last open => openfile removal
    if(of->of_refcount == 0){
        vfs_close(of->of_vnode);
        V(of->of_sem);
        sem_destroy(of->of_sem);
        kfree(of);

    // No last open (no removal)
    }else{
        V(of->of_sem);
    }

    curproc->p_filetable[fd] = NULL;

    return 0;
}

int sys_lseek(int fd, off_t pos, int whence, int64_t *retval){

    int err, filesize;
    off_t offset;
    struct stat statbuf;
    struct proc *p = curproc;
    struct openfile *of = p->p_filetable[fd];

    KASSERT(curthread != NULL);
    KASSERT(curproc != NULL );

    P(of->of_sem);

    // Check fd validity

    // fd is not a valid file handle
    if(fd < 0 || fd >= OPEN_MAX || of == NULL){
        err = EBADF;
        return err;
    }

    // fd refers to an object which does not support seeking
    if((fd >= STDIN_FILENO) && (fd<=STDERR_FILENO)){
        err = ESPIPE;
        return err;
    } 

    // Retrieve the file size to can use it then
	err = VOP_STAT(of->of_vnode, &statbuf);
	if (err){
        return err;
    }else{
        filesize = statbuf.st_size;
    }

    // Check how is the flag passed as argument
    switch(whence){
        case SEEK_SET: // the new position is pos
            offset = pos;
        break;
        case SEEK_CUR: // the new position is the current position plus pos
            offset = of->of_offset + pos;
        break;
        case SEEK_END: // the new position is the position of end-of-file plus pos
            offset =  filesize + pos;
        break;

        default: // whence is invalid
            err = EINVAL;
            return err;
        break;
    }
    
    // The resulting seek position would be negative (or beyond file size)
    if((offset < 0) || (offset > filesize)){
        err = EINVAL;
        return err;
    }

    // Update the file offset
    of->of_offset = offset;

    V(of->of_sem);

    // Return the new offset
    *retval = offset;

    return 0;
}

int sys_dup2(int oldfd, int newfd, int *retval){

    int err;
    struct proc *p = curproc;
    struct openfile *oldof = p->p_filetable[oldfd];
    struct openfile *newof = p->p_filetable[newfd];

    KASSERT(curthread != NULL);
    KASSERT(curproc != NULL);

    // Check arguments validity:

    // - old/newfd is not a valid file handle
    if(oldfd < 0 || oldfd >= OPEN_MAX || oldof == NULL ||
       newfd < 0 || newfd >= OPEN_MAX ){
        err = EBADF;
        return err;
    }

    // using dup2 to clone a file handle onto itself has no effect
    if (newfd == oldfd){
        *retval = newfd;
        return 0;
    }

    // Check if the new fd is already associated to an open file, if yes close it
    if(newof != NULL){
        err = sys_close(newfd);
        if(err)
            return err;
    }

    // Copy the openfile pointer of the old fd inside the new fd and update the refcount
    curproc->p_filetable[newfd] = oldof;
    P(oldof->of_sem);
    curproc->p_filetable[oldfd]->of_refcount++;
    V(oldof->of_sem);

    // Return the new (cloned) file descriptor
    *retval = newfd;

    return 0;
}

int sys_chdir(userptr_t pathname, int *retval){

    int err;
    char *kpathname, tmppath[PATH_MAX+1];
    struct proc *p = curproc;
    KASSERT(curthread != NULL);
    KASSERT(p != NULL );

    // Check argument:

    // - pathname was an invalid pointer.
    if(pathname == NULL){
        err = EFAULT;
        return err;
    }

    // Copy the pathname string from user to kernel space to protect it
    kpathname = kstrdup((char *)pathname);
    if(kpathname==NULL){
        return ENOMEM;
    }

    // Useful because vfs_open changes the string
    strcpy(tmppath,kpathname);
    if(tmppath==NULL){
        return ENOMEM;
    }

    // Set the p_cwd vnode with the new one (together to pathname string p_cwdpath)
    err = vfs_chdir((char *)pathname);
    if(err){
        return err;
    }else{ 
        *retval = 0;
    }

    return 0;
}

int sys___getcwd(userptr_t buf, size_t buflen, int *retval){

    int err;
    struct iovec iov;
    struct uio u;

    KASSERT(curthread != NULL);
    KASSERT(curproc != NULL );

    // Check argument:

    // - buf points to an invalid address.
    if(buf == NULL){
        err = EFAULT;
        return err;
    }

    // Setup the uio record (use a proper function to init all fields)
	uio_uinit(&iov, &u, buf, buflen, 0, UIO_READ);
    u.uio_space = curproc->p_addrspace;
	u.uio_segflg = UIO_USERSPACE; // for user space address

    // Retrieve the uio struct associated with the current directory
    // (containing vnode and string with pathname)
    err = vfs_getcwd(&u);
    if(err)
        return err;

    // Actual lenght of the current pathname directory is returned
    *retval = buflen - u.uio_resid;

    if(*retval < 0){
        err = EFAULT;
    }

    return 0;
}