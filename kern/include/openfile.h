#ifndef _OPENFILE_H_
#define _OPENFILE_H_

#include <synch.h>
#include <vnode.h>
#include <limits.h> // for OPEN_MAX constant used inside proc.h

struct vnode;

/*
 *   A struct to manage the open files.
 */
struct openfile {
    struct vnode *of_vnode;     /* Pointer to locate the data */
    int of_flags;     /* How to open a file (O_READ, O_WRITE, etc.) */
	int of_offset;     /* File offset */
	struct semaphore *of_sem;     /* Semaphore for openfile */
	int of_refcount;     /* Reference count */	
};

#endif /*_OPENFILE_H_*/
