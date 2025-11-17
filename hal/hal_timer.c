/*
 * 定时器硬件抽象层实现 - STM32风格
 */

#include "hal_timer.h"
#include <stdio.h>

// STM32定时器寄存器位定义
// CR1寄存器位定义
#define TIM_CR1_CEN_Pos              (0U)
#define TIM_CR1_CEN_Msk              (1U << TIM_CR1_CEN_Pos)
#define TIM_CR1_CEN                  TIM_CR1_CEN_Msk  // 计数器使能

#define TIM_CR1_ARPE_Pos             (7U)
#define TIM_CR1_ARPE_Msk             (1U << TIM_CR1_ARPE_Pos)
#define TIM_CR1_ARPE                 TIM_CR1_ARPE_Msk  // 自动重载预加载使能

// CCMR1寄存器位定义
#define TIM_CCMR1_OC1M_Pos           (4U)
#define TIM_CCMR1_OC1M_Msk           (0x7U << TIM_CCMR1_OC1M_Pos)
#define TIM_CCMR1_OC1M_1             (1U << (TIM_CCMR1_OC1M_Pos + 1))
#define TIM_CCMR1_OC1M_2             (1U << (TIM_CCMR1_OC1M_Pos + 2))

// CCER寄存器位定义
#define TIM_CCER_CC1E_Pos            (0U)
#define TIM_CCER_CC1E_Msk            (1U << TIM_CCER_CC1E_Pos)
#define TIM_CCER_CC1E                TIM_CCER_CC1E_Msk  // 通道1输出使能

#define TIM_CCER_CC1P_Pos            (1U)
#define TIM_CCER_CC1P_Msk            (1U << TIM_CCER_CC1P_Pos)
#define TIM_CCER_CC1P                TIM_CCER_CC1P_Msk  // 通道1极性

// 其他通道的位定义（为了完整性）
#define TIM_CCER_CC2E_Pos            (4U)
#define TIM_CCER_CC2E_Msk            (1U << TIM_CCER_CC2E_Pos)
#define TIM_CCER_CC2E                TIM_CCER_CC2E_Msk  // 通道2输出使能

#define TIM_CCER_CC3E_Pos            (8U)
#define TIM_CCER_CC3E_Msk            (1U << TIM_CCER_CC3E_Pos)
#define TIM_CCER_CC3E                TIM_CCER_CC3E_Msk  // 通道3输出使能

#define TIM_CCER_CC4E_Pos            (12U)
#define TIM_CCER_CC4E_Msk            (1U << TIM_CCER_CC4E_Pos)
#define TIM_CCER_CC4E                TIM_CCER_CC4E_Msk  // 通道4输出使能

// 模拟STM32定时器结构体
typedef struct {
    volatile uint32_t CR1;    // 控制寄存器1
    volatile uint32_t CR2;    // 控制寄存器2
    volatile uint32_t SMCR;   // 从模式控制寄存器
    volatile uint32_t DIER;   // DMA中断使能寄存器
    volatile uint32_t SR;     // 状态寄存器
    volatile uint32_t EGR;    // 事件生成寄存器
    volatile uint32_t CCMR1;  // 捕获/比较模式寄存器1
    volatile uint32_t CCMR2;  // 捕获/比较模式寄存器2
    volatile uint32_t CCER;   // 捕获/比较使能寄存器
    volatile uint32_t CNT;    // 计数器
    volatile uint32_t PSC;    // 预分频器
    volatile uint32_t ARR;    // 自动重载寄存器
    volatile uint32_t RCR;    // 重复计数器寄存器
    volatile uint32_t CCR1;   // 捕获/比较寄存器1
    volatile uint32_t CCR2;   // 捕获/比较寄存器2
    volatile uint32_t CCR3;   // 捕获/比较寄存器3
    volatile uint32_t CCR4;   // 捕获/比较寄存器4
    volatile uint32_t BDTR;   // 刹车和死区寄存器
    volatile uint32_t DCR;    // DMA控制寄存器
    volatile uint32_t DMAR;   // DMA地址寄存器
} stm32_timer_t;

// 模拟STM32定时器基地址
#define STM32_TIM1_BASE    ((stm32_timer_t *)0x40012C00)
#define STM32_TIM2_BASE    ((stm32_timer_t *)0x40000000)
#define STM32_TIM3_BASE    ((stm32_timer_t *)0x40000400)
#define STM32_TIM4_BASE    ((stm32_timer_t *)0x40000800)
#define STM32_TIM5_BASE    ((stm32_timer_t *)0x40000C00)

