CC      ?= $(CROSS_COMPILE)gcc
LD      ?= $(CROSS_COMPILE)ld
AR      ?= $(CROSS_COMPILE)ar
NM      ?= $(CROSS_COMPILE)nm
OBJCOPY ?= $(CROSS_COMPILE)objcopy
OBJDUMP ?= $(CROSS_COMPILE)objdump
READELF ?= $(CROSS_COMPILE)readelf

OBJS = libzbouncer.o

CFLAGS += -Wall -I../ta/include -I$(TEEC_EXPORT)/include -I./include -I./ -Os
#Add/link other required libraries here
LDADD += -lteec.a -L$(TEEC_EXPORT)/lib

LIBNAME = libzbouncer

.PHONY: all
all: $(OBJS)

$(LIBNAME): $(OBJS)
	$(CC) -o $@ $< $(LDADD)

.PHONY: clean
clean:
	rm -f $(OBJS) $(LIBNAME)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
