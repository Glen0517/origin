#ifndef __COMM_MCU_PRIV_H__
#define __COMM_MCU_PRIV_H__

#include "comm_mcu.h"
#include "product_type.h"

// UART设备定义
#define UART_DEV_PATH        "/dev/ttyS0"
#define UART_BAUDRATE        9600
#define UART_DATA_BITS       8
#define UART_STOP_BITS       1
#define UART_PARITY          0

// UART通信协议定义
#define CMD_QUERY_KEY_STATUS    0x01    // 查询按键/红外状态
#define CMD_KEY_STATUS_RESP     0x03    // 按键/红外状态响应
#define CMD_QUERY_TEMP_HUMID    0x02    // 查询温度湿度
#define CMD_TEMP_HUMID_RESP     0x04    // 温度湿度响应
#define CMD_QUERY_VERSION       0x05    // 查询固件版本
#define CMD_VERSION_RESP        0x06    // 固件版本响应
#define CMD_SET_LED_STATE       0x07    // 设置LED状态
#define CMD_SET_LED_RESP        0x08    // LED状态设置响应

// 按键状态位定义
#define KEY_BIT_PLAY_PAUSE      (1 << 0)
#define KEY_BIT_VOL_UP          (1 << 1)
#define KEY_BIT_VOL_DOWN        (1 << 2)
#define KEY_BIT_SOURCE_SWITCH   (1 << 3)
#define KEY_BIT_SOUND_MODE      (1 << 4)
#define KEY_BIT_BASS_UP         (1 << 5)
#define KEY_BIT_TREBLE_UP       (1 << 6)
#define KEY_BIT_IR_LEARN        (1 << 7)

// UART数据包格式定义
#define PACKET_HEADER           0xAA    // 数据包头部
#define PACKET_TAIL             0x55    // 数据包尾部

// 数据包最大长度
#define MAX_PACKET_LEN          64

// UART通信错误码
#define UART_ERR_NONE           0       // 无错误
#define UART_ERR_TIMEOUT        1       // 超时错误
#define UART_ERR_CHECKSUM       2       // 校验和错误
#define UART_ERR_INVALID_PACKET 3       // 无效数据包
#define UART_ERR_UNKNOWN_CMD    4       // 未知命令

// LED状态定义
#define LED_STATE_OFF           0
#define LED_STATE_ON            1
#define LED_STATE_BLINK         2
#define LED_STATE_BREATH        3

// 通信MCU配置结构体
typedef struct {
    int baud;               // 波特率
    int init_ok;            // 初始化状态
} CommMcuCfg_t;

// 温度湿度数据结构体
typedef struct {
    int temperature;        // 温度值（摄氏度）
    int humidity;           // 湿度值（百分比）
} TempHumidData_t;

// 固件版本数据结构体
typedef struct {
    int major;              // 主版本号
    int minor;              // 次版本号
    int patch;              // 补丁版本号
} VersionData_t;

// LED状态设置结构体
typedef struct {
    int led_idx;            // LED索引
    int led_state;          // LED状态
} LedState_t;

// 内部函数声明
int uart_txrx_init(void);
void uart_txrx_deinit(void);
int amp_ctrl_init(void);
void amp_ctrl_deinit(void);
int uart_mcu_send_cmd(uint8_t cmd, uint8_t *data, int data_len);
int uart_mcu_send_raw_data(uint8_t *data, int len);
int uart_mcu_recv_data(uint8_t *buf, int len, int timeout_ms);

// 通信协议相关函数声明
int uart_pack_data(uint8_t cmd, uint8_t *data, int data_len, uint8_t *packet, int *packet_len);
int uart_unpack_data(uint8_t *packet, int packet_len, uint8_t *cmd, uint8_t *data, int *data_len);
uint8_t uart_calculate_checksum(uint8_t *data, int len);

#endif // __COMM_MCU_PRIV_H__