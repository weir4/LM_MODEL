/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_gpio.h
 * 文件标识： 
 * 内容摘要： GPIO驱动文件
 * 其它说明： 无
 * 当前版本： V1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月19日
 *
 *******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef LM_GPIO_H
#define LM_GPIO_H

/* Includes ------------------------------------------------------------------*/
#include "sys.h"

/* Exported types ------------------------------------------------------------*/
/**
  * @brief  GPIO用户配置结构体
  */
typedef struct {
    GPIO_TypeDef *port;   /*!< GPIO端口 */
    uint32_t Pin;         /*!< GPIO引脚 */
    uint32_t Mode;        /*!< GPIO模式 */
    uint32_t Pull;        /*!< 上拉/下拉配置 */
    uint32_t Speed;       /*!< GPIO速度 */
} gpio_userconfig_t;

/**
  * @brief  GPIO调试配置结构体
  */
typedef struct {
    uint32_t xtest;       /*!< 测试参数 */
} gpio_debugconfig_t;

/**
  * @brief  GPIO设备句柄结构体
  */
typedef struct {
    gpio_userconfig_t   userconfig_t;   /*!< 用户配置参数 */
    gpio_debugconfig_t  debugconfig_t;  /*!< 调试配置参数 */
} gpio_handle_t;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
gpio_handle_t* lm_create_gpio_device(gpio_userconfig_t *userconfig);
void lm_gpio_write(gpio_handle_t *handle_t, GPIO_PinState state);
GPIO_PinState lm_gpio_read(gpio_handle_t *handle_t);
void lm_gpio_toggle(gpio_handle_t *handle_t);
void lm_gpio_irqconfig(gpio_handle_t *handle_t, uint32_t PreemptPriority, uint32_t SubPriority);

/* Private types -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private constants ---------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

#endif /* LM_GPIO_H */
