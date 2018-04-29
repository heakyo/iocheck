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

	return 0;
}