// 模拟定时器实例数组 (用于演示)
static stm32_timer_t *timer_instances[TIMER_MAX] = {
    STM32_TIM1_BASE,  // TIMER_1
    STM32_TIM2_BASE,  // TIMER_2
    STM32_TIM3_BASE,  // TIMER_3
    STM32_TIM4_BASE,  // TIMER_4
    STM32_TIM5_BASE,  // TIMER_5
    NULL,             // TIMER_6 及以上暂时不实现
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

// 保存每个定时器的PWM配置信息
typedef struct {
    timer_output_channel_t channel;    // PWM输出通道
    bool initialized;                  // 是否已初始化
    float last_duty_cycle;             // 上次设置的占空比
} timer_pwm_info_t;

static timer_pwm_info_t pwm_info[TIMER_MAX];

/**
 * @brief 初始化定时器硬件抽象层
 * @return 成功返回true，失败返回false
 */
static bool hal_timer_hal_init(void) {
    // 初始化PWM配置信息数组
    for (int i = 0; i < TIMER_MAX; i++) {
        pwm_info[i].channel = TIMER_CHANNEL_1;    // 默认使用通道1
        pwm_info[i].initialized = false;          // 初始状态为未初始化
        pwm_info[i].last_duty_cycle = 0.0f;       // 初始占空比为0
        
        // 对于真实的定时器，设置默认的ARR值
        if (timer_instances[i] != NULL) {
            // 设置一个合理的默认ARR值，这将决定PWM的分辨率
            timer_instances[i]->ARR = 1000;  // 假设PWM频率为1kHz (1MHz / 1000)
            timer_instances[i]->PSC = 71;    // 假设系统时钟为72MHz，预分频后为1MHz
            
            // 初始化计数器为0
            timer_instances[i]->CNT = 0;
            
            // 重置CCR寄存器
            timer_instances[i]->CCR1 = 0;
            timer_instances[i]->CCR2 = 0;
            timer_instances[i]->CCR3 = 0;
            timer_instances[i]->CCR4 = 0;
        }
    }
    
    printf("STM32定时器HAL初始化完成\n");
    return true;
}

/* ============ ADC模块实现 - STM32库风格 ============ */

/**
 * @brief ADC状态标志位
 */
#define ADC_STATE_RESET             (0x00000000U)
#define ADC_STATE_ERROR             (0x00000001U)
#define ADC_STATE_BUSY              (0x00000002U)
#define ADC_STATE_EOC               (0x00000004U)
#define ADC_STATE_CAL              (0x00000008U)
#define ADC_STATE_CAL_DONE         (0x00000010U)
#define ADC_STATE_AWD              (0x00000020U)
#define ADC_STATE_READY            (0x00000080U)

/* ADC实例数组 */
static ADC_TypeDef* adc_instances[] = {
    ADC1,  /* ADC实例0 */
    ADC2,  /* ADC实例1 */
    ADC3   /* ADC实例2 */
};

/* ADC句柄数组 */
static adc_handle_t adc_handles[3] = {0};

/* ADC回调函数类型 */
typedef void (*adc_callback_t)(void);
static adc_callback_t adc_callbacks[3] = {0};

/**
 * @brief 初始化ADC
 * @param adc_handle ADC句柄指针
 * @param config ADC配置指针
 * @return 成功返回true，失败返回false
 */
bool adc_init(adc_handle_t *adc_handle, adc_config_t *config) {
    // 参数验证
    if (adc_handle == NULL || config == NULL) {
        return false;
    }
    
    // 验证ADC实例
    if (config->Instance == NULL) {
        return false;
    }
    
    // 查找ADC实例索引
    uint32_t adc_index = 0;
    bool found = false;
    for (adc_index = 0; adc_index < 3; adc_index++) {
        if (adc_instances[adc_index] == config->Instance) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        return false;
    }
    
    // 保存配置
    adc_handle->Instance = config->Instance;
    adc_handle->Init = *config;
    adc_handle->State = ADC_STATE_RESET;
    adc_handle->ErrorCode = ADC_ERROR_NONE;
    
    // 执行ADC初始化序列
    ADC_TypeDef *adc = config->Instance;
    
    // 禁用ADC
    adc->CR2 &= ~ADC_CR2_ADON;
    
    // 等待ADC稳定（延时）
    for (volatile uint32_t i = 0; i < 1000; i++) {
        // 简单延时
    }
    
    // 配置ADC参数
    uint32_t cr2_value = 0;
    
    // 设置分辨率
    switch (config->Resolution) {
        case ADC_RESOLUTION_12BIT:
            // 12位模式，默认值
            break;
        case ADC_RESOLUTION_10BIT:
            cr2_value |= (1U << 16);  // 设置10位分辨率
            break;
        case ADC_RESOLUTION_8BIT:
            cr2_value |= (2U << 16);  // 设置8位分辨率
            break;
        case ADC_RESOLUTION_6BIT:
            cr2_value |= (3U << 16);  // 设置6位分辨率
            break;
    }
    
    // 设置数据对齐
    if (config->DataAlignment == ADC_ALIGNMENT_LEFT) {
        cr2_value |= ADC_CR2_ALIGN;
    }
    
    // 设置扫描模式
    if (config->ScanConvMode == ADC_SCAN_MODE_ENABLED) {
        cr2_value |= ADC_CR2_SCAN;
    }
    
    // 设置连续转换模式
    if (config->ContinuousConvMode == ADC_CONTINUOUS_MODE_ENABLED) {
        cr2_value |= ADC_CR2_CONT;
    }
    
    // 配置转换数量
    uint32_t smpr2_value = 0;
    if (config->NbrOfConversion > 0) {
        // 配置采样时间（使用默认采样时间）
        smpr2_value = 0xFFFFFFFF;  // 所有通道使用最大采样时间
        adc->SMPR2 = smpr2_value;
    }
    
    // 配置规则转换序列长度
    adc->SQR1 = (config->NbrOfConversion - 1) & 0x0F;
    
    // 保存到全局句柄数组
    adc_handles[adc_index] = *adc_handle;
    
    // 设置状态为就绪
    adc_handle->State = ADC_STATE_READY;
    
    printf("ADC初始化成功: ADC实例 %p, 分辨率 %u位, 转换数量 %u\n", 
           config->Instance, config->Resolution + 6, config->NbrOfConversion);
    
    return true;
}

/**
 * @brief 校准ADC
 * @param adc_handle ADC句柄指针
 * @return 成功返回true，失败返回false
 */
bool adc_calibrate(adc_handle_t *adc_handle) {
    // 参数验证
    if (adc_handle == NULL || adc_handle->Instance == NULL) {
        return false;
    }
    
    // 检查ADC是否已初始化
    if (adc_handle->State != ADC_STATE_READY) {
        return false;
    }
    
    ADC_TypeDef *adc = adc_handle->Instance;
    
    // 启用ADC（如果还未启用）
    adc->CR2 |= ADC_CR2_ADON;
    
    // 等待ADC稳定
    for (volatile uint32_t i = 0; i < 20000; i++) {
        // 延时等待ADC稳定
    }
    
    // 开始校准
    adc->CR2 |= ADC_CR2_CAL;
    
    // 等待校准完成
    uint32_t timeout = 10000;
    while ((adc->CR2 & ADC_CR2_CAL) && (timeout > 0)) {
        timeout--;
        // 延时等待校准完成
        for (volatile uint32_t i = 0; i < 100; i++);
    }
    
    if (timeout == 0) {
        adc_handle->ErrorCode = ADC_ERROR_CALIBRATION;
        adc_handle->State = ADC_STATE_ERROR;
        return false;
    }
    
    // 再次等待ADC稳定
    for (volatile uint32_t i = 0; i < 20000; i++) {
        // 延时等待ADC稳定
    }
    
    adc_handle->State = ADC_STATE_CAL_DONE;
    
    printf("ADC校准成功: ADC实例 %p\n", adc_handle->Instance);
    return true;
}

/**
 * @brief 启用ADC
 * @param adc_handle ADC句柄指针
 * @return 成功返回true，失败返回false
 */
bool adc_enable(adc_handle_t *adc_handle) {
    // 参数验证
    if (adc_handle == NULL || adc_handle->Instance == NULL) {
        return false;
    }
    
    // 检查状态
    if (adc_handle->State == ADC_STATE_ERROR) {
        return false;
    }
    
    ADC_TypeDef *adc = adc_handle->Instance;
    
    // 启用ADC
    adc->CR2 |= ADC_CR2_ADON;
    
    // 等待ADC启动完成
    uint32_t timeout = 10000;
    while ((adc->SR & ADC_SR_AWD) && (timeout > 0)) {
        timeout--;
        for (volatile uint32_t i = 0; i < 100; i++);
    }
    
    adc_handle->State = ADC_STATE_READY;
    
    printf("ADC启用成功: ADC实例 %p\n", adc_handle->Instance);
    return true;
}

/**
 * @brief 禁用ADC
 * @param adc_handle ADC句柄指针
 * @return 成功返回true，失败返回false
 */
bool adc_disable(adc_handle_t *adc_handle) {
    // 参数验证
    if (adc_handle == NULL || adc_handle->Instance == NULL) {
        return false;
    }
    
    ADC_TypeDef *adc = adc_handle->Instance;
    
    // 停止任何正在进行的转换
    adc->CR2 &= ~ADC_CR2_SWSTART;  // 停止规则转换启动
    adc->CR2 &= ~ADC_CR2_JSWSTART; // 停止注入转换启动
    
    // 禁用ADC
    adc->CR2 &= ~ADC_CR2_ADON;
    
    // 清除状态
    adc_handle->State = ADC_STATE_RESET;
    
    printf("ADC禁用成功: ADC实例 %p\n", adc_handle->Instance);
    return true;
}

/**
 * @brief 启动ADC转换
 * @param adc_handle ADC句柄指针
 * @param channel ADC通道
 * @return 成功返回true，失败返回false
 */
bool adc_start_conversion(adc_handle_t *adc_handle, adc_channel_t channel) {
    // 参数验证
    if (adc_handle == NULL || adc_handle->Instance == NULL) {
        return false;
    }
    
    // 检查状态
    if (adc_handle->State == ADC_STATE_ERROR) {
        return false;
    }
    
    // 配置通道
    if (!adc_config_channel(adc_handle, channel, 1)) {
        return false;
    }
    
    ADC_TypeDef *adc = adc_handle->Instance;
    
    // 清除EOC标志
    adc->SR &= ~ADC_SR_EOC;
    
    // 启动转换
    adc->CR2 |= ADC_CR2_SWSTART;
    
    adc_handle->State = ADC_STATE_BUSY;
    
    printf("ADC转换启动: 通道 %u\n", channel);
    return true;
}

/**
 * @brief 停止ADC转换
 * @param adc_handle ADC句柄指针
 * @return 成功返回true，失败返回false
 */
bool adc_stop_conversion(adc_handle_t *adc_handle) {
    // 参数验证
    if (adc_handle == NULL || adc_handle->Instance == NULL) {
        return false;
    }
    
    ADC_TypeDef *adc = adc_handle->Instance;
    
    // 停止转换
    adc->CR2 &= ~ADC_CR2_SWSTART;  // 停止规则转换启动
    adc->CR2 &= ~ADC_CR2_JSWSTART; // 停止注入转换启动
    
    adc_handle->State = ADC_STATE_READY;
    
    printf("ADC转换停止\n");
    return true;
}

/**
 * @brief 配置ADC通道
 * @param adc_handle ADC句柄指针
 * @param channel ADC通道
 * @param rank 通道排名
 * @return 成功返回true，失败返回false
 */
bool adc_config_channel(adc_handle_t *adc_handle, adc_channel_t channel, uint32_t rank) {
    // 参数验证
    if (adc_handle == NULL || adc_handle->Instance == NULL) {
        return false;
    }
    
    if (channel >= ADC_CHANNEL_VBAT || rank == 0 || rank > 16) {
        return false;
    }
    
    ADC_TypeDef *adc = adc_handle->Instance;
    
    // 配置规则序列
    if (rank <= 6) {
        // 配置SQR序列
        adc->SQR3 = (channel << ((rank - 1) * 5));
    } else if (rank <= 12) {
        // 配置SQR2序列
        adc->SQR2 = (channel << ((rank - 7) * 5));
    } else {
        // 配置SQR1序列
        adc->SQR1 |= (channel << ((rank - 13) * 5));
    }
    
    printf("ADC通道配置: 通道 %u, 排名 %u\n", channel, rank);
    return true;
}

/**
 * @brief 读取ADC转换值
 * @param adc_handle ADC句柄指针
 * @return ADC转换值
 */
uint16_t adc_read_value(adc_handle_t *adc_handle) {
    // 参数验证
    if (adc_handle == NULL || adc_handle->Instance == NULL) {
        return 0;
    }
    
    // 等待转换完成
    ADC_TypeDef *adc = adc_handle->Instance;
    uint32_t timeout = 10000;
    while (!(adc->SR & ADC_SR_EOC) && (timeout > 0)) {
        timeout--;
        for (volatile uint32_t i = 0; i < 100; i++);
    }
    
    if (timeout == 0) {
        adc_handle->State = ADC_STATE_ERROR;
        adc_handle->ErrorCode = ADC_ERROR_TIMEOUT;
        return 0;
    }
    
    // 清除EOC标志
    adc->SR &= ~ADC_SR_EOC;
    
    // 读取转换值
    uint16_t value = (uint16_t)adc->DR;
    
    adc_handle->State = ADC_STATE_READY;
    
    return value;
}

/**
 * @brief 读取指定通道的ADC值
 * @param adc_handle ADC句柄指针
 * @param channel ADC通道
 * @return ADC转换值
 */
uint16_t adc_read_channel(adc_handle_t *adc_handle, adc_channel_t channel) {
    // 启动转换
    if (!adc_start_conversion(adc_handle, channel)) {
        return 0;
    }
    
    // 读取转换值
    return adc_read_value(adc_handle);
}

/**
 * @brief 读取ADC状态
 * @param adc_handle ADC句柄指针
 * @return ADC状态
 */
uint32_t adc_get_state(adc_handle_t *adc_handle) {
    if (adc_handle == NULL) {
        return ADC_STATE_ERROR;
    }
    return adc_handle->State;
}

/**
 * @brief 获取ADC错误代码
 * @param adc_handle ADC句柄指针
 * @return ADC错误代码
 */
uint32_t adc_get_error(adc_handle_t *adc_handle) {
    if (adc_handle == NULL) {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    return adc_handle->ErrorCode;
}

/**
 * @brief 配置ADC模拟看门狗
 * @param adc_handle ADC句柄指针
 * @param low_threshold 低阈值
 * @param high_threshold 高阈值
 * @param channel ADC通道
 * @return 成功返回true，失败返回false
 */
bool adc_analog_watchdog_config(adc_handle_t *adc_handle, uint16_t low_threshold, 
                                uint16_t high_threshold, adc_channel_t channel) {
    // 参数验证
    if (adc_handle == NULL || adc_handle->Instance == NULL) {
        return false;
    }
    
    ADC_TypeDef *adc = adc_handle->Instance;
    
    // 配置看门狗阈值
    adc->HTR = high_threshold;
    adc->LTR = low_threshold;
    
    // 配置看门狗通道
    // 这里简化实现，实际应该配置具体的看门狗通道
    
    printf("ADC模拟看门狗配置: 低阈值 %u, 高阈值 %u\n", low_threshold, high_threshold);
    return true;
}

/**
 * @brief 启用ADC模拟看门狗
 * @param adc_handle ADC句柄指针
 * @return 成功返回true，失败返回false
 */
bool adc_analog_watchdog_enable(adc_handle_t *adc_handle) {
    // 参数验证
    if (adc_handle == NULL || adc_handle->Instance == NULL) {
        return false;
    }
    
    ADC_TypeDef *adc = adc_handle->Instance;
    
    // 启用模拟看门狗
    adc->CR1 |= ADC_CR1_AWDEN;  // 启用规则通道看门狗
    
    printf("ADC模拟看门狗启用\n");
    return true;
}

/**
 * @brief 禁用ADC模拟看门狗
 * @param adc_handle ADC句柄指针
 * @return 成功返回true，失败返回false
 */
bool adc_analog_watchdog_disable(adc_handle_t *adc_handle) {
    // 参数验证
    if (adc_handle == NULL || adc_handle->Instance == NULL) {
        return false;
    }
    
    ADC_TypeDef *adc = adc_handle->Instance;
    
    // 禁用模拟看门狗
    adc->CR1 &= ~ADC_CR1_AWDEN;
    
    printf("ADC模拟看门狗禁用\n");
    return true;
}

/**
 * @brief ADC中断处理函数
 * @param adc_handle ADC句柄指针
 */
void adc_irq_handler(adc_handle_t *adc_handle) {
    // 参数验证
    if (adc_handle == NULL || adc_handle->Instance == NULL) {
        return;
    }
    
    ADC_TypeDef *adc = adc_handle->Instance;
    uint32_t it_source = adc->SR;
    
    // 检查EOC中断
    if (it_source & ADC_SR_EOC) {
        adc->SR &= ~ADC_SR_EOC;
        adc_handle->State = ADC_STATE_EOC;
        
        // 调用回调函数（如果配置了）
        // 这里需要根据实际的ADC实例找到对应的回调
        for (uint32_t i = 0; i < 3; i++) {
            if (&adc_handles[i] == adc_handle && adc_callbacks[i] != NULL) {
                adc_callbacks[i]();
                break;
            }
        }
    }
    
    // 检查模拟看门狗中断
    if (it_source & ADC_SR_AWD) {
        adc->SR &= ~ADC_SR_AWD;
        adc_handle->State = ADC_STATE_AWD;
        
        printf("ADC模拟看门狗触发\n");
    }
}

/**
 * @brief 启用ADC中断
 * @param adc_handle ADC句柄指针
 */
void adc_enable_irq(adc_handle_t *adc_handle) {
    // 参数验证
    if (adc_handle == NULL || adc_handle->Instance == NULL) {
        return;
    }
    
    ADC_TypeDef *adc = adc_handle->Instance;
    
    // 启用EOC中断
    adc->CR1 |= ADC_CR1_EOCIE;
    
    printf("ADC中断启用\n");
}

/**
 * @brief 禁用ADC中断
 * @param adc_handle ADC句柄指针
 */
void adc_disable_irq(adc_handle_t *adc_handle) {
    // 参数验证
    if (adc_handle == NULL || adc_handle->Instance == NULL) {
        return;
    }
    
    ADC_TypeDef *adc = adc_handle->Instance;
    
    // 禁用EOC中断
    adc->CR1 &= ~ADC_CR1_EOCIE;
    
    printf("ADC中断禁用\n");
}

/**
 * @brief 初始化所有定时器
 * @return 成功返回true，失败返回false
 */
bool timer_init_all(void) {
    // 调用HAL初始化函数
    return hal_timer_hal_init();
}

/**
 * @brief 初始化定时器
 */
bool hal_timer_init(timer_channel_t channel, timer_config_t *config) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)channel;
    (void)config;
    // 简化实现：直接返回成功
    return true;
}

