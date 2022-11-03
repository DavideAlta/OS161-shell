# OS161 COMPILE NOTES

DOESN'T REPEAT ALL STEPS EVERY TIMES.

ONLY STEPS 3, 4 AND 5 MUST BE USED TO RE-COMPILE.

But if we add new files, and add them to conf.kern, all steps are needed !

----------------------
1. CONFIGURE:

Inside the folder kern/conf there are conf.kern and HELLO (HELLO is a file copied and renamed from GENERIC, in our case from DUMBVM to no have errors).

command: `./config HELLO`

2. DEPEND:

Go inside kern/compile/HELLO (new created folder thanks to previous config).

command: `bmake depend`

3. COMPILE/LINK:

Inside kern/compile/HELLO folder you launch the Makefile to compile all the OS161 files.

command: `bmake`

4. INSTALL:

If no errors with the compilation we proceed creating the new kernel file.

command: `bmake install`

5. LAUNCH OS161:

The previous command create a kernel-HELLO file inside os161/root folder, now we can launch OS161 with the new modifications.

command: `sys161 kernel-HELLO`

## OWN CUSTOM TEST
Insert in `userland/testbin/<mytest>` the `<mytest>.c` and its Makefile (copied by other folders in testbin).

For each modification: type bmake inside the userland folder (re-compile all the test).

note: In the folder userland/testbin we have inserted the test of every single syscall (sysfork, ...).









