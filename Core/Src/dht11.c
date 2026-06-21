#include "dht11.h"

#define DHT11_PIN GPIO_PIN_1
#define DHT11_PORT GPIOB
//测试1
uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi) {
    uint8_t i, j, temp_data[5] = {0};
    
    // 1. 主机发送开始信号：拉低至少18ms
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(20); 
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    
    // 2. 等待DHT11响应 (延时20-40us)
    for(i=0; i<10; i++); 
    
    if(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET) {
        while(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET); // 等待低电平结束
        while(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET);    // 等待高电平结束
        
        // 3. 读取40位数据
        for(i=0; i<40; i++) {
            while(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET);
            uint32_t time_count = 0;
            while(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET) {
                time_count++;
                if(time_count > 100) break; // 超时保护
            }
            if(time_count > 40) { // 大于40us 代表数据位为1
                temp_data[i/8] |= (1 << (7 - (i%8)));
            }
        }
        
        // 4. 校验数据
        if(temp_data[0] + temp_data[1] + temp_data[2] + temp_data[3] == temp_data[4]) {
            *humi = temp_data[0];
            *temp = temp_data[2];
            return 0; // 成功
        }
    }
    return 1; // 失败
}