/**
 * @brief 启动定时器
 */
void hal_timer_start(timer_channel_t timer) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)timer;
    // 简化实现：空操作
}

/**
 * @brief 停止定时器
 */
void hal_timer_stop(timer_channel_t timer) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)timer;
    // 简化实现：空操作
}

/**
 * @brief 获取定时器计数值
 */
uint32_t hal_timer_get_counter(timer_channel_t channel) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)channel;
    // 简化实现：返回0
    return 0;
}

/**
 * @brief 初始化PWM输出
 * @param timer 定时器通道
 * @param config PWM配置结构体指针
 * @return 成功返回true，失败返回false
 */
bool hal_timer_init_pwm(timer_channel_t timer, timer_pwm_config_t *config) {
    // 参数验证
    if (timer >= TIMER_MAX || config == NULL || timer_instances[timer] == NULL) {
        return false;
    }
    
    // 保存PWM通道配置信息
    pwm_info[timer].channel = config->channel;
    pwm_info[timer].initialized = true;
    
    // 模拟STM32 PWM初始化
    stm32_timer_t *tim = timer_instances[timer];
    
    // 使能定时器时钟 (在真实STM32代码中，这里需要调用RCC相关函数)
    // RCC->APB1ENR |= RCC_APB1ENR_TIMxEN;
    
    // 设置PWM模式和通道配置
    switch (config->channel) {
        case TIMER_CHANNEL_1:
            // 设置PWM模式1
            tim->CCMR1 &= ~(TIM_CCMR1_OC1M_Msk);
            tim->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;  // PWM模式1
            
            // 设置PWM极性
            if (config->polarity == TIMER_PWM_POLARITY_HIGH) {
                tim->CCER &= ~(TIM_CCER_CC1P_Msk);  // 高电平有效
            } else {
                tim->CCER |= TIM_CCER_CC1P;  // 低电平有效
            }
            
            // 使能输出通道
            if (config->output_enable) {
                tim->CCER |= TIM_CCER_CC1E;  // 使能通道1
            }
            break;
            
        case TIMER_CHANNEL_2:
            // 设置PWM模式1
            tim->CCMR1 &= ~(0x7U << 12);  // 清除OC2M位
            tim->CCMR1 |= (1U << 13) | (1U << 14);  // PWM模式1
            
            // 设置PWM极性
            if (config->polarity == TIMER_PWM_POLARITY_HIGH) {
                tim->CCER &= ~(TIM_CCER_CC2E_Msk << 1);  // 高电平有效
            } else {
                tim->CCER |= (TIM_CCER_CC2E_Msk << 1);  // 低电平有效
            }
            
            // 使能输出通道
            if (config->output_enable) {
                tim->CCER |= TIM_CCER_CC2E;  // 使能通道2
            }
            break;
            
        case TIMER_CHANNEL_3:
            // 设置PWM模式1
            tim->CCMR2 &= ~(0x7U << 4);  // 清除OC3M位
            tim->CCMR2 |= (1U << 5) | (1U << 6);  // PWM模式1
            
            // 设置PWM极性
            if (config->polarity == TIMER_PWM_POLARITY_HIGH) {
                tim->CCER &= ~(TIM_CCER_CC3E_Msk << 1);  // 高电平有效
            } else {
                tim->CCER |= (TIM_CCER_CC3E_Msk << 1);  // 低电平有效
            }
            
            // 使能输出通道
            if (config->output_enable) {
                tim->CCER |= TIM_CCER_CC3E;  // 使能通道3
            }
            break;
            
        case TIMER_CHANNEL_4:
            // 设置PWM模式1
            tim->CCMR2 &= ~(0x7U << 12);  // 清除OC4M位
            tim->CCMR2 |= (1U << 13) | (1U << 14);  // PWM模式1
            
            // 设置PWM极性
            if (config->polarity == TIMER_PWM_POLARITY_HIGH) {
                tim->CCER &= ~(TIM_CCER_CC4E_Msk << 1);  // 高电平有效
            } else {
                tim->CCER |= (TIM_CCER_CC4E_Msk << 1);  // 低电平有效
            }
            
            // 使能输出通道
            if (config->output_enable) {
                tim->CCER |= TIM_CCER_CC4E;  // 使能通道4
            }
            break;
            
        default:
            break;
    }
    
    // 设置初始脉冲值
    uint32_t pulse_value = (uint32_t)((float)tim->ARR * ((float)config->pulse / 100.0f));
    switch (config->channel) {
        case TIMER_CHANNEL_1:
            tim->CCR1 = pulse_value;
            break;
        case TIMER_CHANNEL_2:
            tim->CCR2 = pulse_value;
            break;
        case TIMER_CHANNEL_3:
            tim->CCR3 = pulse_value;
            break;
        case TIMER_CHANNEL_4:
            tim->CCR4 = pulse_value;
            break;
        default:
            break;
    }
    
    // 使能自动重载预加载
    tim->CR1 |= TIM_CR1_ARPE;
    
    // 使能定时器
    tim->CR1 |= TIM_CR1_CEN;
    
    return true;
}

