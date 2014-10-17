# JLV - 10/17/14
# Early version - use only as framework example

OBJECTS=$(patsubst %.c,%.o, $(wildcard *.c))
CFLAGS= -Wall -Wextra -Werror

main: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o main

%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o
	$(CC) -MM $(CFLAGS) $*.c > $*.d
	@mv -f $*.d $*.d.tmp
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp | tee $*.d
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | \
	sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@rm -f $*.d.tmp

clean:
	-rm -f main *.o *.d

