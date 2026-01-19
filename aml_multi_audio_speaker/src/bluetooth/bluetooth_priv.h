#include "bt.h"
#include "product_type.h"
#include "peripheral.h"

// 蓝牙内部配置结构体
typedef struct {
    char bt_name[32];      // 蓝牙设备名
    char bt_pin[8];        // 蓝牙配对码
    bool bt_auto_connect;  // 是否自动重连
    bool mesh_en;          // 是否开启蓝牙MESH
    bool init_ok;          // 初始化状态
    bool bt_enable;        // 蓝牙使能状态
    bool bt_connected;     // 蓝牙连接状态
    bool bt_media_playing; // 蓝牙媒体播放状态
    bool bt_media_enable;  // 蓝牙媒体使能状态
} BtCfg_t;

// 内部私有函数声明
int bt_a2dp_init(void);
void bt_a2dp_deinit(void);
int bt_a2dp_start_stream(void);
void bt_a2dp_stop_stream(void);
int bt_a2dp_set_volume(int volume);
int bt_a2dp_get_volume(void);
int bt_mesh_init(void);
void bt_mesh_deinit(void);

