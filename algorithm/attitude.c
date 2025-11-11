/*
 * 姿态解算算法实现
 */

#include "attitude.h"
#include "../service/system.h"
#include "../config/config.h"
#include <math.h>

// 静态变量
static attitude_config_t attitude_config;
static attitude_state_t attitude_state;
static bool attitude_initialized = false;

// 使用types.h中定义的PI和角度转换常量，不再重复定义

/**
 * @brief 初始化姿态解算模块
 */
bool attitude_init(attitude_config_t *config) {
    if (config == NULL) {
        return false;
    }
    
    // 保存配置
    attitude_config = *config;
    
    // 初始化状态
    attitude_state.quaternion.w = 1.0f;
    attitude_state.quaternion.x = 0.0f;
    attitude_state.quaternion.y = 0.0f;
    attitude_state.quaternion.z = 0.0f;
    
    attitude_state.euler.roll = 0.0f;
    attitude_state.euler.pitch = 0.0f;
    attitude_state.euler.yaw = 0.0f;
    
    attitude_state.angular_velocity.x = 0.0f;
    attitude_state.angular_velocity.y = 0.0f;
    attitude_state.angular_velocity.z = 0.0f;
    
    attitude_state.linear_acceleration.x = 0.0f;
    attitude_state.linear_acceleration.y = 0.0f;
    attitude_state.linear_acceleration.z = 0.0f;
    
    attitude_state.gravity_vector.x = 0.0f;
    attitude_state.gravity_vector.y = 0.0f;
    attitude_state.gravity_vector.z = 1.0f;
    
    attitude_state.roll_rate = 0.0f;
    attitude_state.pitch_rate = 0.0f;
    attitude_state.yaw_rate = 0.0f;
    
    attitude_state.update_count = 0;
    attitude_state.last_update_time = system_get_time_ms();
    
    attitude_initialized = true;
    return true;
}

/**
 * @brief 更新姿态解算
 */
bool attitude_update(vector3f_t *gyro_data, vector3f_t *accel_data, 
                    vector3f_t *mag_data, float dt) {
    // 使用(void)标记未使用的参数，避免编译警告
    // 注意：mag_data 虽然在某些算法中使用，但在当前实现中需要明确标记
    (void)mag_data;
    
    if (!attitude_initialized || gyro_data == NULL || accel_data == NULL) {
        return false;
    }
    
    // 保存数据
    attitude_state.angular_velocity = *gyro_data;
    attitude_state.linear_acceleration = *accel_data;
    
    // 根据选择的算法进行更新
    switch (attitude_config.algorithm) {
        case ATTITUDE_ALGORITHM_COMPLEMENTARY:
            complementary_filter_update(gyro_data, accel_data, dt);
            break;
        case ATTITUDE_ALGORITHM_MADGWICK:
            madgwick_filter_update(gyro_data, accel_data, mag_data, dt);
            break;
        case ATTITUDE_ALGORITHM_MAHONY:
            mahony_filter_update(gyro_data, accel_data, mag_data, dt);
            break;
        default:
            return false;
    }
    
    // 将四元数转换为欧拉角
    quaternion_to_euler(&attitude_state.quaternion, &attitude_state.euler);
    
    // 更新重力向量
    attitude_state.gravity_vector.x = 2.0f * (
        attitude_state.quaternion.x * attitude_state.quaternion.z - 
        attitude_state.quaternion.w * attitude_state.quaternion.y);
    
    attitude_state.gravity_vector.y = 2.0f * (
        attitude_state.quaternion.w * attitude_state.quaternion.x + 
        attitude_state.quaternion.y * attitude_state.quaternion.z);
    
    attitude_state.gravity_vector.z = (
        attitude_state.quaternion.w * attitude_state.quaternion.w - 
        attitude_state.quaternion.x * attitude_state.quaternion.x - 
        attitude_state.quaternion.y * attitude_state.quaternion.y + 
        attitude_state.quaternion.z * attitude_state.quaternion.z);
    
    // 更新状态
    attitude_state.update_count++;
    attitude_state.last_update_time = system_get_time_ms();
    
    return true;
}

