/**
 * @file    bsp_gpio_drv.c
 * @brief   BSP GPIO 抽象层 + 中断回调注册实现
 * @version V2.0
 * @date    2026-08-24
 */

#include "bsp_gpio_drv.h"
#include "bsp_dwt.h"
#include "SEGGER_RTT_Log.h"
#include <string.h>

/* ========== 默认操作函数 ========== */

static void default_gpio_init(gpio_pin_t *pin) {
    if (pin == NULL || pin->port == NULL) return;
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = pin->pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(pin->port, &GPIO_InitStruct);
}

static void default_gpio_write(gpio_pin_t *pin, bool state) {
    if (pin == NULL || pin->port == NULL) return;
    
    GPIO_PinState hal_state;
    if (pin->active_high) {
        hal_state = state ? GPIO_PIN_SET : GPIO_PIN_RESET;
    } else {
        hal_state = state ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }
    HAL_GPIO_WritePin(pin->port, pin->pin, hal_state);
}

static void default_gpio_toggle(gpio_pin_t *pin) {
    if (pin == NULL || pin->port == NULL) return;
    HAL_GPIO_TogglePin(pin->port, pin->pin);
}

static bool default_gpio_read(gpio_pin_t *pin) {
    if (pin == NULL || pin->port == NULL) return false;
    
    GPIO_PinState state = HAL_GPIO_ReadPin(pin->port, pin->pin);
    if (pin->active_high) {
        return (state == GPIO_PIN_SET);
    } else {
        return (state == GPIO_PIN_RESET);
    }
}

static void default_gpio_set_high(gpio_pin_t *pin) {
    default_gpio_write(pin, true);
}

static void default_gpio_set_low(gpio_pin_t *pin) {
    default_gpio_write(pin, false);
}

static gpio_ops_t g_default_ops = {
    .init = default_gpio_init,
    .write = default_gpio_write,
    .toggle = default_gpio_toggle,
    .read = default_gpio_read,
    .set_high = default_gpio_set_high,
    .set_low = default_gpio_set_low
};

static void nop_gpio_write(gpio_pin_t *pin, bool state) { (void)pin; (void)state; }
static void nop_gpio_set_high(gpio_pin_t *pin) { (void)pin; }
static void nop_gpio_set_low(gpio_pin_t *pin) { (void)pin; }

static void input_gpio_init(gpio_pin_t *pin) {
    if (pin == NULL || pin->port == NULL) return;

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = pin->pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(pin->port, &GPIO_InitStruct);
}

/* ========== 中断回调注册表 ========== */

#define MAX_IRQ_REGISTERS  16   /* ★ 增加注册表大小，支持更多引脚 */

static gpio_irq_reg_t g_irq_regs[MAX_IRQ_REGISTERS] = {0};

/* ★ 修复 P0：查找时同时匹配 port + pin */
static int bsp_gpio_irq_find_slot(GPIO_TypeDef *port, uint16_t pin) {
    for (int i = 0; i < MAX_IRQ_REGISTERS; i++) {
        if (g_irq_regs[i].enabled &&
            g_irq_regs[i].port == port &&
            g_irq_regs[i].pin == pin) {
            return i;
        }
    }
    return -1;
}

static int bsp_gpio_irq_find_free_slot(void) {
    for (int i = 0; i < MAX_IRQ_REGISTERS; i++) {
        if (!g_irq_regs[i].enabled) {
            return i;
        }
    }
    return -1;
}

/* ========== GPIO 设备 API ========== */

bool bsp_gpio_init(gpio_dev_t *dev, GPIO_TypeDef *port, uint16_t pin, bool active_high) {
    if (dev == NULL || port == NULL) return false;
    
    dev->pin.port = port;
    dev->pin.pin = pin;
    dev->pin.active_high = active_high;
    
    if (dev->ops.init == NULL) {
        dev->ops = g_default_ops;
    }
    
    dev->ops.init(&dev->pin);
    dev->is_initialized = true;
    dev->ops.set_low(&dev->pin);
    
    DBG_LOG("GPIO init: port=%p, pin=0x%04X, active_high=%d", port, pin, active_high);
    return true;
}

bool bsp_gpio_init_default(gpio_dev_t *dev, GPIO_TypeDef *port, uint16_t pin) {
    return bsp_gpio_init(dev, port, pin, true);
}