/**
 * @brief 设置PWM占空比
 * @param timer 定时器通道
 * @param duty_cycle 占空比 (0.0f ~ 1.0f)
 * @return 成功返回true，失败返回false
 */
bool hal_timer_set_pwm_duty_cycle(timer_channel_t timer, float duty_cycle) {
    // 参数验证
    if (timer >= TIMER_MAX || timer_instances[timer] == NULL || !pwm_info[timer].initialized) {
        // 无效的定时器通道或未初始化
        return false;
    }
    
    // 验证占空比范围 (0.0f - 1.0f)
    if (duty_cycle < 0.0f) {
        duty_cycle = 0.0f;
    } else if (duty_cycle > 1.0f) {
        duty_cycle = 1.0f;
    }
    
    // 获取定时器实例
    stm32_timer_t *tim = timer_instances[timer];
    
    // 1. 获取定时器的自动重载值 (ARR)
    // 在STM32中，ARR寄存器存储了定时器的自动重载值
    uint32_t arr_value = tim->ARR;
    
    // 如果ARR为0，设置一个默认值以避免除零错误
    if (arr_value == 0) {
        arr_value = 1000;  // 默认ARR值，实际应用中应该已经配置好了
        tim->ARR = arr_value;
    }
    
    // 2. 根据占空比计算比较值 (CCR) = ARR * duty_cycle
    // 计算CCR值，这将决定PWM的占空比
    uint32_t pulse_value = (uint32_t)((float)arr_value * duty_cycle);
    
    // 3. 设置定时器比较寄存器的值
    // 根据配置的PWM通道，设置相应的CCR寄存器
    timer_output_channel_t channel = pwm_info[timer].channel;
    
    switch (channel) {
        case TIMER_CHANNEL_1:
            tim->CCR1 = pulse_value;
            break;
        case TIMER_CHANNEL_2:
            tim->CCR2 = pulse_value;
            break;
        case TIMER_CHANNEL_3:
            tim->CCR3 = pulse_value;
            break;
        case TIMER_CHANNEL_4:
            tim->CCR4 = pulse_value;
            break;
        default:
            // 无效的通道
            return false;
    }
    
    // 4. 确保PWM输出已启用
    // 根据通道设置CCER寄存器中的使能位
    switch (channel) {
        case TIMER_CHANNEL_1:
            tim->CCER |= TIM_CCER_CC1E;
            break;
        case TIMER_CHANNEL_2:
            tim->CCER |= TIM_CCER_CC2E;
            break;
        case TIMER_CHANNEL_3:
            tim->CCER |= TIM_CCER_CC3E;
            break;
        case TIMER_CHANNEL_4:
            tim->CCER |= TIM_CCER_CC4E;
            break;
        default:
            break;
    }
    
    // 更新最后设置的占空比记录
    pwm_info[timer].last_duty_cycle = duty_cycle;
    
    // 添加调试输出，记录设置的PWM参数
    printf("STM32风格 PWM设置: 定时器 %d, 通道 %d, 占空比 %.2f%%, ARR=0x%lX, CCR=0x%lX\n", 
           timer, channel, duty_cycle * 100.0f, arr_value, pulse_value);
    
    return true;
}

