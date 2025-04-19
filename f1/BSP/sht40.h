#ifndef __SHT40_H_
#define __SHT40_H_

#include <stdint.h>

#define SHT40_ADDRESS 0x44 													// SHT40的IIC地址
#define SHT40_COMMAND_MEASURE_HIGH_PRECISION 0xFD  	// 测量命令 0XFD



uint8_t SHT40_Start_Measurement(void); 												// IIC发送测量指令
uint8_t SHT40_Read_Measurement(uint8_t* data, uint8_t length);	// IIC接收测量结果

void IIC_Send_Byte(uint8_t byte);

#endif
