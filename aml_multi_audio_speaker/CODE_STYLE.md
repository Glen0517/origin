# 代码风格指南

## 1. 命名规范

### 1.1 文件命名
- 源文件：使用小写字母和下划线，如 `memory_manager.c`
- 头文件：使用小写字母和下划线，如 `memory_manager.h`
- 测试文件：使用 `test_` 前缀，如 `test_memory_manager.c`

### 1.2 函数命名
- 使用小写字母和下划线，如 `memory_manager_init()`
- 函数名应清晰表达函数的功能
- 对于私有函数，可使用下划线前缀，如 `_memory_pool_alloc()`

### 1.3 变量命名
- 使用小写字母和下划线，如 `block_size`
- 全局变量：使用 `g_` 前缀，如 `g_sys_running`
- 常量：使用全大写字母和下划线，如 `MAX_BLOCK_SIZE`
- 枚举值：使用全大写字母和下划线，如 `MEMORY_POOL_FULL`

### 1.4 结构体和类型命名
- 结构体：使用驼峰命名法，如 `MemoryPoolConfig`
- 类型定义：使用驼峰命名法并添加 `_t` 后缀，如 `MemoryPoolConfig_t`
- 枚举类型：使用驼峰命名法并添加 `_e` 后缀，如 `MemoryBlockState_e`

## 2. 代码格式

### 2.1 缩进
- 使用 4 个空格进行缩进，不使用制表符
- 大括号 `{` 放在行尾，与语句在同一行
- 大括号 `}` 单独占一行，与对应的语句对齐

### 2.2 空格
- 运算符两侧使用空格，如 `a = b + c`
- 逗号后使用空格，如 `function(a, b, c)`
- 分号后使用空格（如果在同一行），如 `for (i = 0; i < 10; i++)`
- 函数参数列表中，逗号后使用空格

### 2.3 空行
- 函数之间使用两个空行
- 函数内部逻辑块之间使用一个空行
- 头文件和源文件的注释块后使用一个空行

### 2.4 行长度
- 代码行长度应控制在 80-100 字符以内
- 超过长度的行应适当换行，保持可读性

## 3. 注释规范

### 3.1 文件头部注释
- 每个文件都应有头部注释，包括文件描述、作者、日期等信息
- 使用 Doxygen 风格的注释格式

```c
/**
 * @file memory_manager.c
 * @brief 内存管理模块实现
 * @details 提供内存池管理、内存分配和释放、内存泄漏检测等功能
 * @author AML Audio Team
 * @date 2026-01-28
 */
```

### 3.2 函数注释
- 每个函数都应有注释，说明函数的功能、参数、返回值等
- 使用 Doxygen 风格的注释格式

```c
/**
 * @brief 初始化内存池
 * @details 根据配置参数创建和初始化内存池
 * @param config 内存池配置参数
 * @return 内存池指针，失败返回NULL
 */
MemoryPool_t *memory_pool_init(MemoryPoolConfig_t *config);
```

### 3.3 变量和结构体注释
- 重要的变量和结构体应有注释，说明其用途
- 结构体的每个字段都应有注释

```c
/**
 * @brief 内存池配置结构体
 * @details 定义内存池的配置参数
 */
typedef struct {
    size_t block_size;      ///< 内存块大小
    size_t block_count;     ///< 内存块数量
    size_t alignment;       ///< 内存对齐要求
    const char *name;       ///< 内存池名称
} MemoryPoolConfig_t;
```

### 3.4 代码注释
- 复杂的代码逻辑应有注释说明
- 关键算法和决策应有注释
- 避免不必要的注释，代码本身应清晰易懂

## 4. 代码结构

### 4.1 头文件保护
- 使用 `#ifndef`、`#define`、`#endif` 防止头文件重复包含
- 宏定义使用大写字母和下划线

```c
#ifndef __MEMORY_MANAGER_H__
#define __MEMORY_MANAGER_H__

// 头文件内容

#endif // __MEMORY_MANAGER_H__
```

### 4.2 包含顺序
- 系统头文件（如 `<stdio.h>`）
- 第三方库头文件
- 项目内部头文件
- 使用相对路径包含项目内部头文件

### 4.3 函数实现顺序
- 公共函数在前，私有函数在后
- 相关函数应放在一起
- 初始化函数在前，反初始化函数在后

## 5. 错误处理

