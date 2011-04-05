TARGET = libhpctimer.a
CFLAGS = -Wall
OBJS = hpctimer.o
CC = gcc
AR = ar

.SUFFIXES: .c

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(AR) rc $@ $^

clean:
	rm -rf ./*.o ./*~ $(TARGET)
