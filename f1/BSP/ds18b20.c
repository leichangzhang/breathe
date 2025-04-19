#include "ds18b20.h"
#include "main.h"
#include "us_delay.h"


/*
*
* 单总线IO操作
*
*/

// 初始化单总线
static void DS18B20_IO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    /* 总线空闲为高电平 */
    HAL_GPIO_WritePin(DQ_GPIO_Port, DQ_Pin, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = DQ_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DQ_GPIO_Port, &GPIO_InitStruct);
}


// 单总线设为输出模式
static void DS18B20_IO_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DQ_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DQ_GPIO_Port, &GPIO_InitStruct);
}


// 单总线设为输入模式
static void DS18B20_IO_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DQ_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DQ_GPIO_Port, &GPIO_InitStruct);
}


// 单总线写电平
static void DS18B20_DQ_OUT(int state)
{
    HAL_GPIO_WritePin(DQ_GPIO_Port, DQ_Pin, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}


// 单总线读电平
static int DS18B20_DQ_IN(void)
{
    return HAL_GPIO_ReadPin(DQ_GPIO_Port, DQ_Pin) == GPIO_PIN_SET ? 1 : 0;
}


/*
*
* DS18B20操作
*
*/

// 复位DS18B20
void DS18B20_Rst(void)
{
	DS18B20_IO_OUT(); 		//设定为输出模式
    DS18B20_DQ_OUT(0); 	//拉低DQ
    delay_us(750);    	//拉低750us
    DS18B20_DQ_OUT(1); 	//DQ=1
    delay_us(15);     	//15US
}


// 测试DS18B20
//  返回1:DS18B20无响应
//  返回0:成功
uint8_t DS18B20_Check(void)
{
    uint8_t retry=0;
		DS18B20_IO_IN();		//设定为输入模式
    while (DS18B20_DQ_IN()&&retry<200)
    {
        retry++;
        delay_us(1);
    };
    if(retry>=200)return 1;
    else retry=0;
    while (!DS18B20_DQ_IN()&&retry<240)
    {
        retry++;
        delay_us(1);
    };
    if(retry>=240)return 1;
    return 0;
}


// 从DS18B20读取一个位
uint8_t DS18B20_Read_Bit(void)
{
    uint8_t data;
    DS18B20_IO_OUT();
    DS18B20_DQ_OUT(0);
    delay_us(2);
    DS18B20_DQ_OUT(1);
    DS18B20_IO_IN();
    delay_us(12);
    if(DS18B20_DQ_IN()) data=1;
    else data=0;
    delay_us(50);
    return data;
}


// 从DS18B20读取一个字节
uint8_t DS18B20_Read_Byte(void)
{
    uint8_t i,j,dat;
    dat=0;
    for (i=1; i<=8; i++)
    {
        j=DS18B20_Read_Bit();
        dat=(j<<7)|(dat>>1);
    }
    return dat;
}


// 写一个字节到DS18B20
void DS18B20_Write_Byte(uint8_t dat)
{
    uint8_t j;
    uint8_t testb;
    DS18B20_IO_OUT();
    for (j=1; j<=8; j++)
    {
        testb=dat&0x01;
        dat=dat>>1;
        if (testb)
        {
            DS18B20_DQ_OUT(0);
            delay_us(2);
            DS18B20_DQ_OUT(1);
            delay_us(60);
        }
        else
        {
            DS18B20_DQ_OUT(0);
            delay_us(60);
            DS18B20_DQ_OUT(1);
            delay_us(2);
        }
    }
}


// 开始温度转换
void DS18B20_Start(void)
{
    DS18B20_Rst();
    DS18B20_Check();
    DS18B20_Write_Byte(0xcc);		// 跳过ROM
    DS18B20_Write_Byte(0x44);		// 转换指令
}


// 初始化DS18B20
uint8_t DS18B20_Init(void)
{
    DS18B20_IO_Init();
    DS18B20_Rst();
    return DS18B20_Check();
}


// 获取温度
//  精度：0.1C
//  返回值：温度值，范围：-550~1250
int16_t DS18B20_Get_Temp(void)
{
    uint8_t temp;
    uint8_t TL,TH;
    int16_t tem;

    __disable_irq();    /* 中断可能会单总线的时序从而导致读出来的温度值不正确，所以读取之前屏蔽中断 */

    DS18B20_Start ();
    DS18B20_Rst();
    DS18B20_Check();
    DS18B20_Write_Byte(0xcc);	// 跳过ROM
    DS18B20_Write_Byte(0xbe);	// 转换指令
    TL=DS18B20_Read_Byte(); 	// LSB
    TH=DS18B20_Read_Byte(); 	// MSB
    if(TH>7)
    {
        TH=~TH;
        TL=~TL;
        temp=0;		//温度为负
    }
    else temp=1; 	//温度为正
    tem=TH; 			//获得高八位
    tem<<=8;
    tem+=TL;			//获得低八位

    __enable_irq();    /* 再将全局中断打开 */

    tem=(float)tem*0.625;	//转换
    if(temp)return tem; 	//返回温度值
    else return -tem;
}

