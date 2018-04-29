#include "myiocheck.h"

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

int main(int argc, char *argv[])
{
	struct io_check *check;
	struct io_thread *thread;
	int status, tr;
	off_t goffset;

	srand48(time(NULL));
	srandom(time(NULL) + 13);
	srand(time(NULL) + 17);	 /* enhance robustness */

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

		check->runrandom[tr] = (__u8)(random() & 0xFF);
		thread->seq_num = ((__u64)tr << 56) + ((__u64)(check->runrandom[tr]) << 48) + 1;

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

	return 0;
}
