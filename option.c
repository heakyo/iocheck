#define	_GNU_SOURCE	/* for *strerror_r() */
#define	__USE_GNU	/* for O_DIRECT */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <ctype.h>
#include <linux/fs.h>
#include <unistd.h>

#include "iocheck.h"

/**************************************************************************************************************/
#define	OPT_TARGET			'a'
#define	OPT_FILESIZE			'b'
#define	OPT_NUM_THREADS			'c'
#define	OPT_BLOCK_SIZE			'd'
#define	OPT_DATA_DIRECTORY		'e'
#define	OPT_PAD_FILE			'f'
#define	OPT_MAX_TIME			'g'
#define	OPT_REINSTALL_CHECK		'h'
#define	OPT_RESTORE_CHECK		'i'
#define	OPT_RW_ENDURE			'j'
#define	OPT_DISABLE_MISMATCH_EXIT	'k'
#define	OPT_SKIP_VERIFY			'l'
#define	OPT_DISABLE_DANYMIC_ALLOC	'm'
#define	OPT_SEQ_WRITE			'n'
#define	OPT_SKIP_CHECK_MEMSIZE		'o'
#define	OPT_LOOPS			'p'
#define	OPT_INTERVAL			'q'
#define	OPT_PERLINE			'r'
#define	OPT_SET_LOGBZ			's'
#define	OPT_IGNORE_SAVE			't'
#define	OPT_DUTY			'u'
#define	OPT_OFFSET			'v'
#define	OPT_FULLDATA_CMP		'w'
#define	OPT_IMMEDIATE_READ		'x'
#define	OPT_THREAD_EXIT			'y'
#define	OPT_HELP			'z'
#define	OPT_DISCARD			'A'
#define	OPT_BATCH_DISCARD_INTERVAL	'B'
#define	OPT_BATCH_DISCARD_SIZERATIO	'C'
#define	OPT_SIGINT_ECODE		'D'
#define	OPT_MAX_LATENCY			'E'
#define	OPT_SKIP_CHECK_LBA		'F'

static struct option lopts[] = {
	{"target", required_argument, NULL, OPT_TARGET},
	{"file-size", required_argument, NULL, OPT_FILESIZE},
	{"num-threads", required_argument, NULL, OPT_NUM_THREADS},
	{"block-size", required_argument, NULL, OPT_BLOCK_SIZE},
	{"data-directory", required_argument, NULL, OPT_DATA_DIRECTORY},
	{"fill-file", no_argument, NULL, OPT_PAD_FILE},
	{"max-time", required_argument, NULL, OPT_MAX_TIME},
	{"reinstall-check", required_argument, NULL, OPT_REINSTALL_CHECK},
	{"restore-check", no_argument, NULL, OPT_RESTORE_CHECK},
	{"rw-endure", required_argument, NULL, OPT_RW_ENDURE},
	{"disable-mismatch-exit", no_argument, NULL, OPT_DISABLE_MISMATCH_EXIT},
	{"help", no_argument, NULL, OPT_HELP},
	{"skip-verify", no_argument, NULL, OPT_SKIP_VERIFY},
	{"seq-write", no_argument, NULL, OPT_SEQ_WRITE},
	{"skip-check-memsize", no_argument, NULL, OPT_SKIP_CHECK_MEMSIZE},
	{"loops", required_argument, NULL, OPT_LOOPS},
	{"interval", required_argument, NULL, OPT_INTERVAL},
	{"perline", optional_argument, NULL, OPT_PERLINE},
	{"set-logbz", optional_argument, NULL, OPT_SET_LOGBZ},
	{"immediate-read", no_argument, NULL, OPT_IMMEDIATE_READ},
	{"ignore-save", no_argument, NULL, OPT_IGNORE_SAVE},
	{"thread-exit", no_argument, NULL, OPT_THREAD_EXIT},
	{"disable-dynamic-alloc", no_argument, NULL, OPT_DISABLE_DANYMIC_ALLOC},
	{"fulldata-cmp", no_argument, NULL, OPT_FULLDATA_CMP},
	{"offset", required_argument, NULL, OPT_OFFSET},
	{"duty", required_argument, NULL, OPT_DUTY},
	{"discard", required_argument, NULL, OPT_DISCARD},
	{"batch-discard-interval", required_argument, NULL, OPT_BATCH_DISCARD_INTERVAL},
	{"batch-discard-sizeratio", required_argument, NULL, OPT_BATCH_DISCARD_SIZERATIO},
	{"sigint-ecode", required_argument, NULL, OPT_SIGINT_ECODE},
	{"max-latency", required_argument, NULL, OPT_MAX_LATENCY},
	{"skip-check-lba", required_argument, NULL, OPT_SKIP_CHECK_LBA},
	{0, 0, 0, 0},
};

