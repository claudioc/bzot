########################
# Bzot's Makefile
########################
CC := gcc
CFILES := main.c
PROG := bzot
CFLAGS := -Wall -Wextra -g

OBJFILES := $(CFILES:.c=.o)

$(PROG) : $(OBJFILES)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean
clean :
	rm -f $(PROG) $(OBJFILES)
