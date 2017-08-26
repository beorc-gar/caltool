CC       = gcc
CFLAGS   = -Wall -std=c11 -g -fPIC `pkg-config --cflags python3`
LDFLAGS  =

all: caltool Cal.so

caltool: caltool.o calutil.o

caltool.o: caltool.c caltool.h

calutil.o: calutil.c calutil.h

Cal.so: calmodule.o calutil.o
	$(CC) -shared $^ $(LDFLAGS) -o $@

calmodule.o: calmodule.c calutil.h

clean:
	rm -f *.o caltool Cal.so