static void usage(void)
{
	printf("Description:\n");
	printf("\tTest integrity of media which is opened with 'O_DIRECT | O_SYNC'.\n");

	printf("Usage:\n");
	printf("\tiocheck <options>\n");

	printf("Options:\n");
	printf("\t--target=file/memXX\n"
		"\t\tTest target, which can be a directory or raw block device. Do memtest when taget='memXX', XX means memtest unit size, e.g. 512,4096 and son on,\n"
		"\t\tNOTE: memtest don`t support reinstall-check.\n");
	printf("\t--file-size=size or [thread0-filesize,thread1-filesize,...] or {auto|auto-A|auto-B|auto-C}\n"
		"\t\tTest filesize for each file. If use the format of '[]' the number shoud be equal with --num-threads. You could use m/M/g/G for MB/GB.\n"
		"\t\tauto, 1/2 1/4 1/8 ...\n"
		"\t\tauot-A, 1/num-threads\n"
		"\t\tauto-B, (0,1/2] (0,1/4] (0,1/8] ...\n"
		"\t\tauto-C, 4x(0,1M] 2x(1M,100M] 2x(100M,1G] 1/2 1/4 1/8 ... need 16 threads at least\n"
		"\t\tauto-D, num-threads/2 x (0,32M] num-threads/2 x 1/2 1/4 ...\n");
	printf("\t--num-threads=num\n"
		"\t\tThe number of thread you want to start. Each thread rw one file, which size is told by --filesize.\n");
	printf("\t--block-size=size or [min_size,max_size]\n"
		"\t\tRW block size each transfer, the format of '[]' means block-size is a random value between min_size and max_size. You could use k/K.\n");
	printf("\t--data-directory=dir\n"
		"\t\tTest tool will save written data for each file during writing, the option set its directory. If target is a directory, default is same as target.\n");
	printf("\t--fill-file\n"
		"\t\tOn ext4 filesystem, laying out file just allocates media space without actual writing to media. This option forces to fill file.\n");
	printf("\t--max-time=seconds\n"
		"\t\tTest will stop after max-time seconds, 0 means endless loop until you press 'ctrl+c' and this is the default.\n");
	printf("\t--seq-write\n"
		"\t\tBy default this tool does endless random rw. If select this option this tool only does sequential write file ignoring --rw-endure option then exit when\n"
		"\t\tcomplete, then you can use option --reinstall-check to check data integrity\n");
	printf("\t--reinstall-check=test-context-file\n"
		"\t\tCheck data integrity after reinstall, this option will ignore other options except for '--max-time'/'--rw_endure' and 'debug options' then uses options\n"
		"\t\tread from 'test-context-file'. When you press ctrl+c during testint the file 'test-context-file' named 'iotest.context' will be created automatically.\n");
	printf("\t--restore-check\n"
		"\t\tThis option shoud be used with '--reinstall-check'. By default, '--reinstall-check' will only read sequentially and check the integrity all file\n"
		"\t\tthen exit. If set this option, test tool will restore all data file context from last unstall then do write/read/check same as normal test.\n");
	printf("\t--rw-endure=WriteCnt[R/r]:ReadCnt[R/r]\n"
		"\t\tBy default, read/write for each thread will endure a random times between [1, 271] then switch to the reverse. This option without 'R/r' postfix\n"
		"\t\twill fix read/write to cnt times. And this option with 'R/r' postfix will set read/write endure a random times between [1, cnt]. NOTE: cnt can be 0.\n");
	printf("\t--disable-mismatch-exit\n"
		"\t\tBy default, test tool will abort if data mismatch is found. This option disable abort when found mismatch.\n");
	printf("\t--skip-check-memsize\n"
		"\t\tSkip check memset room when doinf swap test\n");
	printf("\t--loops=n\n"
		"\t\tStop test after write n * target_size\n");
	printf("\t--interval=N seconds\n"
		"\t\tInterval of report statistical info\n");
	printf("\t--perline[=FILENAME]\n"
		"\t\tReport statistical info perline. If filename is given, write report to this file\n");
	printf("\t--set-logbz=N\n"
		"\t\tUse this logical block size instead of actual\n");
	printf("\t--immediate-read\n"
		"\t\tRead immediately after every write to test flash imcomplete write\n");
	printf("\t--ignore-save\n"
		"\t\tDon't save io context will press ctrl+c\n");
	printf("\t--thread-exit\n"
		"\t\tThread exit but main not exit when IO error\n");
	printf("\t--fulldata-cmp\n"
		"\t\tCheck sector fulldata instead of first 8 Byte\n");
	printf("\t--offset=seek\n"
		"\t\tSpecify device start offset\n");
	printf("\t--duty=runtime:idletime or [thread0-runtime:idletime,thread1-runtime:idletime...]\n"
		"\t\tSpecify runtime and idletime, support s for second and h for hour\n");
	printf("\t--discard=N\n"
		"\t\tInsert N/100 dicard cmds among read and write. For example: 0 - no discard; 100 - all cmds are discard; 25 - 25%% cmds are discard\n");
	printf("\t--batch-discard-interval=N\n"
		"\t\tSent batch discard cmds to target per N seconds! 0 mean disable.\n");
	printf("\t--batch-discard-sizeratio=N\n"
		"\t\tSent the length of filesize*N/100 discard cmds to target per batch! 0 mean disable.\n");
	printf("\t--sigint-ecode=n\n"
		"\t\tProgram exit with code 0 when capture ctrl-c default but this option will change exit code.\n");
	printf("\t--max-latency=N(us)\n"
		"\t\tProgram exit with error if blocksize rw latency large than this value.\n");
	printf("\t--skip-check-lba=filename\n"
		"\t\tDo not check data integrity of these lbas when do reinstall check. One decimal lba address perline in file\n");
	printf("\t--help\n"
		"\t\tDisplay this help and exit.\n");

	printf("Debug options:\n");
	printf("\t--skip-verify\n"
		"\t\tSkip data check.\n");
	printf("\t--disable-dynamic-alloc\n"
		"\t\tDisable dynamic allocate data memory for each data transfer.\n");
}

