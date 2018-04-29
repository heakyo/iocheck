
# DEBUG=-D__DEBUG__
obj=iocheck
src=main.c option.c

myobj=myiocheck
mysrc=myiocheck.c myoption.c

all: $(obj) $(myobj)

$(obj): $(src) iocheck.h
	gcc $(DEBUG) -m64 -Wall -std=gnu99 -g -o $@ $(src) -lpthread

$(myobj): $(mysrc) myiocheck.h
	gcc $(DEBUG) -m64 -Wall -std=gnu99 -g -o $@ $^ -lpthread

clean:
	rm -f $(obj) $(myobj)

delete:
	rm -f iotest.*
