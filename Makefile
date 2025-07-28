CC := gcc

demo: main.c cswitch.s
	$(CC) -g -O2 -z noexecstack main.c cswitch.s -o $@

.PHONY: clean
clean:
	$(RM) demo
