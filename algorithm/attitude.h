/*
 * 姿态解算算法接口定义
 */

#ifndef ATTITUDE_H
#define ATTITUDE_H

#include "../include/types.h"
#include "../config/config.h"

// 姿态解算算法类型
typedef enum {
    ATTITUDE_ALGORITHM_COMPLEMENTARY,  // 互补滤波
    ATTITUDE_ALGORITHM_MADGWICK,       // Madgwick算法
    ATTITUDE_ALGORITHM_MAHONY          // Mahony算法
} attitude_algorithm_t;

// 姿态解算配置结构体
typedef struct {
    attitude_algorithm_t algorithm;     // 姿态解算算法类型
    float complementary_gain;           // 互补滤波增益 (0.0-1.0)
    float beta;                         // Madgwick/Mahony算法参数
    float dt;                           // 采样时间间隔 (s)
    bool use_magnetometer;              // 是否使用磁力计
} attitude_config_t;

// 姿态解算状态结构体
typedef struct {
    quaternion_t quaternion;            // 四元数表示
    euler_angle_t euler;                // 欧拉角表示 (度)
    vector3f_t angular_velocity;        // 角速度 (度/秒)
    vector3f_t linear_acceleration;     // 线加速度 (m/s²)
    vector3f_t gravity_vector;          // 重力向量
    float roll_rate;                    // 横滚角速度
    float pitch_rate;                   // 俯仰角速度
    float yaw_rate;                     // 偏航角速度
    uint32_t update_count;              // 更新次数
    uint32_t last_update_time;          // 上次更新时间
} attitude_state_t;

/**
 * @brief 初始化姿态解算模块
 * @param config 姿态解算配置
 * @return 是否初始化成功
 */
bool attitude_init(attitude_config_t *config);

/**
 * @brief 更新姿态解算
 * @param gyro_data 陀螺仪数据 (度/秒)
 * @param accel_data 加速度计数据 (g)
 * @param mag_data 磁力计数据 (可选)
 * @param dt 采样时间间隔 (秒)
 * @return 是否更新成功
 */
bool attitude_update(vector3f_t *gyro_data, vector3f_t *accel_data, 
                    vector3f_t *mag_data, float dt);

/**
 * @brief 获取当前姿态状态
 * @param state 姿态状态结构体
 * @return 是否获取成功
 */
bool attitude_get_state(attitude_state_t *state);

/**
 * @brief 设置四元数
 * @param quat 四元数
 */
void attitude_set_quaternion(quaternion_t *quat);

/**
 * @brief 设置欧拉角
 * @param euler 欧拉角 (度)
 */
void attitude_set_euler(euler_angle_t *euler);

/**
 * @brief 将四元数转换为欧拉角
 * @param quat 四元数
 * @param euler 欧拉角 (度)
 */
void quaternion_to_euler(quaternion_t *quat, euler_angle_t *euler);

/**
 * @brief 将欧拉角转换为四元数
 * @param euler 欧拉角 (度)
 * @param quat 四元数
 */
void euler_to_quaternion(euler_angle_t *euler, quaternion_t *quat);

/**
 * @brief 互补滤波姿态更新
 * @param gyro_data 陀螺仪数据
 * @param accel_data 加速度计数据
 * @param dt 采样时间间隔
 */
void complementary_filter_update(vector3f_t *gyro_data, vector3f_t *accel_data, float dt);

/**
 * @brief Madgwick算法姿态更新
 * @param gyro_data 陀螺仪数据
 * @param accel_data 加速度计数据
 * @param mag_data 磁力计数据
 * @param dt 采样时间间隔
 */
void madgwick_filter_update(vector3f_t *gyro_data, vector3f_t *accel_data, 
                           vector3f_t *mag_data, float dt);

/**
 * @brief Mahony算法姿态更新
 * @param gyro_data 陀螺仪数据
 * @param accel_data 加速度计数据
 * @param mag_data 磁力计数据
 * @param dt 采样时间间隔
 */
void mahony_filter_update(vector3f_t *gyro_data, vector3f_t *accel_data, 
                         vector3f_t *mag_data, float dt);

/**
 * @brief 归一化四元数
 * @param quat 四元数
 */
void quaternion_normalize(quaternion_t *quat);

/**
 * @brief 计算向量叉积
 * @param a 向量a
 * @param b 向量b
 * @param result 叉积结果
 */
void vector_cross_product(vector3f_t *a, vector3f_t *b, vector3f_t *result);

/**
 * @brief 计算向量点积
 * @param a 向量a
 * @param b 向量b
 * @return 点积结果
 */
float vector_dot_product(vector3f_t *a, vector3f_t *b);

/**
 * @brief 归一化向量
 * @param vec 向量
 */
void vector_normalize(vector3f_t *vec);

#endif // ATTITUDE_H