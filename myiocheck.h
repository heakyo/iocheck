#ifndef __MY_IOCHECK_H__
#define __MY_IOCHECK_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define	CHECK_CONTEXT_TAG	"iocheck-context-file"

struct io_check {
	char id[64];

	int running_count;
	char running_char[4];

	size_t thread_stacksize;
};

/******************************************************************/
int parse_cmdline(struct io_check *check, int argc, char **argv);

#endif
