/*
 * 解锁检测模块实现
 */

#include "arming_detector.h"
#include "../service/system.h"
#include "../config/config.h"
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

// 静态变量
static arming_detector_config_t detector_config;
static arming_detector_status_t detector_status;
static bool detector_initialized = false;

// 默认配置
static const arming_detector_config_t DEFAULT_CONFIG = {
    .throttle_threshold = 0.05f,
    .roll_angle_threshold = 20.0f,
    .pitch_angle_threshold = 20.0f,
    .arm_delay_ms = 500,
    .arm_check_interval_ms = 50
};

/**
 * @brief 初始化解锁检测模块
 */
bool arming_detector_init(arming_detector_config_t *config) {
    // 使用默认配置或用户配置
    if (config != NULL) {
        detector_config = *config;
    } else {
        detector_config = DEFAULT_CONFIG;
    }
    
    // 初始化状态
    detector_status.state = ARMING_STATE_DISARMED;
    detector_status.conditions_met = false;
    detector_status.condition_check_time = 0;
    detector_status.arm_request_time = 0;
    detector_status.safety_switch_state = false;
    detector_status.last_arm_command_time = 0;
    
    detector_initialized = true;
    system_log(LOG_LEVEL_INFO, "解锁检测模块初始化成功\n");
    return true;
}

/**
 * @brief 检查解锁条件是否满足
 */
static bool check_arming_conditions(float throttle, float roll, float pitch, bool imu_ready) {
    // 检查所有条件：油门低、姿态稳定、IMU就绪
    bool throttle_ok = (throttle <= detector_config.throttle_threshold);
    bool attitude_ok = (fabs(roll) <= detector_config.roll_angle_threshold) && \
                       (fabs(pitch) <= detector_config.pitch_angle_threshold);
    bool imu_ok = imu_ready;
    bool safety_ok = !detector_status.safety_switch_state; // 安全开关关闭时允许解锁
    
    return throttle_ok && attitude_ok && imu_ok && safety_ok;
}

/**
 * @brief 更新解锁检测状态
 */
arming_state_t arming_detector_update(float throttle, float roll, float pitch, bool imu_ready) {
    if (!detector_initialized) {
        return ARMING_STATE_DISARMED;
    }
    
    uint32_t current_time = system_get_time_ms();
    
    // 定期检查解锁条件
    if (current_time - detector_status.condition_check_time >= detector_config.arm_check_interval_ms) {
        detector_status.conditions_met = check_arming_conditions(throttle, roll, pitch, imu_ready);
        detector_status.condition_check_time = current_time;
    }
    
    // 根据状态机处理不同状态
    switch (detector_status.state) {
        case ARMING_STATE_DISARMED:
            if (detector_status.conditions_met) {
                detector_status.state = ARMING_STATE_READY_TO_ARM;
            }
            break;
            
        case ARMING_STATE_READY_TO_ARM:
            if (!detector_status.conditions_met) {
                detector_status.state = ARMING_STATE_DISARMED;
            }
            // 如果有解锁请求且条件满足，开始解锁过程
            if (detector_status.arm_request_time && \
                current_time - detector_status.arm_request_time >= detector_config.arm_delay_ms) {
                detector_status.state = ARMING_STATE_ARMED;
                system_log(LOG_LEVEL_INFO, "无人机已解锁\n");
            }
            break;
            
        case ARMING_STATE_ARMING:
            // 解锁过程中，如果条件不满足，取消解锁
            if (!detector_status.conditions_met) {
                detector_status.state = ARMING_STATE_DISARMED;
                detector_status.arm_request_time = 0;
                system_log(LOG_LEVEL_WARNING, "解锁条件不满足，解锁取消\n");
            }
            break;
            
        case ARMING_STATE_ARMED:
            // 检查是否需要强制锁定
            if (!detector_status.conditions_met) {
                detector_status.state = ARMING_STATE_DISARMING;
                system_log(LOG_LEVEL_WARNING, "条件不满足，正在锁定\n");
            }
            break;
            
        case ARMING_STATE_DISARMING:
            // 完成锁定
            detector_status.state = ARMING_STATE_DISARMED;
            detector_status.arm_request_time = 0;
            system_log(LOG_LEVEL_INFO, "无人机已锁定\n");
            break;
    }
    
    return detector_status.state;
}

/**
 * @brief 请求解锁
 */
bool arming_detector_request_arm(void) {
    if (!detector_initialized) {
        return false;
    }
    
    // 只有在准备解锁状态才能请求解锁
    if (detector_status.state == ARMING_STATE_READY_TO_ARM) {
        detector_status.arm_request_time = system_get_time_ms();
        detector_status.state = ARMING_STATE_ARMING;
        detector_status.last_arm_command_time = detector_status.arm_request_time;
        system_log(LOG_LEVEL_INFO, "收到解锁请求，正在验证条件...\n");
        return true;
    } else if (detector_status.state == ARMING_STATE_ARMED) {
        // 已经解锁，直接返回成功
        return true;
    } else {
        system_log(LOG_LEVEL_WARNING, "无法解锁：解锁条件不满足\n");
        return false;
    }
}

/**
 * @brief 请求锁定
 */
bool arming_detector_request_disarm(void) {
    if (!detector_initialized) {
        return false;
    }
    
    if (detector_status.state == ARMING_STATE_ARMED) {
        detector_status.state = ARMING_STATE_DISARMING;
        detector_status.arm_request_time = 0;
        system_log(LOG_LEVEL_INFO, "收到锁定请求\n");
        return true;
    } else if (detector_status.state != ARMING_STATE_DISARMED) {
        // 如果在其他状态，直接设为锁定中
        detector_status.state = ARMING_STATE_DISARMING;
        detector_status.arm_request_time = 0;
        return true;
    }
    
    return false; // 已经是锁定状态
}

/**
 * @brief 获取解锁状态
 */
bool arming_detector_get_status(arming_detector_status_t *status) {
    if (!detector_initialized || status == NULL) {
        return false;
    }
    
    *status = detector_status;
    return true;
}

/**
 * @brief 设置安全开关状态
 */
void arming_detector_set_safety_switch(bool state) {
    if (!detector_initialized) {
        return;
    }
    
    detector_status.safety_switch_state = state;
    
    // 如果安全开关打开，强制锁定
    if (state && detector_status.state != ARMING_STATE_DISARMED) {
        detector_status.state = ARMING_STATE_DISARMING;
        system_log(LOG_LEVEL_WARNING, "安全开关打开，强制锁定\n");
    }
}

/**
 * @brief 强制锁定
 */
bool arming_detector_force_disarm(void) {
    if (!detector_initialized) {
        return false;
    }
    
    detector_status.state = ARMING_STATE_DISARMING;
    detector_status.arm_request_time = 0;
    system_log(LOG_LEVEL_INFO, "强制锁定无人机\n");
    return true;
}

/**
 * @brief 检查是否可以解锁
 */
bool arming_detector_can_arm(void) {
    if (!detector_initialized) {
        return false;
    }
    
    return detector_status.conditions_met;
}