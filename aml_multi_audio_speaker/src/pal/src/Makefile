# PAL层编译配置
include ../../../lib/toolchain.mk
include ../../../lib/compile_flags.mk

PAL_INCLUDE = -I../include -I../../.. -I../../../include -I../../../src/hal/include

PAL_OBJS = \
    pal.o \
    pal_system.o \
    pal_storage.o \
    pal_network.o

%.o: %.c
	$(CC) $(CFLAGS) $(PAL_INCLUDE) -c $< -o $@

libpal.a: $(PAL_OBJS)
	$(AR) rcs $@ $^

clean:
	$(RM) -f $(PAL_OBJS) libpal.a

.PHONY: clean