/**
 * @brief 获取当前姿态状态
 */
bool attitude_get_state(attitude_state_t *state) {
    if (!attitude_initialized || state == NULL) {
        return false;
    }
    
    *state = attitude_state;
    return true;
}

/**
 * @brief 设置四元数
 */
void attitude_set_quaternion(quaternion_t *quat) {
    if (quat != NULL) {
        attitude_state.quaternion = *quat;
        quaternion_normalize(&attitude_state.quaternion);
        
        // 更新欧拉角
        quaternion_to_euler(&attitude_state.quaternion, &attitude_state.euler);
    }
}

/**
 * @brief 设置欧拉角
 */
void attitude_set_euler(euler_angle_t *euler) {
    if (euler != NULL) {
        euler_to_quaternion(euler, &attitude_state.quaternion);
        quaternion_normalize(&attitude_state.quaternion);
        
        // 保存欧拉角
        attitude_state.euler = *euler;
    }
}

/**
 * @brief 将四元数转换为欧拉角
 */
void quaternion_to_euler(quaternion_t *quat, euler_angle_t *euler) {
    if (quat == NULL || euler == NULL) {
        return;
    }
    
    // 归一化四元数
    quaternion_t q = *quat;
    quaternion_normalize(&q);
    
    // 计算欧拉角 (Z-Y-X顺序，即偏航-俯仰-横滚)
    float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    euler->roll = atan2f(sinr_cosp, cosr_cosp) * RAD_TO_DEG;
    
    float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    if (fabs(sinp) >= 1.0f) {
        euler->pitch = copysignf(90.0f, sinp);
    } else {
        euler->pitch = asinf(sinp) * RAD_TO_DEG;
    }
    
    float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    euler->yaw = atan2f(siny_cosp, cosy_cosp) * RAD_TO_DEG;
}

/**
 * @brief 将欧拉角转换为四元数
 */
void euler_to_quaternion(euler_angle_t *euler, quaternion_t *quat) {
    if (euler == NULL || quat == NULL) {
        return;
    }
    
    // 将角度转换为弧度
    float roll_rad = euler->roll * DEG_TO_RAD * 0.5f;
    float pitch_rad = euler->pitch * DEG_TO_RAD * 0.5f;
    float yaw_rad = euler->yaw * DEG_TO_RAD * 0.5f;
    
    // 计算正弦和余弦
    float cr = cosf(roll_rad);
    float sr = sinf(roll_rad);
    float cp = cosf(pitch_rad);
    float sp = sinf(pitch_rad);
    float cy = cosf(yaw_rad);
    float sy = sinf(yaw_rad);
    
    // 计算四元数 (Z-Y-X顺序)
    quat->w = cr * cp * cy + sr * sp * sy;
    quat->x = sr * cp * cy - cr * sp * sy;
    quat->y = cr * sp * cy + sr * cp * sy;
    quat->z = cr * cp * sy - sr * sp * cy;
    
    // 归一化
    quaternion_normalize(quat);
}

/**
 * @brief 互补滤波姿态更新
 */
void complementary_filter_update(vector3f_t *gyro_data, vector3f_t *accel_data, float dt) {
    // 局部变量
    float alpha = attitude_config.complementary_gain;
    float roll_accel, pitch_accel;
    
    // 归一化加速度计数据
    vector3f_t accel_norm = *accel_data;
    vector_normalize(&accel_norm);
    
    // 从加速度计计算姿态
    roll_accel = atan2f(accel_norm.y, accel_norm.z) * RAD_TO_DEG;
    pitch_accel = atan2f(-accel_norm.x, sqrtf(accel_norm.y * accel_norm.y + accel_norm.z * accel_norm.z)) * RAD_TO_DEG;
    
    // 互补滤波
    attitude_state.euler.roll = alpha * (attitude_state.euler.roll + gyro_data->x * dt) + (1.0f - alpha) * roll_accel;
    attitude_state.euler.pitch = alpha * (attitude_state.euler.pitch + gyro_data->y * dt) + (1.0f - alpha) * pitch_accel;
    
    // 偏航角只能从陀螺仪积分获取
    attitude_state.euler.yaw += gyro_data->z * dt;
    
    // 将欧拉角转回四元数
    euler_to_quaternion(&attitude_state.euler, &attitude_state.quaternion);
}

