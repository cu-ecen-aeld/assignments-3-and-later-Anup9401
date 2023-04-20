
APP=writer
CFLAGS=-Wall
CFILE=$(APP).c
OBJECTS=$(APP).o
BINARY=$(APP)

ifdef CROSS_COMPILE
	CC=$(CROSS_COMPILE)gcc
else
	CC=gcc
endif

all : $(BINARY)

$(BINARY) : $(OBJECTS)
	$(CC) -o $@ $^

%.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean : 
	rm -rf $(BINARY) $(OBJECTS)

