/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_dma.h
 * 文件标识： 
 * 内容摘要： 模拟量采集驱动文件
 * 其它说明： 无
 * 当前版本： V1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月19日
 *
 *******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef LM_DMA_H
#define LM_DMA_H

/* Includes ------------------------------------------------------------------*/
#include "sys.h"

/* Exported types ------------------------------------------------------------*/

/**
  * @brief  DMA用户配置结构体
  * @note   用于存储DMA的基本配置参数
  */
typedef struct {
    DMA_Stream_TypeDef * port;           /*!< DMA流/通道端口选择 */
    uint32_t Channel;                    /*!< DMA通道选择 */
    uint32_t Direction;                  /*!< 数据传输方向 */
    uint32_t PeriphInc;                  /*!< 外设地址递增模式 */
    uint32_t MemInc;                     /*!< 存储器地址递增模式 */
    uint32_t PeriphDataAlignment;        /*!< 外设数据对齐方式 */
    uint32_t MemDataAlignment;           /*!< 存储器数据对齐方式 */
    uint32_t Priority;                   /*!< DMA通道优先级 */
} dma_userconfig_t;

/**
  * @brief  DMA调试配置结构体
  * @note   用于存储DMA的调试和高级配置参数
  */
typedef struct {
   uint32_t FIFOMode;                    /*!< FIFO模式使能/禁用 */
   uint32_t FIFOThreshold;               /*!< FIFO阈值设置 */
   uint32_t MemBurst;                    /*!< 存储器突发传输模式 */
   uint32_t PeriphBurst;                 /*!< 外设突发传输模式 */
} dma_debugconfig_t;

/**
  * @brief  DMA设备句柄结构体
  * @note   包含DMA设备的完整配置信息
  */
typedef struct {
    dma_userconfig_t   userconfig_t;     /*!< 用户配置参数 */
    dma_debugconfig_t  debugconfig_t;    /*!< 调试配置参数 */
    DMA_HandleTypeDef  hdma_x;           /*!< HAL库DMA句柄 */
} dma_handle_t;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

dma_handle_t* lm_create_dma_device(dma_userconfig_t *userconfig);
void lm_dma_irqconfig(dma_handle_t *handle_t, uint32_t PreemptPriority, uint32_t SubPriority);

/* Private types -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private constants ---------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

#endif /* LM_DMA_H */