/* ★ 修复 P2：输入模式禁用写操作 */
bool bsp_gpio_init_input(gpio_dev_t *dev, GPIO_TypeDef *port, uint16_t pin, bool active_high) {
    if (dev == NULL || port == NULL) return false;

    dev->pin.port = port;
    dev->pin.pin = pin;
    dev->pin.active_high = active_high;

    if (dev->ops.init == NULL) {
        dev->ops = g_default_ops;
    }
    dev->ops.init = input_gpio_init;
    dev->ops.write = nop_gpio_write;        /* ★ 禁用写操作 */
    dev->ops.set_high = nop_gpio_set_high;
    dev->ops.set_low = nop_gpio_set_low;
    dev->ops.init(&dev->pin);
    dev->is_initialized = true;

    DBG_LOG("GPIO input init: port=%p, pin=0x%04X", port, pin);
    return true;
}

void bsp_gpio_register_ops(gpio_dev_t *dev, gpio_ops_t *ops) {
    if (dev == NULL || ops == NULL) return;
    
    dev->ops.init = ops->init ? ops->init : g_default_ops.init;
    dev->ops.write = ops->write ? ops->write : g_default_ops.write;
    dev->ops.toggle = ops->toggle ? ops->toggle : g_default_ops.toggle;
    dev->ops.read = ops->read ? ops->read : g_default_ops.read;
    dev->ops.set_high = ops->set_high ? ops->set_high : g_default_ops.set_high;
    dev->ops.set_low = ops->set_low ? ops->set_low : g_default_ops.set_low;
}

/* 标准操作（带函数指针） */
void bsp_gpio_set_high(gpio_dev_t *dev) {
    if (dev == NULL || !dev->is_initialized) return;
    dev->ops.set_high(&dev->pin);
}

void bsp_gpio_set_low(gpio_dev_t *dev) {
    if (dev == NULL || !dev->is_initialized) return;
    dev->ops.set_low(&dev->pin);
}

void bsp_gpio_toggle(gpio_dev_t *dev) {
    if (dev == NULL || !dev->is_initialized) return;
    dev->ops.toggle(&dev->pin);
}

void bsp_gpio_write(gpio_dev_t *dev, bool state) {
    if (dev == NULL || !dev->is_initialized) return;
    dev->ops.write(&dev->pin, state);
}

bool bsp_gpio_read(gpio_dev_t *dev) {
    if (dev == NULL || !dev->is_initialized) return false;
    return dev->ops.read(&dev->pin);
}

bool bsp_gpio_is_initialized(gpio_dev_t *dev) {
    return (dev != NULL && dev->is_initialized);
}

/* ========== 中断回调注册 API ========== */

bool bsp_gpio_irq_register(GPIO_TypeDef *port, uint16_t pin,
                           gpio_irq_callback_t callback, void *user_data,
                           uint8_t edge, uint32_t debounce_us, uint8_t active_level) {
    if (callback == NULL || port == NULL) {
        DBG_LOG("IRQ register: invalid param for pin 0x%04X", pin);
        return false;
    }

    /* ★ 修复 P0：查找时传入 port */
    int slot = bsp_gpio_irq_find_slot(port, pin);
    
    if (slot >= 0) {
        /* ★ 修复 P1：更新已有注册 */
        g_irq_regs[slot].callback = callback;
        g_irq_regs[slot].user_data = user_data;
        g_irq_regs[slot].edge = edge;
        g_irq_regs[slot].debounce_us = debounce_us;
        g_irq_regs[slot].active_level = active_level;
        g_irq_regs[slot].enabled = true;                          /* ★ 确保启用 */
        g_irq_regs[slot].last_active = bsp_GetCycleCount();       /* ★ 使用当前值 */
        g_irq_regs[slot].last_level = HAL_GPIO_ReadPin(port, pin); /* ★ 保存当前电平 */
        
        DBG_LOG("IRQ register: updated port=%p pin=0x%04X", port, pin);
        return true;
    }

    slot = bsp_gpio_irq_find_free_slot();
    if (slot < 0) {
        DBG_LOG("IRQ register: no free slot for pin 0x%04X", pin);
        return false;
    }

    g_irq_regs[slot].port = port;
    g_irq_regs[slot].pin = pin;
    g_irq_regs[slot].callback = callback;
    g_irq_regs[slot].user_data = user_data;
    g_irq_regs[slot].enabled = true;
    g_irq_regs[slot].edge = edge;
    g_irq_regs[slot].debounce_us = debounce_us;
    g_irq_regs[slot].active_level = active_level;
    g_irq_regs[slot].last_active = bsp_GetCycleCount();           /* ★ 使用当前值 */
    g_irq_regs[slot].last_level = HAL_GPIO_ReadPin(port, pin);   /* ★ 保存当前电平 */

    DBG_LOG("IRQ register: port=%p pin=0x%04X slot=%d", port, pin, slot);
    return true;
}

