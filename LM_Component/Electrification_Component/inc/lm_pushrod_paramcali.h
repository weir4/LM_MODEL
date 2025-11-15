/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_pushrod_paramcali.h
 * 文件标识： 
 * 内容摘要： 电推杆控制
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年10月18日
 *
 *******************************************************************************/



/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef LM_PUSHROD_PARAMCALI_H
#define LM_PUSHROD_PARAMCALI_H

/* Includes ------------------------------------------------------------------*/
#include "sys.h"
#include "stm32f1xx_hal_conf.h"

/* Exported types ------------------------------------------------------------*/

/**
  * @brief ADC通道配置结构体
  * @details 用于配置ADC转换通道的相关参数，包括通道选择、转换顺序和采样时间
  */
typedef struct {
    uint32_t ADC_Channel;         /**< ADC通道号，选择具体的ADC输入通道 */
    uint8_t Rank;                 /**< 转换序列，指定在扫描模式中的转换顺序 */
    uint8_t SampleTime;           /**< 采样时间，设置ADC对输入信号的采样持续时间 */
} ADC_ChannelConfig;

/**
  * @brief 电机参数结构体
  * @details 用于存储电机的关键运行参数，包括总线电流和电源电压
  */
typedef struct {
    float Current_Bus;            /**< 总线电流值，单位：安培(A) */
    float Voltage_Power;          /**< 电源电压值，单位：伏特(V) */
} motor_param_t;

enum {
	  Current = 0,
	  Voltage = 1
};


/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private constants ---------------------------------------------------------*/
/* Private macros ------------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
void lm_param_deal(void);
static void lm_param_gpio_init(void);
void lm_param_adc_config(void);
void lm_param_dma_init(void);
void HAL_DMA_XferCpltCallback(DMA_HandleTypeDef *hdma);
void lm_adc_filter(void);
#endif /* LM_PUSHROD_PARAMCALI_H */