/**
 * @brief 模拟定时器节拍（用于测试）
 * @param timer 定时器通道
 */
void hal_timer_simulate_tick(timer_channel_t timer) {
    // 使用(void)标记未使用的参数，避免编译警告
    (void)timer;
    // 简化实现：空函数
}

/**
 * @brief 初始化PWM（独立PWM模块风格）
 * @param timer PWM定时器通道
 * @param channel PWM通道
 * @param frequency PWM频率
 * @return 成功返回true，失败返回false
 */
bool pwm_init(timer_channel_t timer, timer_output_channel_t channel, uint32_t frequency) {
    // 参数验证
    if (timer >= TIMER_MAX || channel >= TIMER_CHANNEL_MAX) {
        return false;
    }
    
    if (timer_instances[timer] == NULL) {
        return false;
    }
    
    // 配置PWM参数
    timer_pwm_config_t pwm_config = {
        .channel = channel,
        .mode = TIMER_PWM_MODE_1,
        .polarity = TIMER_PWM_POLARITY_HIGH,
        .pulse = frequency / 2,  // 初始占空比50%
        .output_enable = true
    };
    
    // 初始化PWM
    bool result = hal_timer_init_pwm(timer, &pwm_config);
    
    if (result) {
        printf("PWM初始化成功: 定时器 %d, 通道 %d, 频率 %uHz\n", timer, channel, frequency);
    } else {
        printf("PWM初始化失败: 定时器 %d, 通道 %d\n", timer, channel);
    }
    
    return result;
}

