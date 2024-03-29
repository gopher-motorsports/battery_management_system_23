/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define AMS_FAULT_OUT_Pin GPIO_PIN_13
#define AMS_FAULT_OUT_GPIO_Port GPIOC
#define IMD_FAULT_SDC_Pin GPIO_PIN_14
#define IMD_FAULT_SDC_GPIO_Port GPIOC
#define BSPD_FAULT_SDC_Pin GPIO_PIN_15
#define BSPD_FAULT_SDC_GPIO_Port GPIOC
#define MCU_HEARTBEAT_Pin GPIO_PIN_0
#define MCU_HEARTBEAT_GPIO_Port GPIOC
#define MCU_FAULT_Pin GPIO_PIN_1
#define MCU_FAULT_GPIO_Port GPIOC
#define MCU_GSENSE_Pin GPIO_PIN_2
#define MCU_GSENSE_GPIO_Port GPIOC
#define CS_ASCI_Pin GPIO_PIN_4
#define CS_ASCI_GPIO_Port GPIOA
#define SHDN_Pin GPIO_PIN_4
#define SHDN_GPIO_Port GPIOC
#define INT_Pin GPIO_PIN_5
#define INT_GPIO_Port GPIOC
#define INT_EXTI_IRQn EXTI9_5_IRQn
#define RST_Pin GPIO_PIN_10
#define RST_GPIO_Port GPIOB
#define CS_EPD_Pin GPIO_PIN_12
#define CS_EPD_GPIO_Port GPIOB
#define DC_Pin GPIO_PIN_14
#define DC_GPIO_Port GPIOB
#define BUSY_Pin GPIO_PIN_6
#define BUSY_GPIO_Port GPIOC
#define BUSY_EXTI_IRQn EXTI9_5_IRQn
#define IMD_PWM_Pin GPIO_PIN_7
#define IMD_PWM_GPIO_Port GPIOC
#define TMS_Pin GPIO_PIN_13
#define TMS_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define AMS_FAULT_SDC_Pin GPIO_PIN_2
#define AMS_FAULT_SDC_GPIO_Port GPIOD
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
