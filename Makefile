#
# Students' Makefile for the Malloc Lab
#

CC = gcc
CFLAGS = -Wall -O2 -m32

OBJS = mdriver.o mm.o memlib.o fsecs.o fcyc.o clock.o ftimer.o

mdriver: $(OBJS)
	$(CC) $(CFLAGS) -o mdriver $(OBJS)

mdriver.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h
memlib.o: memlib.c memlib.h
mm.o: mm.c mm.h memlib.h
fsecs.o: fsecs.c fsecs.h config.h
fcyc.o: fcyc.c fcyc.h
ftimer.o: ftimer.c ftimer.h config.h
clock.o: clock.c clock.h

mm_test.o: mm.c mm.h memlib.h mm_test.c
backend_test: mm_test.o memlib.o
	$(CC) $(CFLAGS) -o backend_test mm_test.o memlib.o

handin:
	git tag -a -f submit -m "Submitting Lab"
	git push
	git push --tags -f


clean:
	rm -f *~ *.o mdriver


