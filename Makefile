all: list.o test

test: list.o test.c
	g++ -o test list.o test.c -lgtest -pthread
	$(MAKE) test_run && $(MAKE) success || $(MAKE) timeout_err

test_run: test
	timeout 2s ./test

list.o: list.c list.h
	gcc -c list.c

clean:
	-rm *.o test

timeout_err:
	@echo
	@echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
	@echo "If you get a Segmentation fault, then your pointer dereference is to a incorrect memory address!"
	@echo "If the Error code is '1', one of the test cases failed! Check the printout"
	@echo "If the Error code is '124' your implementation of the linked list timed out! Perhaps there is an infinite loop?"
	@echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
	@echo

success:
	@echo
	@echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
	@echo "Passed the test, Everything worked!"
	@echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
	@echo

handin:
	git tag -a -f submit -m "Submitting Lab"
	git push
	git push --tags -f
