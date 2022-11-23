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
 * VFS operations involving the current directory.
 */

#include <types.h>
#include <kern/errno.h>
#include <stat.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <vfs.h>
#include <fs.h>
#include <vnode.h>

/*
 * Get current directory as a vnode.
 */
int
vfs_getcurdir(struct vnode **ret)
{
	int rv = 0;

	spinlock_acquire(&curproc->p_lock);
	if (curproc->p_cwd!=NULL) {
		VOP_INCREF(curproc->p_cwd);
		*ret = curproc->p_cwd;
	}
	else {
		rv = ENOENT;
	}
	spinlock_release(&curproc->p_lock);

	return rv;
}

/*
 * Set current directory as a vnode.
 * The passed vnode must in fact be a directory.
 */
int
vfs_setcurdir(struct vnode *dir)
{
	struct vnode *old;
	mode_t vtype;
	int result;

	result = VOP_GETTYPE(dir, &vtype);
	if (result) {
		return result;
	}
	if (vtype != S_IFDIR) {
		return ENOTDIR;
	}

	VOP_INCREF(dir);

	spinlock_acquire(&curproc->p_lock);
	old = curproc->p_cwd;
	curproc->p_cwd = dir;
	spinlock_release(&curproc->p_lock);

	if (old!=NULL) {
		VOP_DECREF(old);
	}

	return 0;
}

/*
 * Set current directory to "none".
 */
int
vfs_clearcurdir(void)
{
	struct vnode *old;

	spinlock_acquire(&curproc->p_lock);
	old = curproc->p_cwd;
	curproc->p_cwd = NULL;
	spinlock_release(&curproc->p_lock);

	if (old!=NULL) {
		VOP_DECREF(old);
	}

	return 0;
}

/*
 * Set current directory, as a pathname. Use vfs_lookup to translate
 * it to a vnode.
 */
int
vfs_chdir(char *path)
{
	struct vnode *vn;
	int result;
	char tmppath[PATH_MAX+1];

	strcpy(tmppath,path);

	result = vfs_lookup(path, &vn);
	if (result) {
		return result;
	}
	result = vfs_setcurdir(vn);
	VOP_DECREF(vn);
	if(result){
		return result;
	}

	set_cwd(tmppath);

	return 0;
}

/*
 * Set current directory's pathname. Change the p_cwdpath field of the proc struct.
 * It consider both the cases: abosolute and relative paths.
 */

int
set_cwd(char *pathname){

	int len, i;
    char tmpcwd[PATH_MAX+1], checkpath[PATH_MAX+1], tmppath[PATH_MAX+1];
    struct proc *p = curproc;
    char *context;

	// strtok_r modifies it
	strcpy(checkpath, pathname);
	// 
	strcpy(tmppath, pathname);
	//
	strcpy(tmpcwd,p->p_cwdpath);

	// if the absolute path start with "/"
	if(checkpath[0]=='/'){

		strcpy(tmpcwd,"emu0:");
		strcat(tmpcwd,pathname);
		strcpy(p->p_cwdpath,tmpcwd);
		
		
	// if pathname starts with "emu0:" is an absolute path
	}else if(strcmp(strtok_r(checkpath,":",&context),"emu0") == 0){ // absolute path 

			strcpy(p->p_cwdpath, tmppath);

	}else{

		// previous directory case
		while(strcmp(strtok_r(checkpath,"/",&context),"..") == 0){
			// remove "../" from tmppath
			len = strlen(tmppath);
			for(i=0; i<=(len-3);i++){
				tmppath[i] = tmppath[i+3];
			}
			tmppath[i] = 0; //null termination
			// remove the last directory from p_cwdpath
			len = strlen(tmpcwd);
			i = 0;
			while(tmpcwd[len-i] != '/'){
				tmpcwd[len-i] = 0;
				i++;
			}
			tmpcwd[len-i] = 0; // Remove also the /

			// re-start from context (the right part of the string)
			strcpy(checkpath,context);
			if(checkpath[0] == 0){
				// Exit condition for while loop (/ is needed to avoid strtok_r failure)
				strcpy(checkpath,"void/");
				//strcpy(tmppath,"emu0:")
			}
		}
		// in all cases: concatenation and updating of cwd path
		if(tmppath[0] != 0){ // no previuos directory case (tmppath has been not emptied)
			if(!((tmppath[0] == '/') || (tmpcwd[strlen(tmpcwd)] == '/'))){
				strcat(tmpcwd,"/");		//	emu0:	+	/
				strcat(tmpcwd,tmppath);	//	emu0:/	+	mytest
			}else{
				strcat(tmpcwd,tmppath);	//	emu0:/	+	mytest (or viceversa)
			}
		}
		strcpy(p->p_cwdpath,tmpcwd);
	}
	return 0;
}

/*
tmpcwd	tmppath	(null)	slash?
0		0		0		1		emu0:  /  mytest
0		0		1		0		emu0:     null (empty because ../ case)
0		1		0		0		emu0:     /mytest
0		1		1		0		emu0:     null (like bove)
1		0		0		0		emu0:/    mytest
1		0		1		0		emu0:/    null (it is an error format)
1		1		0		0		emu0:/    /mytest (it is an error format)

=> when tmppath is null ( ../ case) slash is no needed (first if statement)

tmpcwd	tmppath	slash?
0		0		1		emu0:  /  mytest
0		1		0		emu0:     /mytest
1		0		0		emu0:/    mytest
1		1		0		emu0:/    /mytest (it is an error format)

This is a nor conditon: not(tmpcwd or tmppath)

ALLOWED SYNTAX TO MOVE AMONG DIRECTORIES:
---------------------------cwd = emu0: (i.e. root)
cd emu0:/mytest
cd /mytest
cd mytest
---------------------------cwd = em0:/include/kern
../../ (to root)
../../mytest (to root/mytest)
TO DO: (working on vnode but not yet on string)
/mytest (to root/mytest because before slash is automatically considered the root emu0:), see NOTA
../.. (to root)
.. (to include)

NOTA:
Aggiungere else if per caso /mytest
in cui dobbiamo concatenare per avere 
il path assoluto
*/

/*
 * Get current directory, as a pathname.
 * Use VOP_NAMEFILE to get the pathname and FSOP_GETVOLNAME to get the
 * volume name.
 */
int
vfs_getcwd(struct uio *uio)
{
	struct vnode *cwd;
	int result;
	const char *name;
	char colon=':';

	KASSERT(uio->uio_rw==UIO_READ);

	result = vfs_getcurdir(&cwd);
	if (result) {
		return result;
	}

	/* The current dir must be a directory, and thus it is not a device. */
	KASSERT(cwd->vn_fs != NULL);

	name = FSOP_GETVOLNAME(cwd->vn_fs);
	if (name==NULL) {
		vfs_biglock_acquire();
		name = vfs_getdevname(cwd->vn_fs);
		vfs_biglock_release();
	}
	KASSERT(name != NULL);

	if((strcmp(curproc->p_cwdpath,"emu0:") == 0)||(strcmp(curproc->p_cwdpath,"emu0:/") == 0)){
		result = uiomove((char *)name, strlen(name), uio);
		if (result) {
			goto out;
		}
		result = uiomove(&colon, 1, uio);
		if (result) {
			goto out;
		}
	}

	result = VOP_NAMEFILE(cwd, uio);

 out:

	VOP_DECREF(cwd);
	return result;
}
