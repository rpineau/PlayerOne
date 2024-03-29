# Makefile for libPlayerOne

CC = gcc
CFLAGS = -fpic -Wall -Wextra -O2 -g -DSB_LINUX_BUILD -I. -I./../../
CPPFLAGS = -fpic -Wall -Wextra -O2 -g -DSB_LINUX_BUILD -std=gnu++11  -I. -I./../../
LDFLAGS = -shared -lstdc++ -ludev
RM = rm -f
STRIP = strip
TARGET_LIB = libPlayerOne.so

SRCS = main.cpp PlayerOne.cpp x2camera.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all
all: ${TARGET_LIB}

$(TARGET_LIB): $(OBJS)
	$(CC) ${LDFLAGS} -o $@ $^ static_libs/`arch`/libPlayerOneCamera_Static.a static_libs/`arch`/libusb-1.0.a
	patchelf --add-needed libudev.so.1 libPlayerOne.so
	$(STRIP) $@ >/dev/null 2>&1  || true

$(SRCS:.cpp=.d):%.d:%.cpp
	$(CC) $(CFLAGS) $(CPPFLAGS) -MM $< >$@

.PHONY: clean
clean:
	${RM} ${TARGET_LIB} ${OBJS} *.d
