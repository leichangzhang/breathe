/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sht40.h"
#include "ds18b20.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define DS18B20_INIT_FAILED		1
#define ERR_UART_TIMEOUT			2
#define ERR_UART_ERROR				3
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t flagTrigged = 0;
uint8_t flagErrorCode = 0;

uint8_t sensor1DataBuffer[6] = {0}; // SHT40数据缓存
double sensor1Temperature = 0;
double sensor1Humidity = 0;

int16_t sensor2DataBuffer[3] = {0}; // DS18B20数据缓存

char sendBuffer[50] = {'\0'};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void LED_ErrorCode(uint8_t code);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*
DS18B20 代码使用示例

int main()
{
    short temp;

    while(DS18B20_Init())
    {
        printf(" ds18b20 init failed ! \r\n");
        HAL_Delay(1000);
    }

    while(1)
    {
        temp = DS18B20_Get_Temp();
        printf("当前温度:%0.2f \r\n", (float)temp / 10);
        HAL_Delay(1000);
    }
}

*/
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

	while(DS18B20_Init())
	{
		LED_ErrorCode(DS18B20_INIT_FAILED);
	}
	
	 __HAL_TIM_SET_COUNTER(&htim2, 0);
	HAL_TIM_Base_Start_IT(&htim2);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		if(flagTrigged == 1)
		{
			// 打开LED
			HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
			
			// SHT40 测量温湿度数据
			SHT40_Start_Measurement();
			SHT40_Read_Measurement((uint8_t*)sensor1DataBuffer,6);
			sensor1Temperature = (1.0 * 175 * (sensor1DataBuffer[0] * 256 + sensor1DataBuffer[1])) / 65535.0 - 45;
			sensor1Humidity = (1.0 * 125 * (sensor1DataBuffer[3] * 256 + sensor1DataBuffer[4])) / 65535.0 - 6.0;
			
			// DS18B20 测量温度数据
			sensor2DataBuffer[0] = DS18B20_Get_Temp();
			HAL_Delay(1);
			sensor2DataBuffer[1] = DS18B20_Get_Temp();
			HAL_Delay(1);
			sensor2DataBuffer[2] = DS18B20_Get_Temp();
			HAL_Delay(1);
			double sensor2Temperature = (double)(sensor2DataBuffer[0] + sensor2DataBuffer[1] + sensor2DataBuffer[2]) / 3.0f / 10.0f;
			
			// HC05 发送数据
			memset(sendBuffer, '\0', sizeof(sendBuffer));
			sprintf(sendBuffer, "<TP%.2lf,HM%.2lf,TP%.2lf>", sensor1Temperature, sensor1Humidity, sensor2Temperature);
//			sprintf(sendBuffer, "<TP%.2lf,HM%.2lf,TP%.2lf>", 33.44, 55.66, 77.88); //Debug Only
			HAL_StatusTypeDef ret = HAL_UART_Transmit(&huart1, (uint8_t*)sendBuffer, strlen(sendBuffer), 5);
			if(ret != HAL_OK)
			{
				if(ret == HAL_TIMEOUT)
				{
					flagErrorCode = ERR_UART_TIMEOUT;
				}
				else
				{
					flagErrorCode = ERR_UART_ERROR;
				}
				break;
			}
			
			// 关闭LED
			HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
			flagTrigged = 0;
		}
		else
		{
			HAL_Delay(1);
		}
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
	while(1)
	{
		LED_ErrorCode(flagErrorCode);
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if(htim->Instance == TIM2)
	{
		flagTrigged = 1;
	}
}

void LED_Blink(void)
{
	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
	HAL_Delay(200);
	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
	HAL_Delay(200);
}

void LED_ErrorCode(uint8_t code)
{
	if(code == DS18B20_INIT_FAILED)		// 闪烁三次
	{
		LED_Blink();
		LED_Blink();
		LED_Blink();
		HAL_Delay(3000);
	}
	else if(code == ERR_UART_TIMEOUT)	// 闪烁四次
	{
		LED_Blink();
		LED_Blink();
		LED_Blink();
		LED_Blink();
		HAL_Delay(3000);
	}
	else if(code == ERR_UART_ERROR) 	// 闪烁五次
	{
		LED_Blink();
		LED_Blink();
		LED_Blink();
		LED_Blink();
		LED_Blink();
		HAL_Delay(3000);
	}
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