static void get_optstr(struct option *long_opt, char *optstr, int optstr_size)
{
	int c = 0;
	struct option *opt;

	memset(optstr, 0x00, optstr_size);
	optstr[c++] = ':';

	for (opt = long_opt; NULL != opt->name; opt++) {
		optstr[c++] = opt->val;

		if (required_argument == opt->has_arg) {
			optstr[c++] = ':';
		} else if (optional_argument == opt->has_arg) {
			optstr[c++] = ':';
			optstr[c++] = ':';
		}
		assert(c < optstr_size);
	}
}

static char *get_strsize(char *str, off_t *size)
{
	char *p, *endptr;
	off_t tmp;

	p = str;
	*size = 0;

	while (1) {
		int shift = 1;

		tmp = strtoul(p, &endptr, 10);

		if ('k' == endptr[0] || 'K' == endptr[0])
			tmp *= 1024;
		else if ('m' == endptr[0] || 'M' == endptr[0])
			tmp *= (1024 * 1024);
		else if ('g' == endptr[0] || 'G' == endptr[0])
			tmp *= (1024 * 1024 * 1024);
		else
			shift = 0;

		*size += tmp;

		p = endptr + shift;
		while(*p == ' ') p++;
		if (*p != '+')
			return p;
		else
			p++;
		while(*p == ' ') p++;
	}
}

