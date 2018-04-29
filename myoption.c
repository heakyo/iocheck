#include "myiocheck.h"

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

int parse_cmdline(struct io_check *check, int argc, char **argv)
{
	int opt, fd;
	char optstr[256];
	char *endptr, *headptr;
	struct stat stat;

	if (argc <= 1) {
		usage();
		exit(EXIT_FAILURE);
	}

	get_optstr(lopts, optstr, sizeof(optstr));
	while ((opt = getopt_long_only(argc, argv, optstr, lopts, NULL)) != -1) {
		switch(opt) {
		case OPT_TARGET:
			strcpy(check->target, optarg);
			if ('/' == check->target[strlen(check->target) - 1])
				check->target[strlen(check->target) - 1] = 0;
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
			}
			break;

		case OPT_FILESIZE:
			if (!strncmp(optarg, "auto", 4)) {
				check->automode = strdup(optarg);
			} else 	if ('[' != optarg[0]) {
				headptr = optarg;
				get_strsize(headptr, &check->uniq_filesize);
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

	/* get logic block size and free space */
	if (!strlen(check->target)) {
		printf("Missing --target=? option\n");
		exit(EXIT_FAILURE);
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
		/* TODO */
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
	strcpy(check->context_filename, check->data_directory);
	strcat(check->context_filename, "/iotest.context");
	close(fd);

	if (check->blkdev_logicbz % 512 || 0 == check->blkdev_logicbz / 512) {
		printf("Invalid logbz: %d\n", check->blkdev_logicbz);
		exit(EXIT_FAILURE);
	}

	/* check if has permmit to write data_directory */
	fd = open(check->data_directory, O_RDONLY);
	if (fd < 0) {
		perror("Open data_directory failed");
		return 1;
	}
	close(fd);

	if (FIXED_BZ == check->bz_method) {
		if (check->fix_bz < check->blkdev_logicbz || check->fix_bz % check->blkdev_logicbz) {
			printf("Option --block-size=? must be large-or-equal and integral multiple of %s logical block size %d\n",
				check->is_rawdev ? "raw block device" : "filesystem", check->blkdev_logicbz);
			return 1;
		}
	}

	if (check->nthread <= 0|| check->nthread > 256) {
		printf("Missing or invalid --num-threads=? option %d, invalid is [1, 256]\n", check->nthread);
		return 1;
	}

	if (0 != check->uniq_filesize) {	// uniq filesize for each file
		check->uniq_filesize =
			((check->uniq_filesize + (check->blkdev_logicbz - 1)) / check->blkdev_logicbz) * check->blkdev_logicbz;
		if (0 == check->uniq_filesize) {
			printf("Filesize is 0\n");
			return 1;
		}
		check->total_filesize = check->uniq_filesize * check->nthread;
	} else {				// each file has itself filesize
		/* TODO */
	}

	return 0;
}
