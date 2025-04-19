#ifndef __DS18B20_H_
#define __DS18B20_H_

#include <stdint.h>

#ifndef DS18B20_H__
#define DS18B20_H__

#include <stdint.h>

uint8_t DS18B20_Init(void);							// 初始化DS18B20单总线
int16_t DS18B20_Get_Temp(void);						// 获取温度
void DS18B20_Start(void);								// 温度转换
void DS18B20_Write_Byte(uint8_t dat);		// 写入一个Byte
uint8_t DS18B20_Read_Byte(void);				// 读出一个Byte
uint8_t DS18B20_Read_Bit(void);					// 读出一个Bit
uint8_t DS18B20_Check(void);						// 测试用
void DS18B20_Rst(void);									// 复位DS18B20

#endif

#endif
