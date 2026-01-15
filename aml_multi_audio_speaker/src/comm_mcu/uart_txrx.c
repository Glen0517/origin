#include "comm_mcu_priv.h"
#include "logger.h"
#include "product_type.h"

#include <aml_uart.h>        // 晶晨UART SDK

static bool g_uart_init = false;
static int g_uart_fd = -1;
static int g_uart_baudrate = UART_BAUDRATE;

/**
 * @brief UART接收回调函数
 */
static void uart_rx_callback(uint8_t *data, int len) {
    if (g_uart_init && data && len > 0) {
        LOG_DEBUG("UART received %d bytes: %.*s", len, len, data);
        
        // TODO: 处理接收到的数据
        // 这里可以根据协议解析MCU发送的命令和数据
    }
}

int uart_txrx_init(void) {
    if (g_uart_init) {
        LOG_INFO("UART TX/RX already initialized");
        return 0;
    }
    
    // 初始化Amlogic UART SDK
    if (aml_uart_init() != 0) {
        LOG_ERROR("UART TX/RX init failed: UART SDK init error");
        return -1;
    }
    
    // 打开UART设备
    g_uart_fd = aml_uart_open(UART_DEV_PATH, g_uart_baudrate, 
                              UART_DATA_BITS, UART_STOP_BITS, UART_PARITY);
    
    if (g_uart_fd < 0) {
        LOG_ERROR("UART TX/RX init failed: open UART device error");
        aml_uart_deinit();
        return -1;
    }
    
    // 设置UART接收回调
    aml_uart_set_rx_callback(uart_rx_callback);
    
    // 启用UART接收中断
    aml_uart_enable_rx_irq(true);
    
    g_uart_init = true;
    
    LOG_INFO("UART TX/RX init success");
    LOG_INFO("  Device: %s", UART_DEV_PATH);
    LOG_INFO("  Baudrate: %d", g_uart_baudrate);
    
    return 0;
}

void uart_txrx_deinit(void) {
    if (g_uart_init) {
        // 禁用UART接收中断
        aml_uart_enable_rx_irq(false);
        
        // 关闭UART设备
        if (g_uart_fd >= 0) {
            aml_uart_close(g_uart_fd);
            g_uart_fd = -1;
        }
        
        // 反初始化Amlogic UART SDK
        aml_uart_deinit();
        
        g_uart_init = false;
        
        LOG_INFO("UART TX/RX deinit success");
    }
}

/**
 * @brief 发送数据到MCU
 */
int uart_mcu_send_cmd(uint8_t *cmd, int len) {
    if (!g_uart_init || !cmd || len <= 0 || g_uart_fd < 0) {
        LOG_ERROR("UART send failed: invalid parameters or not initialized");
        return -1;
    }
    
    int sent_len = aml_uart_write(g_uart_fd, cmd, len);
    
    if (sent_len != len) {
        LOG_ERROR("UART send failed: sent %d bytes, expected %d bytes", sent_len, len);
        return -1;
    }
    
    LOG_DEBUG("UART sent %d bytes: %.*s", sent_len, sent_len, cmd);
    
    return sent_len;
}

/**
 * @brief 从MCU接收数据
 */
int uart_mcu_recv_data(uint8_t *buf, int len, int timeout_ms) {
    if (!g_uart_init || !buf || len <= 0 || g_uart_fd < 0) {
        LOG_ERROR("UART receive failed: invalid parameters or not initialized");
        return -1;
    }
    
    int recv_len = aml_uart_read(g_uart_fd, buf, len, timeout_ms);
    
    if (recv_len < 0) {
        LOG_ERROR("UART receive failed: timeout or error");
        return -1;
    }
    
    if (recv_len > 0) {
        LOG_DEBUG("UART received %d bytes: %.*s", recv_len, recv_len, buf);
    }
    
    return recv_len;
}