static int auto_filesize(struct io_check *check)
{
	int i;
	int *uniqint;
	__u64 avspace;
	char mode;

	if (check->nthread > 63) {
		printf("file-size=auto support max 63 threads\n");
		return 1;
	}

	if (!strcmp(check->automode, "auto"))
		mode = 'X';
	else if (!strcmp(check->automode, "auto-A"))
		mode = 'A';
	else if (!strcmp(check->automode, "auto-B"))
		mode = 'B';
	else if (!strcmp(check->automode, "auto-C"))
		mode = 'C';
	else if (!strcmp(check->automode, "auto-D"))
		mode = 'D';
	else {
		printf("Invalid automode: %s\n", check->automode);
		return 1;
	}

	if ('C' == mode && check->nthread < 16) {
		printf("file-size=auto-C need 16 threads at least\n");
		return 1;
	}

	if (check->is_rawdev)
		avspace = check->free_space * 99 / 100;
	else
		avspace = check->free_space * 95 / 100;

	uniqint = malloc(check->nthread * sizeof(*uniqint));
	assert(uniqint != NULL);

	for (i = 0; i < check->nthread; i++)
		uniqint[i] = i;
	for (i = 0; i < check->nthread; i++) {
		int j, tmp;

		j = random() % check->nthread;
		tmp = uniqint[j];
		uniqint[j] = uniqint[i];
		uniqint[i] = tmp;
	}
	check->each_filenum = 0;

	if ('X' == mode) {
		for (i = 0; i < check->nthread; i++) {
			check->each_filesize[uniqint[i]] = ALIGN(avspace / (1ul << (i + 1)) + (random() % 16 + 1) * check->blkdev_logicbz, check->blkdev_logicbz);
			check->each_filenum++;
		}
	}

	if ('A' == mode) {
		for (i = 0; i < check->nthread; i++) {
			check->each_filesize[uniqint[i]] = ALIGN(avspace / check->nthread + (random() % 16 + 1) * check->blkdev_logicbz, check->blkdev_logicbz);
			check->each_filenum++;
		}
	}

	if ('B' == mode) {
		for (i = 0; i < check->nthread; i++) {
			check->each_filesize[uniqint[i]] = ALIGN(rand64() % (avspace / (1ul << (i + 1))) + (random() % 16 + 1) * check->blkdev_logicbz, check->blkdev_logicbz);
			check->each_filenum++;
		}
	}

	if ('C' == mode) {
		for (i = 0; i < check->nthread; i++) {
			if (i < 4) {	// 4 (0, 1M]
				check->each_filesize[uniqint[i]] = ALIGN(lrand48() % (1024 * 1024) + check->blkdev_logicbz, check->blkdev_logicbz);
				check->each_filenum++;
				avspace -= check->each_filesize[uniqint[i]];
				continue;
			}

			if (i < 6) {	// 2 [1M, 100M]
				check->each_filesize[uniqint[i]] = ALIGN(lrand48() % (99 * 1024 * 1024) + 1024 * 1024, check->blkdev_logicbz);
				check->each_filenum++;
				avspace -= check->each_filesize[uniqint[i]];
				continue;
			}

			if (i < 8) {	// 2 [100M, 1G]
				check->each_filesize[uniqint[i]] = ALIGN(lrand48() % (900 * 1024 * 1024) + 100 * 1024 * 1024, check->blkdev_logicbz);
				check->each_filenum++;
				avspace -= check->each_filesize[uniqint[i]];
				continue;
			}

			check->each_filesize[uniqint[i]] = ALIGN(avspace / (1ul << (i - 8 + 1)) + (random() % 16 + 1) * check->blkdev_logicbz, check->blkdev_logicbz);
			check->each_filenum++;
		}
	}

	if ('D' == mode) {
		for (i = 0; i < check->nthread / 2; i++) {
			check->each_filesize[uniqint[i]] = ALIGN(avspace / (1ul << (i + 1)) + (random() % 16 + 1) * check->blkdev_logicbz, check->blkdev_logicbz);
			check->each_filenum++;
		}

		for (; i < check->nthread; i++) {
			check->each_filesize[uniqint[i]] = ALIGN(lrand48() % (32 * 1024 * 1024) + check->blkdev_logicbz, check->blkdev_logicbz);
			check->each_filenum++;
		}
	}

	return 0;
}

static int timeparse(char *p, char **end)
{
	char *endptr;
	int t = strtol(p, &endptr, 10);

	if (tolower(*endptr) == 's' || tolower(*endptr) == 'h') {
		t *= (tolower(*endptr) == 's') ? 1 : 3600;
		endptr++;
	}

	*end = endptr;
	return t;
}

