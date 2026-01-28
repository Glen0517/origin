# HAL层编译配置
include ../../../lib/toolchain.mk
include ../../../lib/compile_flags.mk

HAL_INCLUDE = -I../include -I../../.. -I../../../include

HAL_OBJS = \
    hal.o \
    hal_audio.o \
    hal_bt.o \
    hal_usb.o \
    hal_peri.o

%.o: %.c
	$(CC) $(CFLAGS) $(HAL_INCLUDE) -c $< -o $@

libhal.a: $(HAL_OBJS)
	$(AR) rcs $@ $^

clean:
	$(RM) -f $(HAL_OBJS) libhal.a

.PHONY: clean