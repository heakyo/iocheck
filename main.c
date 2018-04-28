#define	_GNU_SOURCE	/* for *strerror_r() */
#define	__USE_GNU	/* for O_DIRECT */
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>

#include "iocheck.h"

#ifndef BLKDISCARD
#define BLKDISCARD	_IO(0x12,119)
#endif
#define	DISCARD_MAXSIZE	(32*1024*1024*1024L)

struct io_check *gic = NULL;

static int os_phy_memory(struct io_check *check);
static void setup_file(struct io_thread *thread);
static void *runner_thread(void *argv);
static void sigint_handler(int signo);

/**************************************************************************************************************/
int main(int argc, char **argv)
{
	int i, fd, tr, status;
	time_t timep;
	off_t goffset;
	struct io_check *check;
	struct io_thread *thread;
	pthread_attr_t attr;
	__u64 total_write_length, total_read_length;
	__u64 last_total_write_length = 0, last_total_read_length = 0, last_total_valid_length = 0;
	__u64 last_total_discard_count, last_total_discard_length;

	srand48(time(NULL));
	srandom(time(NULL) + 13);
	srand(time(NULL) + 17);	 /* enhance robustness */

	/* allocate check structure */
	check = malloc(sizeof(*check));
	if (NULL == check) {
		printf("Allocate check failed\n");
		return EXIT_FAILURE;
	}
	memset(check, 0x00, sizeof(*check));
	sprintf(check->id, CHECK_CONTEXT_TAG);
	check->running_count = 0;
	check->running_char[0] = '|';
	check->running_char[1] = '/';
	check->running_char[2] = '-';
	check->running_char[3] = '\\';
	check->thread_stacksize = 32 * 1024 * 1024;	// usually 8MB is default size on linux system, here use 32MB

	/* parse arguments */
	if (parse_cmdline(check, argc, argv))
		return EXIT_FAILURE;

	if (os_phy_memory(check))
		return EXIT_FAILURE;

	/* init mutex cond value */
	status = pthread_mutex_init(&check->ready_mutex, NULL);
	if (status) {
		printf("pthread_mutex_init failed: %s\n", strerror(status));
		return EXIT_FAILURE;
	}

	status = pthread_cond_init(&check->ready_cond, NULL);
	if (status) {
		printf("pthread_cond_init failed: %s\n", strerror(status));
		return EXIT_FAILURE;
	}

	/* init threads */
	check->thread = malloc(sizeof(*check->thread) * check->nthread);
	if (NULL == check->thread) {
		printf("Allocate check->thread array failed\n");
		return EXIT_FAILURE;
	}
	memset(check->thread, 0x00, sizeof(*check->thread) * check->nthread);

	for (tr = 0, goffset = 0; tr < check->nthread; tr++) {
		thread = check->thread + tr;
		thread->index = tr;

		thread->check = check;
		thread->terminate = 0;
		thread->write_length = 0;
		thread->read_length = 0;
		thread->discard_count = 0;
		thread->discard_length = 0;

		if (check->reinstall_check)
			thread->seq_num = check->seq_num[tr];
		else {
			check->runrandom[tr] = (__u8)(random() & 0xFF);
			thread->seq_num = ((__u64)tr << 56) + ((__u64)(check->runrandom[tr]) << 48) + 1;
		}

		thread->bz_method = check->bz_method;
		thread->min_bz = check->min_bz / check->blkdev_logicbz;
		thread->max_bz = check->max_bz / check->blkdev_logicbz;
		thread->fix_bz = check->fix_bz / check->blkdev_logicbz;

		thread->file_size = check->each_filenum ? check->each_filesize[tr] : check->uniq_filesize;
		if (check->is_rawdev) {
			thread->goffset = goffset + check->ck_goffset;
			if (thread->goffset + thread->file_size > check->free_space) {
				printf("Thread %d filesize overflow\n", tr);
				exit(EXIT_FAILURE);
			}
			goffset += thread->file_size;
		} else
			thread->goffset = 0;

		thread->duty_on = check->each_duty_on[tr];
		thread->duty_off = check->each_duty_off[tr];
	}

	/* print test info */
	printf("target=%s [%s]\n", check->target, check->is_rawdev ? "raw block device" : "directory");
	printf("%s logical-block-size=%d\n",
		check->is_rawdev ? "block device" : "filesystem", check->blkdev_logicbz);
	printf("filesize=");
	if (!check->each_filenum) {
		printf("%s\n", human_size(check->uniq_filesize));
	} else {
		for (i = 0; i < check->each_filenum; i++)
			printf("%s%s%s", (0 == i) ? "[" : "",
				human_size(check->each_filesize[i]),
				((check->each_filenum - 1) == i) ? "]\n" : ", ");
	}
	printf("total_filesize=%s\n", human_size(check->total_filesize));
	printf("num-threads=%d\n", check->nthread);
	printf("block-size=");
	if (RANDOM_BZ == check->bz_method)
		printf("[%d,%d]\n", check->min_bz, check->max_bz);
	else
		printf("%d\n", check->fix_bz);
	printf("rw_endure_way=");
	if (0 == (check->rw_endure & RW_ENDURE_USER_MASK)) {
		printf("default [1, 271]\n");
	} else {
		printf("%d%s:", check->rw_endure_wr, (check->rw_endure & RW_ENDURE_USER_WR_RAND_MASK) ? "R" : "");
		printf("%d%s\n", check->rw_endure_rd, (check->rw_endure & RW_ENDURE_USER_RD_RAND_MASK) ? "R" : "");
	}
	printf("duty=");
	for (i = 0; i < check->nthread; i++)
		printf("%s%d:%d%s", (0 == i) ? "[" : "", check->each_duty_on[i], check->each_duty_off[i], ((check->nthread - 1) == i) ? "]\n" : ", ");
	printf("memory-needed=%ldMB\n", check->mb_datamem_needed);
	printf("fulldata-cmp=%s\n", check->fulldata_cmp ? "Yes" : "No");
	printf("data-directory=%s\n", check->data_directory);
	printf("offset=%ld\n", check->ck_goffset);
	printf("runrandom=");
	for (i = 0; i < check->nthread; i++)
		printf("%02X%s", check->runrandom[i], (i == check->nthread-1) ? "\n" : ",");

	/* setup file for each thread */
	for (tr = 0; tr < check->nthread; tr++)
		setup_file(check->thread + tr);

	/* create threads */
	status = pthread_attr_init(&attr);
	if (status) {
		printf("pthread_attr_init failed: %s\n", strerror(status));
		return EXIT_FAILURE;
	}

	status = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if (status) {
		printf("pthread_attr_setdetachstate failed: %s\n", strerror(status));
		return EXIT_FAILURE;
	}

	status = pthread_attr_setstacksize(&attr, check->thread_stacksize);
	if (status) {
		printf("pthread_attr_setstacksize failed: %s\n", strerror(status));
		return EXIT_FAILURE;
	}

	for (tr = 0; tr < check->nthread; tr++) {
		status = pthread_create(&check->thread[tr].tid, &attr, &runner_thread, (void *)(check->thread + tr));
		if (status) {
			printf("create thread %d failed: %s\n", tr, strerror(status));
			return EXIT_FAILURE;
		}
	}

	status = pthread_attr_destroy(&attr);
	if (status) {
		printf("pthread_attr_destroy failed: %s\n", strerror(status));
		return EXIT_FAILURE;
	}

	/* synchronous work threads: wait for laying file complete */
	pthread_mutex_lock(&check->ready_mutex);
	while (check->ready < check->nthread)
		pthread_cond_wait(&check->ready_cond, &check->ready_mutex);
	pthread_mutex_unlock(&check->ready_mutex);
	time(&timep);
	printf("-----> Start time: %s", ctime(&timep));

	/* set ctrl+c signal handler */
	gic = check;
	if (SIG_ERR == signal(SIGINT, sigint_handler)) {
		printf("set signal handler failed\n");
		return EXIT_FAILURE;
	}

	/* main thread loop to print realtime speed */
	check->total_time = 0;
	while (1) {
		sleep(check->interval);
		check->total_time += check->interval;

		total_write_length = 0;
		total_read_length = 0;
		for (tr = 0; tr < check->nthread; tr++) {
			total_write_length += check->thread[tr].write_length;
			total_read_length += check->thread[tr].read_length;
		}

		if (!check->reporting_perline || check->perline_fp) {
			moveto_head();
			clear_line();
		}
		if (check->total_time != check->interval && check->reporting_perline && NULL == check->perline_fp)
			printf("\n");

		if (check->max_time)
			print("[%llu/%ld] ", check->total_time, check->max_time);
		else
			print("[%llu] ", check->total_time);
		print("Real-time write: %.2fMB/s read: %.2fMB/s | Average write: %.2fMB/s, read: %.2fMB/s",
			(total_write_length - last_total_write_length) / ((double)1024.0 * 1024 * check->interval),
			(total_read_length - last_total_read_length) / ((double)1024.0 * 1024 * check->interval),
			(total_write_length) / ((double)1024.0 * 1024 * check->total_time),
			(total_read_length) / ((double)1024.0 * 1024 * check->total_time));

		if (NULL != check->perline_fp) {
			fprintf(check->perline_fp, "[%llu] Real-time write: %.2fMB/s read: %.2fMB/s | Average write: %.2fMB/s, read: %.2fMB/s\n", check->total_time,
				(total_write_length - last_total_write_length) / ((double)1024.0 * 1024 * check->interval),
				(total_read_length - last_total_read_length) / ((double)1024.0 * 1024 * check->interval),
				(total_write_length) / ((double)1024.0 * 1024 * check->total_time),
				(total_read_length) / ((double)1024.0 * 1024 * check->total_time));
			fflush(check->perline_fp);
		}

		last_total_write_length = total_write_length;
		last_total_read_length = total_read_length;

		if ((check->max_time && check->total_time >= check->max_time) ||
			(check->loops && last_total_write_length >= (check->total_filesize * check->loops))) {
			for (tr = 0; tr < check->nthread; tr++)
				check->thread[tr].terminate = 1;
			check->stop = 1;
		}

		// stop test if define purely_check
		if (check->purely_check || check->thread_exit) {
			pthread_mutex_lock(&check->ready_mutex);
				if (check->complete_nthread >= check->nthread)
					check->stop = 1;
			pthread_mutex_unlock(&check->ready_mutex);	
		}

		if (!check->stop)
			continue;

		print("\nDon`t press ctrl+c again, for saving data file in process....");
		check->running_count++;

		last_total_write_length = 0;
		last_total_read_length = 0;
		last_total_valid_length = 0;
		last_total_discard_count = 0;
		last_total_discard_length = 0;
		for (tr = 0; tr < check->nthread; tr++) {
			status = pthread_join(check->thread[tr].tid, (void **)&thread);
			if (status) {
				printf("pthread_join failed: %s\n", strerror(status));
				return EXIT_FAILURE;
			}
			last_total_write_length += check->thread[tr].write_length;
			last_total_read_length += check->thread[tr].read_length;
			last_total_valid_length += check->thread[tr].valid_length;
			last_total_discard_count += check->thread[tr].discard_count;
			last_total_discard_length += check->thread[tr].discard_length;
		}

		printf("\n\n");
		for (tr = 0; tr < check->nthread; tr++) {
			printf("Thread %3d:  length %.2fGB write %.2fGB (%llu) read %.2fGB (%llu) valid %.2fGB (%llu) discard %.2fGB (%llu) count %ld\n", check->thread[tr].index,
				check->thread[tr].file_size / ((double)1024.0 * 1024 * 1024),
				check->thread[tr].write_length / ((double)1024.0 * 1024 * 1024), check->thread[tr].write_length,
				check->thread[tr].read_length / ((double)1024.0 * 1024 * 1024), check->thread[tr].read_length,
				check->thread[tr].valid_length / ((double)1024.0 * 1024 * 1024), check->thread[tr].valid_length,
				check->thread[tr].discard_length / ((double)1024.0 * 1024 * 1024), check->thread[tr].discard_length, check->thread[tr].discard_count);
		}
		printf("All thread:  write %.2fGB (%llu) read %.2fGB (%llu) valid %.2fGB (%llu) discard %.2fGB (%llu) count %llu\n",
			last_total_write_length / ((double)1024.0 * 1024 * 1024), last_total_write_length,
			last_total_read_length / ((double)1024.0 * 1024 * 1024), last_total_read_length,
			last_total_valid_length / ((double)1024.0 * 1024 * 1024), last_total_valid_length,
			last_total_discard_length / ((double)1024.0 * 1024 * 1024), last_total_discard_length, last_total_discard_count);
		printf("\nAverage: write %.2fMB/s  read %.2fMB/s\n",
			last_total_write_length / ((double)1024.0 * 1024 * check->total_time),
			last_total_read_length / ((double)1024.0 * 1024 * check->total_time));

		time(&timep);
		printf("<----- End time: %s", ctime(&timep));
		break;
	}

	/* save test context if needed */
	if (!check->purely_check && !check->skip_verify && !check->ismem && !check->ignore_save) {
		for (tr = 0; tr < check->nthread; tr++)
			check->seq_num[tr] = check->thread[tr].seq_num;

		fd = open(check->context_filename, O_CREAT | O_TRUNC | O_RDWR, 0666);
		if (fd < 0) {
			perror("Create/open context_filename failed\n");
			exit(EXIT_FAILURE);
		}
		if (write(fd, check, sizeof(*check)) != sizeof(*check)) {
			perror("Write context_filename failed\n");
			exit(EXIT_FAILURE);
		}
		close(fd);
	}

	if (NULL != check->perline_fp)
		fclose(check->perline_fp);
	free(check->thread);
	free(check);
	return check->sigint ? check->sigint_ecode : 0;
}

