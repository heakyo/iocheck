
# DEBUG=-D__DEBUG__
obj=iocheck
src=main.c option.c

${obj}: ${src} iocheck.h
	gcc ${DEBUG} -m64 -Wall -std=gnu99 -g -o $@ ${src} -lpthread

clean:
	@rm -f ${obj}

delete:
	rm -f iotest.*