int parse_cmdline(struct io_check *check, int argc, char **argv)
{
	int i, fd, opt, rstopt;
	char optstr[256], name[512];
	char *p, *headptr, *endptr;
	struct stat stat;
	struct statfs statfs;

	if (argc <= 1) {
		usage();
		exit(EXIT_FAILURE);
	}

	check->interval = 1;
	check->bz_method = INVALID_BZ;

	rstopt = 0;
	for (i = 0; i < argc; i++) {
		if (!strncmp(argv[i], "--reinstall-check=", strlen("--reinstall-check="))) {
			rstopt = 1;
			assert(argv[i] + strlen("--reinstall-check="));
			strcpy(check->context_filename, argv[i] + strlen("--reinstall-check="));

			fd = open(check->context_filename, O_RDWR);
			if (fd < 0)
				perror_exit("Open check->context_filename %s failed", check->context_filename);

			if (read(fd, check, sizeof(*check)) != sizeof(*check))
				perror_exit("Read check->context_filename %s failed", check->context_filename);

			if (strcmp(check->id, CHECK_CONTEXT_TAG)) {
				printf("Invalid context file\n");
				exit(EXIT_FAILURE);
			}
			check->reinstall_check = 1;

			/* don't read */
			check->stop = 0;
			check->ready = 0;
			check->sigint = 0;
			check->max_time = 0;
			check->complete_nthread = 0;
			check->restore_check = 0;
			check->perline_fp = NULL;
			check->reporting_perline = 0;
		}
	}

	get_optstr(lopts, optstr, sizeof(optstr));
	while ((opt = getopt_long_only(argc, argv, optstr, lopts, NULL)) != -1) {
		switch (opt) {
		case OPT_TARGET:
			strcpy(check->target, optarg);
			if ('/' == check->target[strlen(check->target) - 1])
				check->target[strlen(check->target) - 1] = 0;
			break;

		case OPT_FILESIZE:
			if (!strncmp(optarg, "auto", 4)) {
				check->automode = strdup(optarg);
			} else 	if ('[' != optarg[0]) {
				headptr = optarg;
				get_strsize(headptr, &check->uniq_filesize);
			} else {
				endptr = optarg;
				check->each_filenum = 0;

				do {
					headptr = endptr + 1;
					while(isspace(*headptr)) headptr++;

					endptr = get_strsize(headptr, &check->each_filesize[check->each_filenum]);

					if (',' != endptr[0] && ']' != endptr[0]) {
						printf("Invalid 'filesize' option\n");
						return EXIT_FAILURE;
					}

					check->each_filenum++;
				} while(']' != endptr[0]);
			}
			break;

		case OPT_NUM_THREADS:
			check->nthread = strtoul(optarg, NULL, 10);
			break;

		case OPT_BLOCK_SIZE:
			if ('[' != optarg[0]) {
				check->fix_bz = strtoul(optarg, &endptr, 10);
				if ('k' == endptr[0] || 'K' == endptr[0])
					check->fix_bz *= 1024;
				else if ('m' == endptr[0] || 'M' == endptr[0])
					check->fix_bz *= (1024 * 1024);

				check->bz_method = FIXED_BZ;
			} else {
				headptr = optarg + 1;
				check->min_bz = strtoul(headptr, &endptr, 10);
				if ('k' == endptr[0] || 'K' == endptr[0]) {
					check->min_bz *= 1024;
					endptr++;
				} else if ('m' == endptr[0] || 'M' == endptr[0]) {
					check->min_bz *= (1024 * 1024);
					endptr++;
				}
				if (',' != endptr[0]) {
					printf("Invalid 'block-size' option\n");
					return EXIT_FAILURE;
				}
				endptr++;
				while(isspace(*endptr)) endptr++;

				headptr = endptr;
				check->max_bz = strtoul(headptr, &endptr, 10);
				if ('k' == endptr[0] || 'K' == endptr[0]) {
					check->max_bz *= 1024;
					endptr++;
				} else if ('m' == endptr[0] || 'M' == endptr[0]) {
					check->max_bz *= (1024 * 1024);
					endptr++;
				}

				check->bz_method = RANDOM_BZ;
			}
			break;

		case OPT_DATA_DIRECTORY:
			strcpy(check->data_directory, optarg);
			if ('/' == check->data_directory[strlen(check->data_directory) - 1])
				check->data_directory[strlen(check->data_directory) - 1] = 0;
			break;

		case OPT_PAD_FILE:
			check->pad_file = 1;
			break;

		case OPT_MAX_TIME:
			check->max_time = strtol(optarg, NULL, 10);
			assert (check->max_time > 0);
			break;

		case OPT_REINSTALL_CHECK:
			if (!rstopt) {
				printf("Please input full option string --reinstall-check\n");
				exit(EXIT_FAILURE);
			}
			check->reinstall_check = 1;
			break;

		case OPT_RESTORE_CHECK:
			check->restore_check = 1;
			break;

		case OPT_RW_ENDURE:
			check->rw_endure |= RW_ENDURE_USER_MASK;

			check->rw_endure_wr = strtoul(optarg, &endptr, 10);
			if ('R' == endptr[0] || 'r' == endptr[1]) {
				if (check->rw_endure_wr)
					check->rw_endure |= RW_ENDURE_USER_WR_RAND_MASK;
				endptr++;
			}
			if (':' != endptr[0]) {
				printf("rw_endure option format WriteCnt[R/r]:ReadCnt[R/r]\n");
				exit(EXIT_FAILURE);
			}

			p = endptr + 1;
			check->rw_endure_rd = strtoul(p, &endptr, 10);
			if ('r' == endptr[0] || 'R' == endptr[0]) {
				if (check->rw_endure_rd)
					check->rw_endure |= RW_ENDURE_USER_RD_RAND_MASK;
			}
			assert(0 != check->rw_endure_wr || 0 != check->rw_endure_rd);
			break;

		case OPT_SKIP_VERIFY:
			check->skip_verify = 1;
			break;

		case OPT_DISABLE_MISMATCH_EXIT:
			check->disable_mismatch_exit = 1;
			break;

		case OPT_DISABLE_DANYMIC_ALLOC:
			check->disable_dynamic_alloc = 1;
			break;

		case OPT_SEQ_WRITE:
			check->seq_write = 1;
			break;

		case OPT_SKIP_CHECK_MEMSIZE:
			check->skip_ck_memsize = 1;
			break;

		case OPT_LOOPS:
			check->loops = atoi(optarg);
			break;

		case OPT_INTERVAL:
			check->interval = atoi(optarg);
			break;

		case OPT_PERLINE:
			check->reporting_perline = 1;
			if (NULL == optarg)
				break;
			if (NULL == (check->perline_fp = fopen(optarg, "w"))) {
				printf("create perline file %s failed\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;

		case OPT_SET_LOGBZ:
			check->set_logbz = strtoul(optarg, &endptr, 10);
			if ('k' == endptr[0] || 'K' == endptr[0])
				check->set_logbz *= 1024;
			break;

		case OPT_IMMEDIATE_READ:
			check->immediate_read = 1;
			break;

		case OPT_IGNORE_SAVE:
			check->ignore_save = 1;
			break;

		case OPT_THREAD_EXIT:
			check->thread_exit = 1;
			break;

		case OPT_FULLDATA_CMP:
			check->fulldata_cmp = 1;
			break;

		case OPT_OFFSET:
			check->ck_goffset = strtoul(optarg, &endptr, 10);
			if (*endptr == 'k' || *endptr == 'K')
				check->ck_goffset *= 1024;
			else if (*endptr == 'm' || *endptr == 'M')
				check->ck_goffset *= (1024*1024);
			else if (*endptr == 'g' || *endptr == 'G')
				check->ck_goffset *= (1024*1024*1024);
			break;

		case OPT_DUTY:
			if ('[' != optarg[0]) {
				int on, off;

				on = timeparse(optarg, &endptr);
				if (*endptr != ':') {
					printf("Invalid duty parameter 1\n");
					exit(EXIT_FAILURE);
				}
				off = timeparse(endptr+1, &endptr);

				for (i = 0; i < 512; i++) {
					check->each_duty_on[i] = on;
					check->each_duty_off[i] = off;
				}
			} else {
				endptr = optarg;
				check->each_dutynum = 0;

				do {
					check->each_duty_on[check->each_dutynum] = timeparse(endptr+1, &endptr);
					if (*endptr != ':') {
						printf("Invalid duty parameter 2\n");
						exit(EXIT_FAILURE);
					}
					check->each_duty_off[check->each_dutynum++] = timeparse(endptr+1, &endptr);
					if (*endptr != ',' && *endptr != ']') {
						printf("Invalid duty parameter 3\n");
						exit(EXIT_FAILURE);
					}
				} while (*endptr != ']');
			}
			break;

		case OPT_DISCARD:
			check->discard = atoi(optarg);
			if (check->discard < 0 || check->discard > 1000000) {
				printf("discard should between 0 and 1000000!\n");
				exit(EXIT_FAILURE);
			}
			break;

		case OPT_BATCH_DISCARD_INTERVAL:
			check->batch_discard_interval = atoi(optarg);
			if (check->batch_discard_interval < 0) {
				printf("option batch-discard-interval should > 0\n");
				exit(EXIT_FAILURE);
			}
			break;

		case OPT_BATCH_DISCARD_SIZERATIO:
			check->batch_discard_sizeratio = atoi(optarg);
			if (check->batch_discard_sizeratio < 0 || check->batch_discard_sizeratio > 100) {
				printf("option batch-discard-sizeratio should between 0 and 100!\n");
				exit(EXIT_FAILURE);
			}
			break;

		case OPT_SIGINT_ECODE:
			check->sigint_ecode = atoi(optarg);
			if (check->sigint_ecode < 0 || check->sigint_ecode > 127) {
				printf("sigint-ecode should between 0 and 127!\n");
				exit(EXIT_FAILURE);
			}
			break;

		case OPT_MAX_LATENCY:
			check->max_latency = strtol(optarg, NULL, 10);
			assert(check->max_latency >= 0);
			break;

		case OPT_SKIP_CHECK_LBA:
			check->skip_check_lba_fp = fopen(optarg, "r");
			if (NULL == check->skip_check_lba_fp) {
				print("Open file %s error: ", optarg); perror("");
				exit(EXIT_FAILURE);
			}
			break;

		case OPT_HELP:
			usage();
			exit(0);

		default:
			usage();
			exit(EXIT_FAILURE);
		}
	}

	if (check->seq_write) {
		check->rw_endure = RW_ENDURE_USER_MASK;
		check->rw_endure_rd = 0;
		check->rw_endure_wr = 100;
	}

	check->purely_check = (check->reinstall_check && !check->restore_check);
	if (check->purely_check && check->skip_check_lba_fp) {
		char *p, line[128];

		check->skip_lba = malloc(MAX_SKIP_LBA_COUNT*sizeof(*check->skip_lba));
		assert(check->skip_lba != NULL);
		memset(check->skip_lba, 0x00, MAX_SKIP_LBA_COUNT*sizeof(*check->skip_lba));

		while (fgets(line, sizeof(line), check->skip_check_lba_fp)) {
			p = line;
			while (isblank(*p)) p++;
			if (*p == '\r' || *p == '\n')
				continue;
			check->skip_lba[check->skip_lba_count++] = strtoul(p, NULL, 10);
			if (check->skip_lba_count > MAX_SKIP_LBA_COUNT) {
				printf("skip too much lbas! Max allowed is %d\n", MAX_SKIP_LBA_COUNT);
				exit(EXIT_FAILURE);
			}
		}
		check->skip_lba[check->skip_lba_count] = -1L;
		fclose(check->skip_check_lba_fp);
	}
	if (check->reinstall_check)
		return 0;

	/* get logic block size and free space */
	if (NULL == check->target) {
		printf("Missing --target=? option\n");
		exit(EXIT_FAILURE);
	}

	/* swap test */
	if (!strncmp(check->target, "mem", strlen("mem"))) {
		char *unit_end;

		check->blkdev_logicbz = strtoul(check->target + 3, &unit_end, 10);
		if ('k' == unit_end[0] || 'K' == unit_end[0])
			check->blkdev_logicbz *= 1024;
		if (!check->blkdev_logicbz || check->blkdev_logicbz % 512 || check->blkdev_logicbz / 512 > 8) {
			printf("Invalid mem sector size %d\n", check->blkdev_logicbz);
			exit(EXIT_FAILURE);
		}

		check->free_space = -1;
		check->ismem = 1;
		check->raw_blkdev_logicbz = check->blkdev_logicbz;
		goto check_input_bs;
	}

	/* normal disk test */
	fd = open(check->target, O_RDONLY);
	if (fd < 0) {
		perror("Open target failed");
		return 1;
	}
	if (fstat(fd, &stat) < 0) {
		perror("Get target stat failed");
		return 1;
	}

	if (S_ISDIR(stat.st_mode)) {		// target is directory
		check->is_rawdev = 0;

		if (fstatfs(fd, &statfs)) {
			perror("Get statfs failed\n");
			return 1;
		}
		check->blkdev_logicbz = statfs.f_bsize;
		check->free_space = (off_t)statfs.f_bfree * statfs.f_bsize;

		if ((off_t)statfs.f_blocks * statfs.f_bsize - check->free_space > 4ul * 1024 * 1024 * 1024) {
			char line[64];

			print("The target directory %s maybe not empty, continue anyway [yes|no]: ", check->target);
			fgets(line, sizeof(line), stdin);
			if (strncmp(line, "yes", 3))
				exit(EXIT_FAILURE);
		}

		if ('\0' == check->data_directory[0])
			strcpy(check->data_directory, check->target);
	} else if (S_ISBLK(stat.st_mode)) {	// target is raw block device
		check->is_rawdev = 1;

		if (ioctl(fd, BLKGETSIZE, &check->free_space)) {
			perror("Get free space failed");
			return 1;
		}
		check->free_space *= 512;

		if (ioctl(fd, BLKSSZGET, &check->blkdev_logicbz)) {
			perror("Get block size failed");
			return 1;
		}

		if ('\0' == check->data_directory[0])
			getcwd(check->data_directory, PATH_MAX);
	} else {
		printf("ERR target: %s is neither a directory nor a raw block device\n", check->target);
		return 1;
	}
	check->raw_blkdev_logicbz = check->blkdev_logicbz;
	if (check->set_logbz) {
		if (check->set_logbz < check->blkdev_logicbz || check->set_logbz % check->blkdev_logicbz) {
			printf("set-logbz should larger than actual logbz: set=%d actual=%d\n",
				check->set_logbz, check->blkdev_logicbz);
			exit(EXIT_FAILURE);
		}

		check->blkdev_logicbz = check->set_logbz;
	}
	if (check->ck_goffset % check->blkdev_logicbz) {
		printf("option offset should be divided exactly by %d\n", check->blkdev_logicbz);
		exit(EXIT_FAILURE);
	}
	strcpy(check->context_filename, check->data_directory);
	strcat(check->context_filename, "/iotest.context");
	close(fd);

	if (check->blkdev_logicbz % 512 || 0 == check->blkdev_logicbz / 512) {
		printf("Invalid logbz: %d\n", check->blkdev_logicbz);
		exit(EXIT_FAILURE);
	}

	/* auto file size */
	if (check->automode) {
		if (auto_filesize(check))
			exit(EXIT_FAILURE);
	}

	/* check if has permmit to write data_directory */
	fd = open(check->data_directory, O_RDONLY);
	if (fd < 0) {
		perror("Open data_directory failed");
		return 1;
	}
	close(fd);

	memset(name, 0, sizeof(name));
	strcat(name, check->data_directory);
	strcat(name, "/.wtest");
	fd = open(name, O_CREAT | O_RDWR);
	if (fd < 0) {
		perror("Data directory can`t be writte");
		return 1;
	}
	close(fd);
	unlink(name);

	/* check input block size */
check_input_bs:
	if (FIXED_BZ == check->bz_method) {
		if (check->fix_bz < check->blkdev_logicbz || check->fix_bz % check->blkdev_logicbz) {
			printf("Option --block-size=? must be large-or-equal and integral multiple of %s logical block size %d\n",
				check->is_rawdev ? "raw block device" : "filesystem", check->blkdev_logicbz);
			return 1;
		}
	} else if (RANDOM_BZ == check->bz_method) {
		if (check->min_bz < check->blkdev_logicbz || check->max_bz < check->min_bz
			|| check->min_bz % check->blkdev_logicbz || check->max_bz % check->blkdev_logicbz) {
			printf("Option --block-size=? must be large-or-equal and integral multiple of %s logical block size %d\n",
				check->is_rawdev ? "raw block device" : "filesystem", check->blkdev_logicbz);
			return 1;
		}
	} else {
		printf("Missing --block-size=? option\n");
		return 1;
	}

	/* check num-threads */
	if (check->nthread <= 0|| check->nthread > 256) {
		printf("Missing or invalid --num-threads=? option %d, invalid is [1, 256]\n", check->nthread);
		return 1;
	}

	/* check filesize */
	if (0 == check->uniq_filesize && 0 == check->each_filenum) {
		printf("Missing --filesize=? option\n");
		return 1;
	}

	if (0 != check->uniq_filesize) {	// uniq filesize for each file
		check->uniq_filesize = (check->uniq_filesize / check->blkdev_logicbz) * check->blkdev_logicbz;
		if (0 == check->uniq_filesize) {
			printf("Filesize is 0\n");
			return 1;
		}
		check->total_filesize = check->uniq_filesize * check->nthread;
	} else {				// each file has itself filesize
		if (check->each_filenum != check->nthread) {
			printf("ERR: you shoud give each file size\n");
			exit(EXIT_FAILURE);
		}

		check->total_filesize = 0;
		for (i = 0; i < check->nthread; i++) {
			check->each_filesize[i] = (check->each_filesize[i] / check->blkdev_logicbz) * check->blkdev_logicbz;
			if (0 == check->each_filesize[i]) {
				printf("Thread %d filesize is 0\n", i);
				return 1;
			}
			check->total_filesize += check->each_filesize[i];
		}
	}

	if (check->each_dutynum) {
		if (check->each_dutynum != check->nthread) {
			printf("ERR: you shoud give each thread duty\n");
			exit(EXIT_FAILURE);
		}
	}

	if (!check->ismem && check->total_filesize > check->free_space) {
		printf("ERR: overflow target free space\n");
		return 1;
	}

	check->mb_datamem_needed = ((check->total_filesize / check->blkdev_logicbz) * sizeof(__u64)) / (1024 * 1024);

#if 0
	printf("rw_endure_way=%d rw_endure=%d\n", check->rw_endure_way, check->rw_endure);
	printf("data_directory test file=%s\n", name);
	printf("file min logicbz=%d\n", check->blkdev_logicbz);
	printf("free_space=%ld\n", check->free_space);
	printf("mb_datamem_needed=%ldMB\n", check->mb_datamem_needed);
	printf("target=%s\n", check->target);
	printf("data_directory=%s\n", check->data_directory);
	printf("num-thread=%d\n", check->nthread);
	printf("fix_bz=%d min_bz=%d max_bz=%d\n",
		check->fix_bz, check->min_bz, check->max_bz);
	printf("uniq_filesize=%ld\n", check->uniq_filesize);
	printf("each_filesize=");
	for (i = 0; i < check->each_filenum; i++)
		printf("%ld ", check->each_filesize[i]);
	printf("\n");
#endif
	return 0;
}

/**************************************************************************************************************/