### 5.1 错误返回值
- 使用 `bool` 类型表示操作是否成功
- 使用 `int` 类型表示具体错误码
- 错误码应定义为枚举或宏

### 5.2 错误检查
- 函数参数应进行有效性检查
- 内存分配应检查是否成功
- 错误处理应及时、明确

## 6. 性能考虑

### 6.1 内存使用
- 避免频繁的内存分配和释放
- 使用内存池管理频繁使用的小内存
- 及时释放不再使用的内存

### 6.2 代码优化
- 避免不必要的计算和重复操作
- 使用适当的数据结构和算法
- 考虑缓存友好的数据布局

## 7. 安全性

### 7.1 输入验证
- 所有外部输入应进行验证
- 避免缓冲区溢出
- 检查指针是否为空

### 7.2 安全编码
- 避免使用不安全的函数，如 `strcpy()`
- 使用安全的替代函数，如 `strncpy()`
- 注意内存安全和线程安全

## 8. 测试

### 8.1 单元测试
- 为关键模块编写单元测试
- 测试文件放在 `tests/` 目录下
- 测试应覆盖正常情况和边界情况

### 8.2 测试命名
- 测试函数使用 `test_` 前缀
- 测试文件使用 `test_` 前缀

## 9. 版本控制

### 9.1 提交消息
- 提交消息应清晰、简洁
- 使用英文编写提交消息
- 提交消息格式：`[模块名] 功能描述`

### 9.2 代码审查
- 代码提交前应进行自我审查
- 重要修改应进行团队审查
- 审查应关注代码质量、安全性和性能

## 10. 工具和自动化

### 10.1 代码格式化
- 使用统一的代码格式化工具
- 遵循本指南的代码风格

### 10.2 静态分析
- 定期运行静态分析工具检查代码
- 修复发现的问题

### 10.3 持续集成
- 使用持续集成工具自动化测试和构建
- 确保代码质量和稳定性

## 11. 示例

### 11.1 函数实现示例

```c
/**
 * @brief 初始化内存管理器
 * @details 创建并初始化内存管理器，用于管理内存池
 * @param config 内存管理器配置参数
 * @return 内存管理器指针，失败返回NULL
 */
MemoryManager_t *memory_manager_init(MemoryManagerConfig_t *config) {
    // 检查参数
    if (!config) {
        // 使用默认配置
        config = &g_default_memory_config;
    }
    
    // 分配内存
    MemoryManager_t *manager = (MemoryManager_t *)malloc(sizeof(MemoryManager_t));
    if (!manager) {
        LOG_ERROR("Failed to allocate memory manager");
        return NULL;
    }
    
    // 初始化成员
    memset(manager, 0, sizeof(MemoryManager_t));
    manager->config = *config;
    manager->total_allocated = 0;
    manager->peak_allocated = 0;
    manager->allocation_count = 0;
    manager->free_count = 0;
    
    // 初始化内存池
    manager->memory_pool = memory_pool_init(&config->pool_config);
    if (!manager->memory_pool) {
        LOG_ERROR("Failed to initialize memory pool");
        free(manager);
        return NULL;
    }
    
    LOG_INFO("Memory manager initialized");
    return manager;
}
```

### 11.2 结构体定义示例

```c
/**
 * @brief 内存管理器配置结构体
 * @details 定义内存管理器的配置参数
 */
typedef struct {
    MemoryPoolConfig_t pool_config;    ///< 内存池配置
    bool enable_monitoring;            ///< 是否启用内存监控
    size_t max_memory_usage;           ///< 最大内存使用限制
} MemoryManagerConfig_t;

/**
 * @brief 内存管理器结构体
 * @details 管理内存池和内存分配
 */
typedef struct {
    MemoryManagerConfig_t config;      ///< 配置参数
    MemoryPool_t *memory_pool;         ///< 内存池
    size_t total_allocated;            ///< 当前分配的内存总量
    size_t peak_allocated;             ///< 峰值内存使用量
    size_t allocation_count;           ///< 内存分配次数
    size_t free_count;                 ///< 内存释放次数
    bool monitoring_enabled;           ///< 监控是否启用
} MemoryManager_t;
```

## 12. 总结

本代码风格指南旨在确保项目代码的一致性、可读性和可维护性。所有团队成员应遵循本指南，共同提高代码质量。

代码风格应服务于代码的可读性和可维护性，在特殊情况下可适当调整，但应保持整体一致性。
