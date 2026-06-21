#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f1xx_hal.h" // 或者 #include "main.h"，根据需要包含HAL库

// ==========================================
// 1. 引脚定义宏 (方便移植和修改硬件连接)
// ==========================================
#define DHT11_PORT    GPIOB
#define DHT11_PIN     GPIO_PIN_1

// ==========================================
// 2. 函数声明 (提供外部调用的接口)
// ==========================================

/**
 * @brief  读取 DHT11 的温度和湿度数据
 * @param  *temp: 存放温度数据的指针
 * @param  *humi: 存放湿度数据的指针
 * @retval 0: 读取成功, 1: 读取失败 (超时或校验错误)
 */
uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi);

#endif /* __DHT11_H */