/**
 * @brief Madgwick算法姿态更新
 */
void madgwick_filter_update(vector3f_t *gyro_data, vector3f_t *accel_data, 
                            vector3f_t *mag_data, float dt) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)mag_data;
    quaternion_t q = attitude_state.quaternion;
    float beta = attitude_config.beta;
    
    // 归一化陀螺仪数据 (转换为弧度/秒)
    vector3f_t gyro_rad;
    gyro_rad.x = gyro_data->x * DEG_TO_RAD;
    gyro_rad.y = gyro_data->y * DEG_TO_RAD;
    gyro_rad.z = gyro_data->z * DEG_TO_RAD;
    
    // 归一化加速度计数据
    vector3f_t accel_norm = *accel_data;
    vector_normalize(&accel_norm);
    
    // 辅助变量
    float q1 = q.w;
    float q2 = q.x;
    float q3 = q.y;
    float q4 = q.z;
    
    // 梯度下降算法部分
    float f1 = 2.0f * (q2 * q4 - q1 * q3) - accel_norm.x;
    float f2 = 2.0f * (q1 * q2 + q3 * q4) - accel_norm.y;
    float f3 = 2.0f * (0.5f - q2 * q2 - q3 * q3) - accel_norm.z;
    
    // 雅可比矩阵
    // 仅保留实际使用的变量，注释掉未使用的变量
    // float J11 = -2.0f * q3;
    float J12 = 2.0f * q4;
    float J13 = -2.0f * q1;
    float J14 = 2.0f * q2;
    
    // float J21 = 2.0f * q2;
    float J22 = 2.0f * q1;
    float J23 = 2.0f * q4;
    float J24 = 2.0f * q3;
    
    // float J31 = 0.0f;
    float J32 = -4.0f * q2;
    float J33 = -4.0f * q3;
    float J34 = 0.0f;
    
    // 梯度
    // 注释掉未使用的变量
    // float gradient_x = 2.0f * (J11 * f1 + J21 * f2 + J31 * f3);
    float gradient_y = 2.0f * (J12 * f1 + J22 * f2 + J32 * f3);
    float gradient_z = 2.0f * (J13 * f1 + J23 * f2 + J33 * f3);
    float gradient_w = 2.0f * (J14 * f1 + J24 * f2 + J34 * f3);
    
    // 归一化梯度
    vector3f_t gradient = {gradient_y, gradient_z, gradient_w};
    vector_normalize(&gradient);
    
    // 更新四元数
    float qDot1 = 0.5f * (-q2 * gyro_rad.x - q3 * gyro_rad.y - q4 * gyro_rad.z) - beta * gradient_w;
    float qDot2 = 0.5f * (q1 * gyro_rad.x + q3 * gyro_rad.z - q4 * gyro_rad.y) - beta * gradient.x;
    float qDot3 = 0.5f * (q1 * gyro_rad.y - q2 * gyro_rad.z + q4 * gyro_rad.x) - beta * gradient.y;
    float qDot4 = 0.5f * (q1 * gyro_rad.z + q2 * gyro_rad.y - q3 * gyro_rad.x) - beta * gradient.z;
    
    // 积分更新四元数
    q1 += qDot1 * dt;
    q2 += qDot2 * dt;
    q3 += qDot3 * dt;
    q4 += qDot4 * dt;
    
    // 更新并归一化四元数
    attitude_state.quaternion.w = q1;
    attitude_state.quaternion.x = q2;
    attitude_state.quaternion.y = q3;
    attitude_state.quaternion.z = q4;
    
    quaternion_normalize(&attitude_state.quaternion);
}

/**
 * @brief Mahony算法姿态更新
 */
