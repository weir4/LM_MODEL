/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_adc.h
 * 文件标识： 
 * 内容摘要： 模拟量采集驱动文件
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月01日
 *
 *******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef LM_ADC_H
#define LM_ADC_H

/* Includes ------------------------------------------------------------------*/
#include "sys.h"
/* Exported types ------------------------------------------------------------*/

/**
  * @brief  ADC用户配置结构体
  */
typedef struct {
    ADC_TypeDef *port;                 /*!< ADC端口 */
    uint32_t ClockPrescaler;           /*!< 时钟预分频 */
    uint32_t Resolution;               /*!< 分辨率 */
    uint32_t ScanConvMode;             /*!< 扫描模式 */
    uint32_t EOCSelection;             /*!< 转换结束标志选择 */
    uint32_t ContinuousConvMode;       /*!< 连续转换模式 */
    uint32_t ExternalTrigConv;         /*!< 外部触发源 */
    uint32_t NbrOfConversion;          /*!< 转换通道数量 */
} adc_userconfig_t;

/**
  * @brief  ADC调试配置结构体
  */
typedef struct {
    uint32_t xtest;                    /*!< 测试参数 */
} adc_debugconfig_t;

/**
  * @brief  ADC设备句柄结构体
  */
typedef struct {
    adc_userconfig_t   userconfig_t;   /*!< 用户配置参数 */
    adc_debugconfig_t  debugconfig_t;  /*!< 调试配置参数 */
    ADC_HandleTypeDef  hadc_x;         /*!< HAL库ADC句柄 */
} adc_handle_t;

/**
  * @brief  ADC通道配置结构体
  */
typedef struct {
    uint32_t Channel;                  /*!< ADC通道配置 
                                        This parameter can be a value of @ref ADC_channels */
    uint32_t Rank;                     /*!< 规则组序列器中的排名
                                        This parameter must be a number between Min_Data = 1 and Max_Data = 16 */
    uint32_t SamplingTime;             /*!< 所选通道的采样时间值
                                        This parameter can be a value of @ref ADC_sampling_times */
} adc_ChannelConf;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

adc_handle_t* lm_create_adc_device(adc_userconfig_t *userconfig);
void lm_adc_config_channal(adc_handle_t *dev, adc_ChannelConf *channelconfig);
void lm_adc_start_dma(adc_handle_t *dev, uint32_t* pData, uint32_t Length);
void lm_adb_bind_dma(void);
/* Private types -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private constants ---------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

#endif /* LM_ADC_H */