/**************************************************************************************************************/
static void sigint_handler(int signo)
{
	int tr;

	if (NULL == gic)
		exit(EXIT_SUCCESS);

	gic->stop = 1;
	gic->sigint = 1;

	for (tr = 0; tr < gic->nthread; tr++)
		gic->thread[tr].terminate = 1;
}

static int os_phy_memory(struct io_check *check)
{
	long pagesize, pages;

	pagesize = sysconf(_SC_PAGESIZE);
	pages = sysconf(_SC_PHYS_PAGES);

	if (pagesize < 0 || pages < 0) {
		printf("Get pagesize and phymemory failed\n");
		return EXIT_FAILURE;
	}
	check->pagesize = pagesize;
	check->mb_memory = pagesize * pages / (1024 * 1024);

	if (!check->skip_ck_memsize && check->mb_datamem_needed > (check->mb_memory * 4/5)) {
		printf("More system memory is needed!\n");
		return EXIT_FAILURE;
	}

	return 0;
}

/**************************************************************************************************************/
static void setup_file(struct io_thread *thread)
{
	int status;
	int pad_psz = 256 * 1024;
	struct io_check *check;

	check = thread->check;
	assert(NULL != check);

	/* memtest */
	if (check->ismem) {
		thread->mem = malloc(thread->file_size);
		if (NULL == thread->mem) {
			printf("Memtest: thread %d malloc mem file failed\n", thread->index);
			exit(EXIT_FAILURE);
		}
		return;
	}

	if (check->is_rawdev)
		sprintf(thread->filename, "%s", check->target);
	else
		sprintf(thread->filename, "%s/iotest.%d", check->target, thread->index);
	sprintf(thread->data_filename, "%s/iotest.data.%d", check->data_directory, thread->index);

	/* no need laying for raw device */
	if (check->is_rawdev) {
		thread->fd = open(thread->filename, (check->purely_check ? O_RDONLY : O_RDWR) | O_DIRECT | O_SYNC);
		if (thread->fd < 0)
			perror_exit("FATAL: thread %d open file %s failed", thread->index, thread->filename);
		return;
	} 

	/* laying out file for normal file */
	if (check->reinstall_check) {
		thread->fd = open(thread->filename, (check->purely_check ? O_RDONLY : O_RDWR) | O_DIRECT | O_SYNC);
	} else {
		unlink(thread->filename);
		thread->fd = open(thread->filename, O_CREAT | O_RDWR | O_DIRECT | O_SYNC, 0666);
	}
	if (thread->fd < 0)
		perror_exit("FATAL: thread %d %s file failed", thread->index, check->reinstall_check ? "open" : "create");
	if (check->reinstall_check)
		return;

	print("test: Laying out file %s / %s", thread->filename, human_size(thread->file_size));
	status = posix_fallocate(thread->fd, 0, thread->file_size);
	if (EINVAL == status) {		// maybe this filesystem not support this call
		check->pad_file = 1;
	} else if (status) {
		printf("FATAL: %s fallocate failed: %s\n",
			thread->filename, strerror_r(status, thread->errbuf, sizeof(thread->errbuf)));
		exit(EXIT_FAILURE);
	}

	if (check->pad_file) {
		void *padbuf;
		off_t remain, psz;

		printf(" --> Paddinf file...\n");

		posix_memalign((void **)&padbuf, check->pagesize, pad_psz);
		if (NULL == padbuf) {
			printf("FATAL: thread %d allocate posix_memalign failed\n", thread->index);
			exit(EXIT_FAILURE);
		}
		memset(padbuf, 0xA5, pad_psz);

		remain = thread->file_size;
		while (remain) {
			psz = (remain > pad_psz) ? pad_psz : remain;

			if (write(thread->fd, padbuf, psz) != psz)
				perror_exit("Thread %d padding file failed\n", thread->index);

			remain -= psz;
		}
		free(padbuf);
	} else
		printf("\n");
}

