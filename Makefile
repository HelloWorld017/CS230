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

backend_test.o: mm.c mm.h backend_test.c
	$(CC) -D BACKEND_DEBUG $(CFLAGS) -o backend_test.o -c backend_test.c

backend_test: backend_test.o
	$(CC) $(CFLAGS) -o backend_test backend_test.o

mm_test.o: mm.c mm.h memlib.h
	$(CC) -D MM_DEBUG $(CFLAGS) -o mm_test.o -c mm.c

mm_test: mdriver.o mm_test.o memlib.o fsecs.o fcyc.o clock.o ftimer.o
	$(CC) $(CFLAGS) -o mm_test mdriver.o mm_test.o memlib.o fsecs.o fcyc.o clock.o ftimer.o

handin:
	git tag -a -f submit -m "Submitting Lab"
	git push
	git push --tags -f


clean:
	rm -f *~ *.o mdriver


