/**
 * @file    bsp_gpio_drv.h
 * @brief   BSP GPIO 抽象层 + 中断回调注册
 * @version V2.0
 * @date    2026-08-24
 * 
 * @note    修复记录：
 *          1. 中断回调注册表支持多端口同引脚（port + pin 联合索引）
 *          2. 输入模式禁用写操作函数
 *          3. last_active 初始化使用当前 DWT 计数值
 *          4. 提供 fast 内联版本支持高频场景
 *          5. 支持边沿选择（上升沿/下降沿/双沿）
 */

#ifndef __BSP_GPIO_DRV_H
#define __BSP_GPIO_DRV_H

#include <stdint.h>
#include <stdbool.h>
#include "hal_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== GPIO 设备结构 ========== */

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    bool          active_high;
} gpio_pin_t;

typedef struct {
    void (*init)(gpio_pin_t *pin);
    void (*write)(gpio_pin_t *pin, bool state);
    void (*toggle)(gpio_pin_t *pin);
    bool (*read)(gpio_pin_t *pin);
    void (*set_high)(gpio_pin_t *pin);
    void (*set_low)(gpio_pin_t *pin);
} gpio_ops_t;

typedef struct {
    gpio_pin_t  pin;
    gpio_ops_t  ops;
    bool        is_initialized;
} gpio_dev_t;

/* ========== 中断回调类型 ========== */

/**
 * @brief   GPIO 中断回调函数类型
 * @param   pin         触发中断的引脚号
 * @param   user_data   用户数据
 */
typedef void (*gpio_irq_callback_t)(uint16_t pin, void *user_data);

/**
 * @brief   边沿触发类型
 */
#define BSP_GPIO_EDGE_RISING    (1 << 0)   /* 上升沿触发 */
#define BSP_GPIO_EDGE_FALLING   (1 << 1)   /* 下降沿触发 */
#define BSP_GPIO_EDGE_BOTH      (BSP_GPIO_EDGE_RISING | BSP_GPIO_EDGE_FALLING) /* 双沿触发 */
#define BSP_GPIO_EDGE_ANY       BSP_GPIO_EDGE_BOTH  /* 兼容旧命名 */

/**
 * @brief   电平门忽略哨兵：注册时传此值表示不检查有效电平（每个边沿都触发）
 * @note    GPIO_PIN_RESET=0 / GPIO_PIN_SET=1，故用 0xFF 作哨兵
 */
#define BSP_GPIO_IRQ_LEVEL_ANY    0xFF

/**
 * @brief   中断回调注册项
 */
typedef struct {
    GPIO_TypeDef           *port;          /* 引脚所在端口 */
    uint16_t                pin;           /* 引脚号 */
    gpio_irq_callback_t     callback;      /* 回调函数 */
    void                   *user_data;     /* 用户数据 */
    bool                    enabled;       /* 是否启用 */
    uint8_t                 edge;          /* 触发边沿：RISING/FALLING/BOTH */
    uint32_t                debounce_us;   /* 防抖窗口(us)，0=不防抖 */
    uint8_t                 active_level;  /* 有效电平(GPIO_PIN_SET/RESET)；BSP_GPIO_IRQ_LEVEL_ANY=不检查 */
    uint32_t                last_active;   /* 上次有效触发时的 DWT 计数值（防抖基准） */
    uint8_t                 last_level;    /* ★ 上次电平，用于边沿检测 */
} gpio_irq_reg_t;

/* ========== GPIO 设备 API ========== */

bool bsp_gpio_init(gpio_dev_t *dev, GPIO_TypeDef *port, uint16_t pin, bool active_high);
bool bsp_gpio_init_default(gpio_dev_t *dev, GPIO_TypeDef *port, uint16_t pin);

/**
 * @brief   输入模式初始化
 * @note    写操作函数被禁用，防止误操作
 */
bool bsp_gpio_init_input(gpio_dev_t *dev, GPIO_TypeDef *port, uint16_t pin, bool active_high);

void bsp_gpio_register_ops(gpio_dev_t *dev, gpio_ops_t *ops);

/* 标准操作（带函数指针，适合低频场景） */
void bsp_gpio_set_high(gpio_dev_t *dev);
void bsp_gpio_set_low(gpio_dev_t *dev);
void bsp_gpio_toggle(gpio_dev_t *dev);
void bsp_gpio_write(gpio_dev_t *dev, bool state);
bool bsp_gpio_read(gpio_dev_t *dev);