/**
 * @brief 启动PWM输出
 * @param timer PWM定时器通道
 * @param channel PWM通道
 * @return 成功返回true，失败返回false
 */
bool pwm_start(timer_channel_t timer, timer_output_channel_t channel) {
    // 参数验证
    if (timer >= TIMER_MAX || channel >= TIMER_CHANNEL_MAX) {
        return false;
    }
    
    if (timer_instances[timer] == NULL) {
        return false;
    }
    
    // 获取定时器实例
    stm32_timer_t *tim = timer_instances[timer];
    
    // 启动定时器（如果还未启动）
    tim->CR1 |= TIM_CR1_CEN;
    
    // 启用指定通道
    switch (channel) {
        case TIMER_CHANNEL_1:
            tim->CCER |= TIM_CCER_CC1E;
            break;
        case TIMER_CHANNEL_2:
            tim->CCER |= TIM_CCER_CC2E;
            break;
        case TIMER_CHANNEL_3:
            tim->CCER |= TIM_CCER_CC3E;
            break;
        case TIMER_CHANNEL_4:
            tim->CCER |= TIM_CCER_CC4E;
            break;
        default:
            return false;
    }
    
    printf("PWM启动成功: 定时器 %d, 通道 %d\n", timer, channel);
    return true;
}

/**
 * @brief 停止PWM输出
 * @param timer PWM定时器通道
 * @param channel PWM通道
 * @return 成功返回true，失败返回false
 */