/**************************************************************************************************************/
static int get_count(struct io_check *check, int rw)
{
	if (0 == (RW_ENDURE_USER_MASK & check->rw_endure))
		return (random() % 271 + 1);

	if (IOCHECK_WRITE == rw) {
		if (RW_ENDURE_USER_WR_RAND_MASK & check->rw_endure)
			return (random() % check->rw_endure_wr + 1);
		else
			return check->rw_endure_wr;
	}

	if (IOCHECK_READ == rw) {
		if (RW_ENDURE_USER_RD_RAND_MASK & check->rw_endure)
			return (random() % check->rw_endure_rd + 1);
		else
			return check->rw_endure_rd;
	}

	assert(0);
}

static void *runner_thread(void *argv)
{
	void *buf;
	long i, j, k;
	int rw, count;
	struct io_thread *thread;
	struct io_check *check;
	long logicbz, offset, length, offset_ck = 0, logicb_units;
	long remain, current, curoff;
	int fd;
	size_t retnum;
	__u64 *sdata = NULL, *scmpdata = NULL;
	long range[2], restbytes, curbytes;
	struct timeval tv1, tv2;
	int tv1stat, tv2stat;

	thread = (struct io_thread *)argv;
	thread->immediate_read = 0;
	check = thread->check;
	logicbz = check->blkdev_logicbz;
	logicb_units = logicbz/8;

	/* allocate record data buffer */
	thread->file_logicbs = thread->file_size / logicbz;	// 'parse_cmdline' has align file_size to blkdev_logicbz
	if (!check->skip_verify) {
		thread->data = malloc(thread->file_logicbs * sizeof(*thread->data));
		if (NULL == thread->data) {
			printf("FATAL: thread %d malloc data memory failed\n", thread->index);
			exit(EXIT_FAILURE);
		}

		if (!check->reinstall_check) {
			for (i = 0; i < thread->file_logicbs; i++)
				thread->data[i] = 0;
		} else {
			fd = open(thread->data_filename, check->purely_check ? O_RDONLY : O_RDWR);
			if (fd < 0)
				perror_exit("FATAL: thread %d open data file %s to reinstall check failed", thread->index, thread->data_filename);

			remain = thread->file_logicbs * sizeof(*thread->data);
			curoff = 0;
			while (remain) {
				current = (remain > (1024 * 1024 * 1024)) ? (1024 * 1024 * 1024) : remain;
				if (pread(fd, (char *)thread->data + curoff, current, curoff) != current)
					perror_exit("FATAL: thread %d read data file %s to reinstall check failed", thread->index, thread->data_filename);
				curoff += current;
				remain -= current;
			}
			close(fd);

			if (check->purely_check && check->skip_check_lba_fp) {
				for (i = 0; i < check->skip_lba_count; i++) {
					if (check->skip_lba[i] >= thread->goffset/logicbz && check->skip_lba[i] < thread->goffset/logicbz + thread->file_logicbs)
						thread->data[check->skip_lba[i] - thread->goffset/logicbz] = 0;
				}
			}
		}
	}

	if (check->disable_dynamic_alloc) {
		posix_memalign((void **)&buf, check->pagesize, (RANDOM_BZ == thread->bz_method) ?
			thread->max_bz * logicbz : thread->fix_bz * logicbz);
		if (NULL == buf) {
			printf("FATAL: thread %d malloc buffer failed\n", thread->index);
			exit(EXIT_FAILURE);
		}
	}

	/*  synchronous main thread */
	pthread_mutex_lock(&check->ready_mutex);
	check->ready++;
	if (check->ready >= check->nthread)
		pthread_cond_signal(&check->ready_cond);
	pthread_mutex_unlock(&check->ready_mutex);

	rw = check->reinstall_check ? IOCHECK_READ : IOCHECK_WRITE;
	count = get_count(check, rw);

	if (check->fulldata_cmp) {
		posix_memalign((void **)&scmpdata, check->raw_blkdev_logicbz, logicbz);
		if (NULL == scmpdata) {
			printf("FATAL: thread %d malloc scmpdata failed\n", thread->index);
			exit(EXIT_FAILURE);
		}
	}

	/* IO loops */
	while (!thread->terminate) {
		pthread_yield();
		if (!check->purely_check && check->batch_discard_interval && check->batch_discard_sizeratio) {
			if ((check->total_time > thread->index*(check->batch_discard_interval/check->nthread)) &&
				((check->total_time - thread->index*(check->batch_discard_interval/check->nthread))/check->batch_discard_interval > thread->discard_count)) {
				long bdoff, bdlen;

				bdoff = lrand48() % thread->file_logicbs;
				bdlen = thread->file_logicbs * check->batch_discard_sizeratio/100;
				if (bdoff + bdlen > thread->file_logicbs)
					bdlen = thread->file_logicbs - bdoff;
				assert(bdoff < thread->file_logicbs);
				assert((bdoff + bdlen) <= thread->file_logicbs);

				range[0] = bdoff * logicbz + thread->goffset;
				restbytes = bdlen * logicbz;
				while (restbytes) {
					curbytes = (restbytes > DISCARD_MAXSIZE) ? DISCARD_MAXSIZE : restbytes;
					range[1] = curbytes;
					if (ioctl(thread->fd, BLKDISCARD, &range))
						perror_exit("FATAL: thread %d batch discard fail: start=%ld len=%ld\n", thread->index, range[0], range[1]);
					range[0] += curbytes;
					restbytes -= curbytes;

					thread->discard_count++;
				}
				thread->discard_length += bdlen * logicbz;
				for (i = 0; i < bdlen; i++)
					thread->data[bdoff + i] = 0;
				continue;
			}
		}

		if (!check->purely_check && thread->duty_on) {
			if (check->total_time % (thread->duty_on + thread->duty_off) >= thread->duty_on) {
				usleep(200);
				continue;
			}
		}

		if (check->purely_check) {
			rw = IOCHECK_READ;
		} else {
			if (count) {
				count--;
			} else {
				count = get_count(check, IOCHECK_WRITE == rw ? IOCHECK_READ : IOCHECK_WRITE);
				if (count)
					rw = (IOCHECK_WRITE == rw) ? IOCHECK_READ : IOCHECK_WRITE;
				else
					count = get_count(check, rw);
				count--;
			}
		}

		if (check->purely_check || check->seq_write)
			offset = offset_ck;
		else
			offset = lrand48() % thread->file_logicbs;

		if (RANDOM_BZ == thread->bz_method)
			length = (random() % (thread->max_bz - thread->min_bz + 1)) + thread->min_bz;
		else
			length = thread->fix_bz;

		if (check->purely_check) {
			for (; offset_ck < thread->file_logicbs; offset_ck++) {
				if (thread->data[offset_ck])
					break;
			}

			if (offset_ck > thread->file_logicbs) {
				printf("BUGgggggggg: tr=%d file_logicbs=%ld offset_ck=%ld\n",
					thread->index, thread->file_logicbs, offset_ck);
				exit(EXIT_FAILURE);
			} else if (offset_ck == thread->file_logicbs) {
				; /* nothing to do, the follow code will jump read loop */
			} else {
				for (i = 0; i < length && (i+offset_ck) < thread->file_logicbs; i++) {
					if (!thread->data[offset_ck + i])
						break;
				}
				length = i;
			}
			offset = offset_ck;
			offset_ck += length;
		}

		if (length + offset > thread->file_logicbs) {
			assert(offset <= thread->file_logicbs);
			length = thread->file_logicbs - offset;
			assert(length >= 0);

			if (check->purely_check) {
				thread->terminate = 1;

				pthread_mutex_lock(&check->ready_mutex);
				check->complete_nthread++;
				pthread_mutex_unlock(&check->ready_mutex);
				if (!length)
					break;
			}
		}

		if (check->seq_write && !check->purely_check)
			offset_ck = (offset_ck + length) % thread->file_logicbs;

		assert(offset < thread->file_logicbs);
		assert(length > 0);

		/* do discard if needed */
		if (check->discard && !check->purely_check) {
			if ((random() % 100) < check->discard) {
				range[0] = offset * logicbz + thread->goffset;
				restbytes = length * logicbz;
				while (restbytes) {
					curbytes = (restbytes > DISCARD_MAXSIZE) ? DISCARD_MAXSIZE : restbytes;
					range[1] = curbytes;
					if (ioctl(thread->fd, BLKDISCARD, &range))
						perror_exit("FATAL: thread %d discard fail: start=%ld len=%ld\n", thread->index, range[0], range[1]);
					range[0] += curbytes;
					restbytes -= curbytes;

					thread->discard_count++;
				}
				thread->discard_length += length * logicbz;
				for (i = 0; i < length; i++)
					thread->data[offset + i] = 0;
				continue;
			}
		}

		/* allocate rw data buffer */
		if (!check->disable_dynamic_alloc) {
			posix_memalign((void **)&buf, (check->raw_blkdev_logicbz > check->pagesize) ? check->pagesize : check->raw_blkdev_logicbz, length * logicbz);
			if (NULL == buf) {
				printf("WARNING: thread %d malloc buffer failed, continue\n", thread->index);
				continue;
			}
		}

		/* write if rw == IOCHECK_WRITE */
		if (IOCHECK_WRITE == rw) {
			for (j = 0; j < length; j++) {
				if (check->fulldata_cmp) {
					sdata = (__u64 *)((char *)buf + logicbz * j);

					for (k = 0; k < logicb_units; k++)
						sdata[k] = thread->seq_num + j * logicb_units + k;
				} else {
					*((__u64 *)((char *)buf + logicbz * j)) = thread->seq_num + j;
				}
			}

			if (check->ismem) {
				memcpy((char *)thread->mem + offset * logicbz, buf, length * logicbz);
			} else {
				if (check->max_latency)
					tv1stat = gettimeofday(&tv1, NULL);
				retnum = pwrite(thread->fd, buf, length * logicbz, offset * logicbz + thread->goffset);
				if (check->max_latency)
					tv2stat = gettimeofday(&tv2, NULL);
				if (check->max_latency && check->total_time > 0 && (!tv1stat && !tv2stat)) {
					__s64 latency = (tv2.tv_sec*1000000 + tv2.tv_usec) - (tv1.tv_sec*1000000 + tv1.tv_usec);

					if (latency < 0) {
						printf("BUG: write gettimeofday\n");
						exit(EXIT_FAILURE);
					}
					if (latency > check->max_latency) {
						printf("ERROR: thread %d write %ld bytes takes too long time %lld us\n", thread->index, length*logicbz, latency);
						exit(EXIT_FAILURE);
					}
				}

				if (retnum != length * logicbz) {
					if (check->thread_exit) {
						printf("WARNING: thread %d pwrite IO error exit. offset=%ld length=%ld retnum=%ld\n",
							thread->index, offset * logicbz + thread->goffset, length * logicbz, retnum);
						thread->terminate = 1;

						pthread_mutex_lock(&check->ready_mutex);
						check->complete_nthread++;
						pthread_mutex_unlock(&check->ready_mutex);

						check->thread_exit_offset[thread->index] = offset * logicbz + thread->goffset;
						check->thread_exit_length[thread->index] = length * logicbz;

						break;
					} else
						perror_exit("FATAL: thread %d pwrite failed\n", thread->index);
				}
			}
			thread->write_length += length * logicbz;

			for (j = 0; j < length; j++) {
				if (!check->skip_verify)
					thread->data[offset + j] = thread->seq_num;
				if (check->fulldata_cmp)
					thread->seq_num += logicb_units;
				else
					thread->seq_num++;
			}

			if (check->immediate_read) {
				thread->immediate_read = 1;
				rw = IOCHECK_READ;
			}
		}

		/* read if rw == IOCHECK_READ */
		if (IOCHECK_READ == rw) {
			for (j = 0; j < length; j++) // avoid cache
				*((__u64 *)((char *)buf + j * logicbz)) = 0x00;

			if (check->ismem) {
				memcpy(buf, (char *)thread->mem + offset * logicbz, length * logicbz);
			} else {
				if (check->max_latency)
					tv1stat = gettimeofday(&tv1, NULL);
				retnum = pread(thread->fd, buf, length * logicbz, offset * logicbz + thread->goffset);
				if (check->max_latency)
					tv2stat = gettimeofday(&tv2, NULL);
				if (check->max_latency && check->total_time > 0 && (!tv1stat && !tv2stat)) {
					__s64 latency = (tv2.tv_sec*1000000 + tv2.tv_usec) - (tv1.tv_sec*1000000 + tv1.tv_usec);

					if (latency < 0) {
						printf("BUG: write gettimeofday\n");
						exit(EXIT_FAILURE);
					}
					if (latency > check->max_latency) {
						printf("ERROR: thread %d read %ld bytes takes too long time %lld us\n", thread->index, length*logicbz, latency);
						exit(EXIT_FAILURE);
					}
				}

				if (retnum != length * logicbz) {
					if (check->thread_exit) {
						printf("WARNING: thread %d pread IO error exit. retnum=%ld\n", thread->index, retnum);
						thread->terminate = 1;

						pthread_mutex_lock(&check->ready_mutex);
						check->complete_nthread++;
						pthread_mutex_unlock(&check->ready_mutex);

						break;
					} else
						perror_exit("FATAL: thread %d pread failed\n", thread->index);
				}
			}
			thread->read_length += length * logicbz;

			if (!check->skip_verify) {
				for (j = 0; j < length; j++) {
					__u64 expect, actual;

					expect = thread->data[offset + j];
					if (0 == expect) // empty
						continue;

					if (check->fulldata_cmp) {
						char *p;
						time_t timep;

						for (k = 0; k < logicb_units; k++)
							scmpdata[k] = thread->data[offset + j] + k;
						sdata = (__u64 *)((char *)buf + j * logicbz);

						if (memcmp(scmpdata, sdata, logicbz)) {
							time(&timep);
							ctime_r(&timep, thread->errbuf);
							p = strchr(thread->errbuf, '\n');
							*p = '\0';

							printf("### SECTOR DATA MISMATCH [ %s ] thread-%d %s: off=%ld\n",
								thread->errbuf, thread->index, basename(thread->filename),
								(offset + j) * logicbz + thread->goffset);

							printf("expect:\n");
							for (k = 0; k < logicb_units; k++) {
								printf("%016llX ", scmpdata[k]);
								if ((k+1)%8 == 0)
									printf("\n");
							}

							printf("actual:\n");
							for (k = 0; k < logicb_units; k++) {
								if (scmpdata[k] != sdata[k])
									printf("\033[0;1;31m%016llX\033[0m ", sdata[k]);
								else
									printf("%016llX ", sdata[k]);
								if ((k+1)%8 == 0)
									printf("\n");
							}

							if (!check->disable_mismatch_exit)
								exit(EXIT_FAILURE);
						}

						continue;
					}

					actual = *((__u64 *)((char *)buf + j * logicbz));
					if (expect != actual) {
						char *p;
						time_t timep;
						__u64 *ibuf;

						if (check->purely_check && check->thread_exit && check->thread_exit_length[thread->index]) {
							/* don't use restore-check option for this case */
							if ((thread->goffset + (j + offset) * logicbz) >= check->thread_exit_offset[thread->index] &&
							    (thread->goffset + (j + offset) * logicbz) < (check->thread_exit_offset[thread->index] + check->thread_exit_length[thread->index])) {
								printf("thread-exit thread %d skip to check offset=%ld\n", thread->index, thread->goffset + (j + offset) * logicbz);
								continue;
							}
						}

						time(&timep);
						ctime_r(&timep, thread->errbuf);
						p = strchr(thread->errbuf, '\n');
						*p = '\0';

						posix_memalign((void **)&ibuf,(check->raw_blkdev_logicbz > check->pagesize) ? check->pagesize : check->raw_blkdev_logicbz, logicbz);
						if (NULL == ibuf)
							printf("Thread %d data mismatch read-again malloc failed\n", thread->index);
						if (NULL != ibuf) {
							if (pread(thread->fd, ibuf, logicbz, thread->goffset + (offset + j) * logicbz) != logicbz) {
								printf("Thread %d read-again read failed", thread->index);
								perror(" ");
							}
						}

						printf("### DATA MISMATCH [ %s ] thread-%d %s: off=%ld wr=%016llX rd=%016llX "
							"previous-rd=%016llX next-rd=%016llX, "
							"pk-info: off=%ld len=%ld, read-again=%016llX\n",
							thread->errbuf, thread->index, basename(thread->filename),
							(offset + j) * logicbz + thread->goffset, expect, actual,
							(j > 0) ? *((__u64 *)((char *)buf + (j - 1) * logicbz)) : ~0ull,
							(j < (length -1)) ? *((__u64 *)((char *)buf + (j + 1) * logicbz)) : ~0ull,
							thread->goffset + offset * logicbz, length * logicbz, ibuf[0]);
						if (!check->disable_mismatch_exit)
							exit(EXIT_FAILURE);
					}
				}
			}

			if (check->immediate_read && thread->immediate_read) {
				rw = IOCHECK_WRITE;
				thread->immediate_read = 0;
			}
		}

		if (!check->disable_dynamic_alloc)
			free(buf);
	}

	thread->valid_length = 0;
	for (i = 0; i < thread->file_logicbs; i++) {
		if (thread->data[i])
			thread->valid_length++;
	}
	thread->valid_length *= logicbz;

	if (check->purely_check) {
		if(thread->valid_length != thread->read_length) {
			printf("Thread %d: reinstall-check valid_length=%lld but read_length=%lld\n",
				thread->index, thread->valid_length, thread->read_length);
			exit(EXIT_FAILURE);
		}
	}

	if (!check->purely_check && !check->skip_verify && !check->ismem && !check->ignore_save) {
		char *dbuf;
		off_t remain, psz;

		thread->data_fd = open(thread->data_filename, O_CREAT | O_TRUNC | O_WRONLY, 0666);
		if (thread->data < 0)
			perror_exit("Thread %d create file %s failed\n", thread->index, thread->data_filename);

		dbuf = (char *)thread->data;
		remain = thread->file_logicbs * sizeof(*thread->data);

		while (remain) {
			psz = (remain > 16 * 1024) ? (16 * 1024) : remain;
			if (write(thread->data_fd, dbuf, psz) != psz)
				perror_exit("Thread %d write file %s failed\n", thread->index, thread->data_filename);
			dbuf += psz;
			remain -= psz;

			if (check->running_count)
				print("\b%c", check->running_char[check->running_count++ % 4]);
			pthread_yield();
		}

		free(thread->data);
		close(thread->data_fd);
	}

	if (check->ismem)
		free(thread->mem);
	else
		close(thread->fd);

	if (check->disable_dynamic_alloc)
		free(buf);

	if (scmpdata)
		free(scmpdata);
	return argv;
}
/**************************************************************************************************************/
