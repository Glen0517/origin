@echo off

REM 四轴飞行控制器Windows编译脚本
REM 请确保已安装GCC编译器，并添加到系统PATH环境变量

set CC=gcc
set CFLAGS=-Wall -Wextra -std=c99 -Iinclude -Iconfig -Ihal -Iservice -Idriver -Ilogic -Isafety -Irtos
set TARGET=flight_controller.exe

REM 收集所有源文件
set SOURCES=main.c safety\safety.c safety\arming_detector.c safety\safety_monitor.c service\system.c hal\hal_timer.c hal\hal_uart.c hal\hal_gpio.c hal\hal_i2c.c hal\hal_spi.c driver\mpu6050.c driver\motor.c logic\flight_control.c algorithm\attitude.c algorithm\pid.c communication\communication.c rtos\FreeRTOS.c rtos\rtos_adapter.c rtos\tasks.c platform\platform.c

REM 创建输出目录
if not exist "output" mkdir "output"

REM 编译
%CC% %CFLAGS% %SOURCES% -o output\%TARGET%

REM 检查编译结果
if %ERRORLEVEL% == 0 (
    echo 编译成功！可执行文件位于 output\%TARGET%
) else (
    echo 编译失败！
    exit /b 1
)