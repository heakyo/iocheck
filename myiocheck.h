#ifndef __MY_IOCHECK_H__
#define __MY_IOCHECK_H__

#define _REENTRANT
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
#include <pthread.h>

#define	CHECK_CONTEXT_TAG	"iocheck-context-file"

struct io_thread {
	int index;
	pthread_t tid;

	char filename[256];
	char data_filename[256];
	char errbuf[256];

	union {
		int fd;
		void *mem;
	};
	int data_fd;
	off_t file_size;	// file size in Byte
	off_t file_logicbs;	// files in blkdev_logicbz
	off_t goffset;		// used for raw block device

	__u64 *data;
	__u64 seq_num;

	__u64 write_length;
	__u64 read_length;
	__u64 valid_length;
	__u64 discard_length;

#define	INVALID_BZ	0
#define	RANDOM_BZ	1
#define	FIXED_BZ	2
	int bz_method;
	int fix_bz;		// units: blkdev_logicbz
	int min_bz;
	int max_bz;

	volatile int terminate;

	int iodepth;
	int nrequest;

	/* add here */
	int immediate_read;
	long discard_count;

	int duty_on;
	int duty_off;

	struct io_check *check;
};

struct io_check {
	char id[64];

	char target[PATH_MAX];
	char data_directory[PATH_MAX];
	char context_filename[PATH_MAX];

	int is_rawdev;
	int blkdev_logicbz;	// io_submit must be align
	int raw_blkdev_logicbz;
	off_t free_space;
	off_t ck_goffset;
	int nthread;

#define	INVALID_BZ	0
#define	RANDOM_BZ	1
#define	FIXED_BZ	2
	int bz_method;
	int fix_bz;

	off_t uniq_filesize;
	int each_filenum;
	off_t each_filesize[512];
	off_t total_filesize;

	char *automode;

	int running_count;
	char running_char[4];

	long mb_datamem_needed;

	int skip_ck_memsize;
	int pagesize;		// system pagesize
	int mb_memory;		// system memory size in mb_memory

	pthread_mutex_t ready_mutex;
	pthread_cond_t ready_cond;

	struct io_thread *thread;
	__u64 seq_num[512];	// per-thread
	__u8 runrandom[512];	// per-thread
	int min_bz;
	int max_bz;

	int each_duty_on[512];
	int each_duty_off[512];
	int each_dutynum;

	int fulldata_cmp;
	size_t thread_stacksize;
};

/******************************************************************/
int parse_cmdline(struct io_check *check, int argc, char **argv);

static char size_buf[256];
static inline char *human_size(off_t filesize)	// XXX: not thread-safe
{
	int n = 0;
	char p[2] = {0, 0};

	memset(size_buf, 0, sizeof(size_buf));

	if (filesize >= (1024 * 1024 * 1024)) {
		n += sprintf(size_buf + n, "%ldG", filesize / (1024 * 1024 * 1024));
		filesize %= (1024 * 1024 * 1024);
		p[0] = '.';
	}

	if (filesize >= (1024 * 1024)) {
		n += sprintf(size_buf + n, "%s%ldM", p, filesize / (1024 * 1024));
		filesize %= (1024 * 1024);
		p[0] = '.';
	}

	if (filesize >= 1024) {
		n += sprintf(size_buf + n, "%s%ldK", p, filesize / 1024);
		filesize %= 1024;
		p[0] = '.';
	}

	if (filesize)
		n += sprintf(size_buf + n, "%s%ld", p, filesize);

	assert(n < sizeof(size_buf));
	return size_buf;
}
#endif
