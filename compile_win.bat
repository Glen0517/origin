@echo off

REM 四轴飞行控制器Windows编译脚本
REM 请确保已安装GCC编译器，并添加到系统PATH环境变量

REM 定义编译器和编译选项
set CC=gcc
set CFLAGS=-Wall -Wextra -std=c99 -g -O0 -Iinclude -Iconfig -Ihal -Iservice -Idriver -Ilogic -Isafety -Irtos -Iplatform -Ialgorithm -Icommunication
set TARGET=flight_controller.exe

REM 创建输出目录
if not exist "output" mkdir "output"

REM 收集所有源文件（更简洁的方式）
set SOURCES=
set SOURCES=%SOURCES% main.c
set SOURCES=%SOURCES% platform\platform.c platform\platform_stm32.c
set SOURCES=%SOURCES% hal\hal_timer.c hal\hal_uart.c hal\hal_gpio.c hal\hal_i2c.c hal\hal_spi.c
set SOURCES=%SOURCES% driver\mpu6050.c driver\motor.c
set SOURCES=%SOURCES% safety\safety.c safety\arming_detector.c safety\safety_monitor.c
set SOURCES=%SOURCES% service\system.c
set SOURCES=%SOURCES% logic\flight_control.c
set SOURCES=%SOURCES% algorithm\attitude.c algorithm\pid.c
set SOURCES=%SOURCES% communication\communication.c
set SOURCES=%SOURCES% rtos\FreeRTOS.c rtos\rtos_adapter.c rtos\tasks.c

REM 设置测试源文件
set TEST_SOURCES=
set TEST_SOURCES=%TEST_SOURCES% tests\hal_test.c
set TEST_SOURCES=%TEST_SOURCES% platform\platform.c platform\platform_stm32.c
set TEST_SOURCES=%TEST_SOURCES% hal\hal_uart.c hal\hal_gpio.c hal\hal_i2c.c hal\hal_spi.c

REM 清理旧的输出文件
if exist "output\%TARGET%" del "output\%TARGET%"
if exist "output\hal_test.exe" del "output\hal_test.exe"

echo 开始编译主程序...
REM 编译主程序
%CC% %CFLAGS% %SOURCES% -o output\%TARGET%

REM 检查编译结果
if %ERRORLEVEL% == 0 (
    echo 主程序编译成功！可执行文件位于 output\%TARGET%
    echo 程序大小: 
    for %%I in (output\%TARGET%) do echo %%~zI 字节
) else (
    echo 主程序编译失败！
    exit /b 1
)

echo.
echo 开始编译测试程序...
REM 编译测试程序
%CC% %CFLAGS% %TEST_SOURCES% -o output\hal_test.exe

REM 检查测试程序编译结果
if %ERRORLEVEL% == 0 (
    echo 测试程序编译成功！可执行文件位于 output\hal_test.exe
) else (
    echo 测试程序编译失败！
    exit /b 1
)

echo.
echo 所有编译完成！
echo 编译信息:
echo - 编译器: %CC%
echo - 编译选项: %CFLAGS%
echo - 输出目录: output\