/* 快速操作（内联，适合高频场景，无函数指针开销） */
static inline void bsp_gpio_set_high_fast(gpio_dev_t *dev);
static inline void bsp_gpio_set_low_fast(gpio_dev_t *dev);
static inline void bsp_gpio_toggle_fast(gpio_dev_t *dev);
static inline void bsp_gpio_write_fast(gpio_dev_t *dev, bool state);
static inline bool bsp_gpio_read_fast(gpio_dev_t *dev);

bool bsp_gpio_is_initialized(gpio_dev_t *dev);

/* ========== 中断回调注册 API ========== */

/**
 * @brief   注册 GPIO 中断回调
 * @param   port          引脚所在端口
 * @param   pin           引脚号
 * @param   callback      回调函数
 * @param   user_data     用户数据
 * @param   edge          触发边沿：BSP_GPIO_EDGE_RISING / FALLING / BOTH
 * @param   debounce_us   防抖窗口(us)，0=不防抖
 * @param   active_level  有效电平(GPIO_PIN_SET/RESET)；BSP_GPIO_IRQ_LEVEL_ANY=不检查
 * @retval  true: 成功  false: 失败
 */
bool bsp_gpio_irq_register(GPIO_TypeDef *port, uint16_t pin,
                           gpio_irq_callback_t callback, void *user_data,
                           uint8_t edge, uint32_t debounce_us, uint8_t active_level);

/**
 * @brief   注销 GPIO 中断回调
 * @param   port  引脚所在端口
 * @param   pin   GPIO 引脚号
 */
void bsp_gpio_irq_unregister(GPIO_TypeDef *port, uint16_t pin);

/**
 * @brief   启用/禁用 GPIO 中断回调
 * @param   port    引脚所在端口
 * @param   pin     GPIO 引脚号
 * @param   enable  true: 启用, false: 禁用
 */
void bsp_gpio_irq_enable(GPIO_TypeDef *port, uint16_t pin, bool enable);

/**
 * @brief   获取回调注册信息（调试用）
 */
bool bsp_gpio_irq_get_info(GPIO_TypeDef *port, uint16_t pin, gpio_irq_reg_t *out_info);

/* 中断回调分发 (供 HAL 层调用) */
void bsp_gpio_irq_dispatch(uint16_t GPIO_Pin);

/* ========== 内联函数实现 ========== */

static inline void bsp_gpio_set_high_fast(gpio_dev_t *dev) {
    if (dev == NULL || !dev->is_initialized || dev->pin.port == NULL) return;
    HAL_GPIO_WritePin(dev->pin.port, dev->pin.pin,
                      dev->pin.active_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static inline void bsp_gpio_set_low_fast(gpio_dev_t *dev) {
    if (dev == NULL || !dev->is_initialized || dev->pin.port == NULL) return;
    HAL_GPIO_WritePin(dev->pin.port, dev->pin.pin,
                      dev->pin.active_high ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static inline void bsp_gpio_toggle_fast(gpio_dev_t *dev) {
    if (dev == NULL || !dev->is_initialized || dev->pin.port == NULL) return;
    HAL_GPIO_TogglePin(dev->pin.port, dev->pin.pin);
}

static inline void bsp_gpio_write_fast(gpio_dev_t *dev, bool state) {
    if (dev == NULL || !dev->is_initialized || dev->pin.port == NULL) return;
    GPIO_PinState hal_state = dev->pin.active_high ?
        (state ? GPIO_PIN_SET : GPIO_PIN_RESET) :
        (state ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(dev->pin.port, dev->pin.pin, hal_state);
}

static inline bool bsp_gpio_read_fast(gpio_dev_t *dev) {
    if (dev == NULL || !dev->is_initialized || dev->pin.port == NULL) return false;
    GPIO_PinState state = HAL_GPIO_ReadPin(dev->pin.port, dev->pin.pin);
    return dev->pin.active_high ? (state == GPIO_PIN_SET) : (state == GPIO_PIN_RESET);
}

#ifdef __cplusplus
}
#endif

#endif /* __BSP_GPIO_DRV_H */