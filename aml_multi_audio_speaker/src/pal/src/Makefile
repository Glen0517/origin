# PAL层编译配置

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
	rm -f $(PAL_OBJS) libpal.a

.PHONY: clean