bool pwm_stop(timer_channel_t timer, timer_output_channel_t channel) {
    // 参数验证
    if (timer >= TIMER_MAX || channel >= TIMER_CHANNEL_MAX) {
        return false;
    }
    
    if (timer_instances[timer] == NULL) {
        return false;
    }
    
    // 获取定时器实例
    stm32_timer_t *tim = timer_instances[timer];
    
    // 禁用指定通道
    switch (channel) {
        case TIMER_CHANNEL_1:
            tim->CCER &= ~TIM_CCER_CC1E;
            break;
        case TIMER_CHANNEL_2:
            tim->CCER &= ~TIM_CCER_CC2E;
            break;
        case TIMER_CHANNEL_3:
            tim->CCER &= ~TIM_CCER_CC3E;
            break;
        case TIMER_CHANNEL_4:
            tim->CCER &= ~TIM_CCER_CC4E;
            break;
        default:
            return false;
    }
    
    printf("PWM停止成功: 定时器 %d, 通道 %d\n", timer, channel);
    return true;
}

/**
 * @brief 设置PWM占空比（使用STM32库函数）
 * @param timer PWM定时器通道
 * @param channel PWM通道
 * @param duty_cycle 占空比 (0.0f - 1.0f)
 * @return 成功返回true，失败返回false
 */
