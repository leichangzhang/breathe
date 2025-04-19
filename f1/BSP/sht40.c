#include "sht40.h"
#include "main.h"
#include "us_delay.h"

#define SDA_HIGH() HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_SET)
#define SDA_LOW()  HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_RESET)
#define SCL_HIGH() HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET)
#define SCL_LOW()  HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_RESET)
#define READ_SDA() HAL_GPIO_ReadPin(SDA_GPIO_Port, SDA_Pin)

/*
*
* IIC总线IO操作
*
*/

// SDA数据线设为输出模式
static void SDA_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pin = SDA_Pin;
		GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(SDA_GPIO_Port, &GPIO_InitStruct);
}


// SDA数据线设为输入模式
static void SDA_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pin = SDA_Pin;
		GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(SDA_GPIO_Port, &GPIO_InitStruct);
}


/*
*
* IIC总线功能函数
*
*/

// IIC短延时
void IIC_Delay(void)
{
	delay_us(5);
}


// IIC开始
void IIC_Start(void)
{
	SDA_OUT();
	SDA_HIGH();
	SCL_HIGH();
	IIC_Delay();
	SDA_LOW();
	IIC_Delay();
	SCL_LOW();
	IIC_Delay();
}


// IIC结束
void IIC_Stop(void) 
{
	SDA_OUT();
	SCL_LOW();
	SDA_LOW();
	IIC_Delay();
	SCL_HIGH();
	IIC_Delay();
	SDA_HIGH();
	IIC_Delay();
}


// IIC发送一个字节
void IIC_Send_Byte(uint8_t byte)
{
	SDA_OUT();
	SCL_LOW();
	for (uint8_t i = 0; i < 8; i++) 
	{
		if ((byte<<i) & 0x80) {
				SDA_HIGH();
		} else {
				SDA_LOW();
		}
		IIC_Delay();
		SCL_HIGH();
		IIC_Delay();
		SCL_LOW();
		IIC_Delay();
	}
}


// IIC读取一个字节
uint8_t IIC_Read_Byte(uint8_t ack)
{
	uint8_t byte = 0;
	SDA_IN();
	for (uint8_t i = 0; i < 8; i++) 
	{
		SCL_LOW();
		IIC_Delay();
		SCL_HIGH();
		IIC_Delay();
		byte <<= 1;
		if (READ_SDA()) {
				byte |= 0x01;
		}
		IIC_Delay();
	}
	return byte;
}


// IIC等待ACK
uint8_t IIC_Wait_Ack(void)
{
	uint8_t wait;
	SDA_IN();
	IIC_Delay();
	SCL_HIGH();
	IIC_Delay();
	while (READ_SDA())
	{
		wait++;
		if (wait > 200)
		{
			IIC_Stop();
			return 1;
		}
	}
	SCL_LOW();
	IIC_Delay();
	return 0;
}


// IIC发送ACK
void IIC_Ack(void)
{
	SCL_LOW();
	SDA_OUT();
	SDA_LOW();
	IIC_Delay();
	SCL_HIGH();
	IIC_Delay();
	SCL_LOW();
	IIC_Delay();
}


// IIC发送Not ACK
void IIC_NAck(void)
{
	SCL_LOW();
	SDA_OUT();
	SDA_HIGH();
	IIC_Delay();
	SCL_HIGH();
	IIC_Delay();
	SCL_LOW();
	IIC_Delay();
}


/*
*
* SHT40功能函数
*
*/

// IIC发送指令
uint8_t IIC_Write_Command(uint8_t deviceAddr, uint8_t command)
{
	IIC_Start();
	IIC_Send_Byte(deviceAddr << 1); // IIC从机地址和写模式
	uint8_t ret = IIC_Wait_Ack();
	if(ret) return 1;
	IIC_Send_Byte(command); 				// 写指令
	IIC_Stop();
	return 0;
}


// IIC发送测量指令
uint8_t SHT40_Start_Measurement(void)
{
	uint8_t ret = IIC_Write_Command(SHT40_ADDRESS, SHT40_COMMAND_MEASURE_HIGH_PRECISION);
	HAL_Delay(10); // 延时10ms（手册中写的）
	return ret;
}


// IIC接收测量结果
uint8_t SHT40_Read_Measurement(uint8_t* data, uint8_t length)
{
	IIC_Start();
	IIC_Send_Byte((SHT40_ADDRESS << 1) | 0x01); // IIC从机地址和读模式
	uint8_t ret = IIC_Wait_Ack();
	if(ret) return 1;
	for (uint8_t i = 0; i < length-1; i++) {
		*data = IIC_Read_Byte(i < (length - 1)); // 从IIC读数据
		data++;
		IIC_Ack();
	}
	*data = IIC_Read_Byte(0);
	IIC_NAck();
	IIC_Stop();
	return 0;
}
