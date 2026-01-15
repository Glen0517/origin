include ../../lib/toolchain.mk
include ../../lib/compile_flags.mk

MODULE_NAME := play_ctrl
SRC_FILES   := play_ctrl.c play_state.c sound_field.c
OBJ_FILES   := $(SRC_FILES:.c=.o)
LIB_OUT     := ../../lib/output/lib$(MODULE_NAME).a

all: $(LIB_OUT)
	@echo -e "\033[32m✅ [$(MODULE_NAME)] Compile success\033[0m"

$(LIB_OUT): $(OBJ_FILES)
	$(MKDIR) -p ../../lib/output
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ -I./ -I../../include

clean:
	$(RM) -rf $(OBJ_FILES) $(LIB_OUT)
	@echo -e "\033[32m✅ [$(MODULE_NAME)] Clean success\033[0m"

.PHONY: all clean