/* ★ 修复 P0：注销时传入 port */
void bsp_gpio_irq_unregister(GPIO_TypeDef *port, uint16_t pin) {
    int slot = bsp_gpio_irq_find_slot(port, pin);
    if (slot < 0) return;
    
    g_irq_regs[slot].enabled = false;
    g_irq_regs[slot].port = NULL;
    g_irq_regs[slot].pin = 0;
    g_irq_regs[slot].callback = NULL;
    g_irq_regs[slot].user_data = NULL;
    DBG_LOG("IRQ unregister: port=%p pin=0x%04X", port, pin);
}

/* ★ 修复 P0：启用/禁用时传入 port */
void bsp_gpio_irq_enable(GPIO_TypeDef *port, uint16_t pin, bool enable) {
    int slot = bsp_gpio_irq_find_slot(port, pin);
    if (slot < 0) return;
    g_irq_regs[slot].enabled = enable;
    DBG_LOG("IRQ %s: port=%p pin=0x%04X", enable ? "enable" : "disable", port, pin);
}

bool bsp_gpio_irq_get_info(GPIO_TypeDef *port, uint16_t pin, gpio_irq_reg_t *out_info) {
    if (out_info == NULL) return false;
    int slot = bsp_gpio_irq_find_slot(port, pin);
    if (slot < 0) return false;
    *out_info = g_irq_regs[slot];
    return true;
}

/* ========== 中断回调分发 ========== */

/**
 * @brief   分发 GPIO 中断到注册的回调
 * @param   GPIO_Pin  触发中断的引脚号（位掩码，如 0x0001）
 * @note    硬件 EXTI 应配置为 RISING_FALLING（双沿触发）
 */
void bsp_gpio_irq_dispatch(uint16_t GPIO_Pin) {
    /* ★ 由于不知道 port，需要遍历所有注册项匹配 pin */
    /* ★ 优化：按 pin 索引，如果有多个 port 同 pin，全部触发 */
    for (int i = 0; i < MAX_IRQ_REGISTERS; i++) {
        gpio_irq_reg_t *reg = &g_irq_regs[i];
        if (!reg->enabled || reg->callback == NULL) continue;
        if (reg->pin != GPIO_Pin) continue;
        if (reg->port == NULL) continue;

        /* ★ 边沿检测：读取当前电平，与上次电平比较 */
        GPIO_PinState now = HAL_GPIO_ReadPin(reg->port, reg->pin);
        GPIO_PinState last = (GPIO_PinState)reg->last_level;
        reg->last_level = (uint8_t)now;

        /* 判断本次边沿类型 */
        uint8_t detected_edge = 0;
        if (last == GPIO_PIN_RESET && now == GPIO_PIN_SET) {
            detected_edge = BSP_GPIO_EDGE_RISING;
        } else if (last == GPIO_PIN_SET && now == GPIO_PIN_RESET) {
            detected_edge = BSP_GPIO_EDGE_FALLING;
        } else {
            /* 电平未变化或无效边沿，不触发 */
            continue;
        }

        /* ★ 检查是否匹配注册的边沿类型 */
        if ((reg->edge & detected_edge) == 0) {
            continue;
        }

        /* 有效电平门：检查当前电平是否匹配（可选） */
        if (reg->active_level != BSP_GPIO_IRQ_LEVEL_ANY) {
            if (now != (GPIO_PinState)reg->active_level) {
                continue;
            }
        }

        /* 防抖窗口 */
        if (reg->debounce_us > 0) {
            uint32_t now_cycle = bsp_GetCycleCount();
            if (bsp_GetElapsedUS(reg->last_active, now_cycle) < reg->debounce_us) {
                continue;
            }
            reg->last_active = now_cycle;
        }

        /* 触发回调 */
        reg->callback(GPIO_Pin, reg->user_data);
    }
}