#include "myiocheck.h"

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

	return 0;
}
