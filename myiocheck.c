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

	return 0;
}
