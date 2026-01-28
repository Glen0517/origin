include ../../lib/toolchain.mk
include ../../lib/compile_flags.mk

MODULE_NAME := system
SRC_FILES   := system.c system_api.c system_init.c advanced_log.c daemon.c event.c exception.c ipc.c log.c monitor.c process.c process_manager.c resource.c thread_pool.c
OBJ_FILES   := $(SRC_FILES:.c=.o)
LIB_OUT     := ../../lib/output/lib$(MODULE_NAME).a

all: $(LIB_OUT)
	@echo -e "\033[32m✅ [$(MODULE_NAME)] Compile success\033[0m"

$(LIB_OUT): $(OBJ_FILES)
	$(MKDIR) -p ../../lib/output
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ -I./ -I../../include

clean:
	$(RM) -rf $(OBJ_FILES) $(LIB_OUT)
	@echo -e "\033[32m✅ [$(MODULE_NAME)] Clean success\033[0m"

.PHONY: all clean