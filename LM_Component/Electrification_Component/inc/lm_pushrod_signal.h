/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_pushrod_signal.h
 * 文件标识： 
 * 内容摘要： 电推杆信号捕获与处理
 * 其它说明： 无
 * 当前版本： V1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月01日
 *
 *******************************************************************************/


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef LM_PUSHROD_SIGNAL_H
#define LM_PUSHROD_SIGNAL_H

/* Includes ------------------------------------------------------------------*/
#include "sys.h"
extern uint8_t PushActuatorCommandCurrentSignal[4];
/* Exported types ------------------------------------------------------------*/

/**
  * @brief  输入信号类型枚举定义
  * @note   用于标识不同的输入信号类型
  */
typedef enum {
    INPUT_UP_SIGNAL = 0,     /*!< 上升信号输入 */
    INPUT_DOWN_SIGNAL = 1,       /*!< 下降信号输入 */
    INPUT_UP_LIMIT = 2,          /*!< 上限位信号输入 */
    INPUT_DOWN_LIMIT = 3,        /*!< 下限位信号输入 */
    INPUT_COUNT = 4              /*!< 输入信号总数，自动计算枚举数量 */
} InputSignalType;

/**
  * @brief  输入信号状态枚举定义
  * @note   用于表示输入信号的激活状态
  */
typedef enum {
    INPUT_INACTIVE = 0,      /*!< 输入信号未激活状态 */
    INPUT_ACTIVE = 1,        /*!< 输入信号激活状态 */
} InputState;

/**
  * @brief  GPIO配置结构体定义
  * @note   用于配置GPIO端口的相关参数
  */
typedef struct {
    GPIO_TypeDef* port;      /*!< GPIO端口指针 */
    uint32_t pin;            /*!< GPIO引脚号 */
    uint32_t mode;           /*!< GPIO工作模式 */
    uint32_t speed;          /*!< GPIO输出速度 */
} GPIO_Config_t;



typedef enum
{ Bit_RESET = 0,
  Bit_SET
}BitAction;

/* Exported constants --------------------------------------------------------*/
/* 暂无导出的常量定义 */

/* Exported macro ------------------------------------------------------------*/
/* 暂无导出的宏定义 */

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  HAL库 STM32F1xx系列函数声明
  */

void lm_signal_init(void);
void lm_signal_deal(void);
InputState lm_getpushrodcursignal(InputSignalType index);
InputState lm_getpushrodlastsignal(InputSignalType index);
void lm_setpushrod_cursignal(InputSignalType index, InputState state);
void lm_setpushrod_lastsignal(InputSignalType index, InputState state);
InputState lm_input_read(InputSignalType signal);


void lm_pa10_interrupt_handler(void);
void lm_pa11_interrupt_handler(void);
/* Private types -------------------------------------------------------------*/
/* 暂无私有类型定义 */

/* Private variables ---------------------------------------------------------*/
/* 暂无私有变量定义 */

/* Private constants ---------------------------------------------------------*/
/* 暂无私有常量定义 */

/* Private macros ------------------------------------------------------------*/
/* 暂无私有宏定义 */

/* Private functions ---------------------------------------------------------*/
/* 暂无私有函数声明 */

#endif /* LM_PUSHROD_SIGNAL_H */