void mahony_filter_update(vector3f_t *gyro_data, vector3f_t *accel_data, 
                          vector3f_t *mag_data, float dt) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)mag_data;
    quaternion_t q = attitude_state.quaternion;
    float kp = attitude_config.beta; // 比例增益
    
    // 归一化陀螺仪数据 (转换为弧度/秒)
    vector3f_t gyro_rad;
    gyro_rad.x = gyro_data->x * DEG_TO_RAD;
    gyro_rad.y = gyro_data->y * DEG_TO_RAD;
    gyro_rad.z = gyro_data->z * DEG_TO_RAD;
    
    // 归一化加速度计数据
    vector3f_t accel_norm = *accel_data;
    vector_normalize(&accel_norm);
    
    // 辅助变量
    float q1 = q.w;
    float q2 = q.x;
    float q3 = q.y;
    float q4 = q.z;
    
    // 估算的重力向量
    vector3f_t v = {
        2.0f * (q2 * q4 - q1 * q3),
        2.0f * (q1 * q2 + q3 * q4),
        q1 * q1 - q2 * q2 - q3 * q3 + q4 * q4
    };
    
    // 计算误差向量
    vector3f_t e;
    vector_cross_product(&accel_norm, &v, &e);
    
    // 使用误差向量计算反馈
    vector3f_t w_err = {
        kp * e.x,
        kp * e.y,
        kp * e.z
    };
    
    // 将反馈加到陀螺仪数据上
    vector3f_t omega = {
        gyro_rad.x + w_err.x,
        gyro_rad.y + w_err.y,
        gyro_rad.z + w_err.z
    };
    
    // 更新四元数
    float qDot1 = -0.5f * (q2 * omega.x + q3 * omega.y + q4 * omega.z);
    float qDot2 = 0.5f * (q1 * omega.x + q3 * omega.z - q4 * omega.y);
    float qDot3 = 0.5f * (q1 * omega.y - q2 * omega.z + q4 * omega.x);
    float qDot4 = 0.5f * (q1 * omega.z + q2 * omega.y - q3 * omega.x);
    
    // 积分更新四元数
    q1 += qDot1 * dt;
    q2 += qDot2 * dt;
    q3 += qDot3 * dt;
    q4 += qDot4 * dt;
    
    // 更新并归一化四元数
    attitude_state.quaternion.w = q1;
    attitude_state.quaternion.x = q2;
    attitude_state.quaternion.y = q3;
    attitude_state.quaternion.z = q4;
    
    quaternion_normalize(&attitude_state.quaternion);
}

/**
 * @brief 归一化四元数
 */
void quaternion_normalize(quaternion_t *quat) {
    if (quat == NULL) {
        return;
    }
    
    // 计算模长
    float norm = sqrtf(
        quat->w * quat->w +
        quat->x * quat->x +
        quat->y * quat->y +
        quat->z * quat->z
    );
    
    // 避免除零
    if (norm > 0.0f) {
        // 归一化
        float inv_norm = 1.0f / norm;
        quat->w *= inv_norm;
        quat->x *= inv_norm;
        quat->y *= inv_norm;
        quat->z *= inv_norm;
    }
}

/**
 * @brief 计算向量叉积
 */
void vector_cross_product(vector3f_t *a, vector3f_t *b, vector3f_t *result) {
    if (a == NULL || b == NULL || result == NULL) {
        return;
    }
    
    result->x = a->y * b->z - a->z * b->y;
    result->y = a->z * b->x - a->x * b->z;
    result->z = a->x * b->y - a->y * b->x;
}

/**
 * @brief 计算向量点积
 */
float vector_dot_product(vector3f_t *a, vector3f_t *b) {
    if (a == NULL || b == NULL) {
        return 0.0f;
    }
    
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

/**
 * @brief 归一化向量
 */
void vector_normalize(vector3f_t *vec) {
    if (vec == NULL) {
        return;
    }
    
    // 计算向量长度
    float length = sqrtf(
        vec->x * vec->x +
        vec->y * vec->y +
        vec->z * vec->z
    );
    
    // 避免除零
    if (length > 0.0f) {
        // 归一化
        float inv_length = 1.0f / length;
        vec->x *= inv_length;
        vec->y *= inv_length;
        vec->z *= inv_length;
    }
}