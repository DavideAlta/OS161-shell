#
# Makefile for src/testbin (sources for programs installed in /testbin)
#

TOP=../..
.include "$(TOP)/mk/os161.config.mk"

SUBDIRS=add argtest badcall bigexec bigfile bigfork bigseek bloat conman \
	crash ctest dirconc dirseek dirtest f_test factorial farm faulter \
	filetest forkbomb forktest frack hash hog huge \
	malloctest matmult multiexec palin parallelvm poisondisk psort \
	randcall redirect rmdirtest rmtest \
	sbrktest schedpong sort sparsefile tail tictac triplehuge \
	triplemat triplesort usemtest zero \
	sysfork test_execv test_process test_file_rw \
	test_file_duplseek test_file_dir helloworld helloworld_args

# But not:
#    userthreads    (no support in kernel API in base system)

.include "$(TOP)/mk/os161.subdir.mk"
