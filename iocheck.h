#ifndef	__IOCHECK_H__
#define	__IOCHECK_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <mcheck.h>
#include <linux/types.h>
#include <linux/falloc.h>
#include <errno.h>

/**************************************************************************************************************/
#if 1
#define	handle_error_en(en, msg) \
	do { \
		char extmsg[256]; \
		snprintf(extmsg, sizeof(extmsg), "%s, %s(), %d, %s", __FILE__, __func__, __LINE__, msg); \
		errno = en; \
		perror(extmsg); \
		exit(EXIT_FAILURE); \
	} while (0)

#define	handle_error(msg) \
	do { \
		char extmsg[256]; \
		snprintf(extmsg, sizeof(extmsg), "%s, %s(), %d, %s", __FILE__, __func__, __LINE__, msg); \
		perror(extmsg); \
		exit(EXIT_FAILURE); \
	} while (0)
#else
#define handle_error_en(en, msg)\
	do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while (0)
#endif

/**************************************************************************************************************/
#define	ALIGN(x, y)	(((x) / (y)) * (y))
#define	rand64()	((lrand48() & 0xFFFF) | ((lrand48() & 0xFFFFF) << 16) | ((lrand48() & 0xFFFFF) << 32) | ((lrand48() & 0xFFFFF) << 48))

/**************************************************************************************************************/
#define print(x...)     	do { printf(x); fflush(stdout); } while(0)
#define	perror_exit(x...)	do { print(x); perror(" "); exit(EXIT_FAILURE); } while(0) // perror() is thread-safe

#define move_up(n)      do { printf("\033[%dA", n); } while(0)  // move cursor up n lines
#define move_down(n)    do { printf("\033[%dB", n); } while(0)  // move cursor down n lines
#define move_right(n)   do { printf("\033[%dC", n); } while(0)  // move cursor right n column
#define move_left(n)    do { printf("\033[%dD", n); } while(0)  // move cursor left n column
#define moveto_head()   do { printf("\r"); } while(0)           // move cursor to line head
#define clear_line()    do { printf("\033[K"); } while(0)       // clear display from cursor to line tail

#define save_cursor()   do { printf("\033[s"); } while(0)       // save current cursor location
#define load_cursor()   do { printf("\033[u"); } while(0)       // restore cursor location saved by save_cursor()

#define clear_screen()  do { printf("\033[2J"); } while(0)      // clear all display on the screen
#define hide_cursor     do { printf("\033[?25l"); } while(0)    // hide cursor
#define show_cursor     do { printf("\033[?25h"); } while(0)    // show cursor
#define set_cursor(x,y) do { printf("\033[%d;%dH", y, x); } while(0)    // set cursor to coordinate [x, y]

/**************************************************************************************************************/
#define	IOCHECK_WRITE	0
#define	IOCHECK_READ	1

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

	int nthread;
	struct io_thread *thread;
	size_t thread_stacksize;
	__u64 seq_num[512];	// per-thread
	__u8 runrandom[512];	// per-thread

	size_t thread_exit_offset[512];
	size_t thread_exit_length[512];

	int fulldata_cmp;
	int is_rawdev;
	int blkdev_logicbz;	// io_submit must be align
	int raw_blkdev_logicbz;
	off_t free_space;
	off_t ck_goffset;
	long mb_datamem_needed;

	int skip_ck_memsize;
	int loops;

	FILE *perline_fp;

	int ismem;

	int interval;
	int reporting_perline;

	int pagesize;		// system pagesize
	int mb_memory;		// system memory size in mb_memory

	off_t uniq_filesize;
	int each_filenum;
	off_t each_filesize[512];
	off_t total_filesize;

	int each_duty_on[512];
	int each_duty_off[512];
	int each_dutynum;

	int bz_method;
	int fix_bz;
	int min_bz;
	int max_bz;

#define	RW_ENDURE_USER_MASK		0x01
#define	RW_ENDURE_USER_WR_RAND_MASK	0x02
#define	RW_ENDURE_USER_RD_RAND_MASK	0x04
	int rw_endure;
	int rw_endure_wr;
	int rw_endure_rd;
	int pad_file;
	int reinstall_check;
	int restore_check;
	int purely_check;
	int disable_mismatch_exit;
	long max_time;

	int seq_write;
	int set_logbz;

	/* add here */
	int sigint;
	int sigint_ecode;
	int discard;
	int batch_discard_interval;
	int batch_discard_sizeratio;
	__s64 max_latency;
#define MAX_SKIP_LBA_COUNT	(8*1024*1024)
	FILE *skip_check_lba_fp;
	size_t *skip_lba;
	int skip_lba_count;

	int stop;
	int ready;
	int complete_nthread;
	pthread_mutex_t ready_mutex;
	pthread_cond_t ready_cond;

	int running_count;
	char running_char[4];

	char *automode;
	int ignore_save;

	int immediate_read;

	__u64 total_time;

	// debug option
	int skip_verify;
	int disable_dynamic_alloc;
	int thread_exit;
};

/**************************************************************************************************************/
extern int parse_cmdline(struct io_check *check, int argc, char **argv);

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
/**************************************************************************************************************/
#endif /* __IOCHECK_H__ */