bool pwm_set_duty_cycle(timer_channel_t timer, timer_output_channel_t channel, float duty_cycle) {
    // 参数验证
    if (timer >= TIMER_MAX || channel >= TIMER_CHANNEL_MAX) {
        return false;
    }
    
    if (timer_instances[timer] == NULL || !pwm_info[timer].initialized) {
        return false;
    }
    
    // 验证占空比范围
    if (duty_cycle < 0.0f) {
        duty_cycle = 0.0f;
    } else if (duty_cycle > 1.0f) {
        duty_cycle = 1.0f;
    }
    
    // 使用原有的hal_timer_set_pwm_duty_cycle函数
    bool result = hal_timer_set_pwm_duty_cycle(timer, duty_cycle);
    
    if (result) {
        printf("PWM占空比设置成功: 定时器 %d, 通道 %d, 占空比 %.2f%%\n", 
               timer, channel, duty_cycle * 100.0f);
    }
    
    return result;
}

/**
 * @brief 设置PWM脉冲宽度
 * @param timer PWM定时器通道
 * @param channel PWM通道
 * @param pulse 脉冲宽度值
 * @return 成功返回true，失败返回false
 */
bool pwm_set_pulse(timer_channel_t timer, timer_output_channel_t channel, uint32_t pulse) {
    // 参数验证
    if (timer >= TIMER_MAX || channel >= TIMER_CHANNEL_MAX) {
        return false;
    }
    
    if (timer_instances[timer] == NULL) {
        return false;
    }
    
    // 获取定时器实例
    stm32_timer_t *tim = timer_instances[timer];
    
    // 获取自动重载值
    uint32_t arr_value = tim->ARR;
    if (arr_value == 0) {
        arr_value = 1000; // 默认值
        tim->ARR = arr_value;
    }
    
    // 验证脉冲值范围
    if (pulse > arr_value) {
        pulse = arr_value;
    }
    
    // 设置对应的CCR寄存器
    switch (channel) {
        case TIMER_CHANNEL_1:
            tim->CCR1 = pulse;
            break;
        case TIMER_CHANNEL_2:
            tim->CCR2 = pulse;
            break;
        case TIMER_CHANNEL_3:
            tim->CCR3 = pulse;
            break;
        case TIMER_CHANNEL_4:
            tim->CCR4 = pulse;
            break;
        default:
            return false;
    }
    
    printf("PWM脉冲宽度设置成功: 定时器 %d, 通道 %d, 脉冲 %u\n", timer, channel, pulse);
    return true;
}

/**
 * @brief 生成PWM脉冲
 * @param timer PWM定时器通道
 * @param channel PWM通道
 * @param pulse 脉冲宽度
 * @param delay_us 脉冲延迟(微秒)
 * @return 成功返回true，失败返回false
 */
bool pwm_generate_pulse(timer_channel_t timer, timer_output_channel_t channel, uint32_t pulse, uint32_t delay_us) {
    // 参数验证
    if (timer >= TIMER_MAX || channel >= TIMER_CHANNEL_MAX) {
        return false;
    }
    
    if (timer_instances[timer] == NULL) {
        return false;
    }
    
    // 首先停止PWM输出
    if (!pwm_stop(timer, channel)) {
        return false;
    }
    
    // 设置脉冲宽度
    if (!pwm_set_pulse(timer, channel, pulse)) {
        return false;
    }
    
    // 延迟（这里简化为直接设置，实际上应该使用精确延时）
    for (volatile uint32_t i = 0; i < delay_us * 1000; i++) {
        // 简单延时循环
    }
    
    // 启动PWM输出
    if (!pwm_start(timer, channel)) {
        return false;
    }
    
    // 再延迟一段时间
    for (volatile uint32_t i = 0; i < delay_us * 1000; i++) {
        // 简单延时循环
    }
    
    // 停止PWM输出
    pwm_stop(timer, channel);
    
    printf("PWM脉冲生成完成: 定时器 %d, 通道 %d, 脉冲 %u, 延迟 %uus\n", 
           timer, channel, pulse, delay_us);
    
    return true;
}