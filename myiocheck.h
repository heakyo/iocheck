#ifndef __MY_IOCHECK_H__
#define __MY_IOCHECK_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>
#include <linux/fs.h>
#include <linux/limits.h>

#define	CHECK_CONTEXT_TAG	"iocheck-context-file"

struct io_check {
	char id[64];

	char target[PATH_MAX];
	char data_directory[PATH_MAX];
	char context_filename[PATH_MAX];

	int is_rawdev;
	int blkdev_logicbz;	// io_submit must be align
	int raw_blkdev_logicbz;
	off_t free_space;
	int nthread;

#define	INVALID_BZ	0
#define	RANDOM_BZ	1
#define	FIXED_BZ	2
	int bz_method;
	int fix_bz;

	off_t uniq_filesize;
	off_t total_filesize;
	char *automode;

	int running_count;
	char running_char[4];

	size_t thread_stacksize;
};

/******************************************************************/
int parse_cmdline(struct io_check *check, int argc, char **argv);

#endif
