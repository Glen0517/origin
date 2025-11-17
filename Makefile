# Makefile for Drone Flight Controller

# 编译器设置
CC = gcc
CXX = g++
AR = ar
RM = rm -rf
MKDIR = mkdir -p

# 编译选项
CFLAGS = -Wall -Wextra -g -O0 -std=gnu11
CXXFLAGS = -Wall -Wextra -g -O0 -std=gnu++11
LDFLAGS = 

# 目标文件名
TARGET = flight_controller
TEST_TARGET = flight_test

# 目录结构
SRC_DIR = .
HAL_DIR = $(SRC_DIR)/hal
TEST_DIR = $(SRC_DIR)/tests
DRIVER_DIR = $(SRC_DIR)/driver
SERVICE_DIR = $(SRC_DIR)/service
ALGORITHM_DIR = $(SRC_DIR)/algorithm
LOGIC_DIR = $(SRC_DIR)/logic
COMMUNICATION_DIR = $(SRC_DIR)/communication
SAFETY_DIR = $(SRC_DIR)/safety
INCLUDE_DIR = $(SRC_DIR)/include
CONFIG_DIR = $(SRC_DIR)/config
RTOS_DIR = $(SRC_DIR)/rtos

# 源文件
C_SOURCES = \
    $(SRC_DIR)/main.c \
    $(HAL_DIR)/hal_gpio.c \
    $(HAL_DIR)/hal_uart.c \
    $(HAL_DIR)/hal_spi.c \
    $(HAL_DIR)/hal_i2c.c \
    $(HAL_DIR)/hal_timer.c \
    $(DRIVER_DIR)/mpu6050.c \
    $(DRIVER_DIR)/motor.c \
    $(SERVICE_DIR)/system.c \
    $(ALGORITHM_DIR)/attitude.c \
    $(ALGORITHM_DIR)/pid.c \
    $(LOGIC_DIR)/flight_control.c \
    $(COMMUNICATION_DIR)/communication.c \
    $(SAFETY_DIR)/safety.c \
    $(SAFETY_DIR)/arming_detector.c \
    $(SAFETY_DIR)/safety_monitor.c \
    $(RTOS_DIR)/FreeRTOS.c \
    $(RTOS_DIR)/rtos_adapter.c

# 测试程序源文件
TEST_C_SOURCES = \
    $(TEST_DIR)/simple_test.c \
    $(HAL_DIR)/hal_gpio.c \
    $(HAL_DIR)/hal_uart.c \
    $(HAL_DIR)/hal_spi.c \
    $(HAL_DIR)/hal_i2c.c \
    $(HAL_DIR)/hal_timer.c \
    $(DRIVER_DIR)/mpu6050.c \
    $(DRIVER_DIR)/motor.c \
    $(SERVICE_DIR)/system.c \
    $(ALGORITHM_DIR)/attitude.c \
    $(ALGORITHM_DIR)/pid.c \
    $(LOGIC_DIR)/flight_control.c \
    $(COMMUNICATION_DIR)/communication.c \
    $(SAFETY_DIR)/safety.c \
    $(SAFETY_DIR)/arming_detector.c \
    $(SAFETY_DIR)/safety_monitor.c \
    $(RTOS_DIR)/FreeRTOS.c \
    $(RTOS_DIR)/rtos_adapter.c

# 头文件目录
INCLUDE_PATHS = \
    -I$(INCLUDE_DIR) \
    -I$(CONFIG_DIR) \
    -I$(HAL_DIR) \
    -I$(DRIVER_DIR) \
    -I$(SERVICE_DIR) \
    -I$(ALGORITHM_DIR) \
    -I$(LOGIC_DIR) \
    -I$(COMMUNICATION_DIR) \
    -I$(SAFETY_DIR) \
    -I$(RTOS_DIR)

# 目标文件
OBJECTS = $(patsubst %.c, %.o, $(C_SOURCES))
TEST_OBJECTS = $(patsubst %.c, %.o, $(TEST_C_SOURCES))

# 构建目标
all: $(TARGET)

# 链接可执行文件
$(TARGET): $(OBJECTS)
	@echo "Linking $@..."
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build completed successfully!"

# 编译C文件
%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) -c $< -o $@

# 清理目标文件和可执行文件
clean:
	@echo "Cleaning up..."
	$(RM) $(OBJECTS) $(TEST_OBJECTS) $(TARGET) $(TEST_TARGET)
	@echo "Clean completed!"

# 依赖关系
-include $(OBJECTS:.o=.d)
-include $(TEST_OBJECTS:.o=.d)

# 生成依赖文件
%.d: %.c
	@echo "Generating dependencies for $<..."
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) -MM -MT "$@ $(@:.d=.o)" $< > $@

# 构建测试程序
test: $(TEST_TARGET)
	@echo "Running tests..."
	./$(TEST_TARGET)

# 链接测试程序
$(TEST_TARGET): $(TEST_OBJECTS)
	@echo "Linking $@..."
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) $(TEST_OBJECTS) -o $@ $(LDFLAGS)
	@echo "Test build completed successfully!"

# 编译测试文件
test_compile:
	@echo "Compiling test files..."
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) -c $(TEST_DIR)/simple_test.c -o $(TEST_DIR)/simple_test.o

# 运行程序
run:
	./$(TARGET)

# 调试程序
debug:
	gdb ./$(TARGET)

# 调试测试程序
debug_test:
	gdb ./$(TEST_TARGET)

# 显示帮助信息
help:
	@echo "Usage: make [target]"
	@echo "Targets:"
	@echo "  all       - Build the project"
	@echo "  test      - Build and run tests (增强版功能测试)"
	@echo "  clean     - Clean up build files"
	@echo "  run       - Build and run the project"
	@echo "  debug     - Build and debug the project"
	@echo "  debug_test - Debug the test program"
	@echo "  help      - Show this help message"

.PHONY: all clean run debug help