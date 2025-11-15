/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_timer.h
 * 文件标识： 
 * 内容摘要： 模拟量采集驱动文件
 * 其它说明： 无
 * 当前版本： V1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月19日
 *
 *******************************************************************************/
   
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef LM_TIMER_H
#define LM_TIMER_H

/* Includes ------------------------------------------------------------------*/
#include "sys.h"
#include "stdint.h"

/* Exported types ------------------------------------------------------------*/

/**
  * @brief GPIO配置结构体
  * @details 用于配置GPIO引脚的基本参数，包括端口、引脚号、时钟、模式和速度
  */
typedef struct {
    GPIO_TypeDef* port;           /**< GPIO端口指针，如GPIOA、GPIOB等 */
    uint32_t pin;                 /**< 引脚号，使用GPIO_PIN_x宏定义 */
    uint32_t rcc_clock;           /**< RCC时钟使能标识，用于使能对应GPIO端口的时钟 */
    uint32_t mode;                /**< GPIO工作模式，如输入、输出、复用功能等 */
    uint32_t speed;               /**< GPIO输出速度，如低速、中速、高速等 */
} GPIO_Config;

/**
  * @brief NVIC配置结构体
  * @details 用于配置中断控制器的参数，包括中断通道和优先级设置
  */
typedef struct {
    uint8_t IRQChannel;           /**< 中断通道号，对应具体的外设中断 */
    uint8_t PreemptionPriority;   /**< 抢占优先级，数值越小优先级越高 */
    uint8_t SubPriority;          /**< 子优先级，在同一抢占优先级内的子优先级 */
} NVIC_Config;

/**
  * @brief 定时器时基配置结构体
  * @details 用于配置定时器的基本定时参数，包括周期、预分频等
  */
typedef struct {
    uint16_t period;              /**< 自动重载值，决定定时器周期和PWM频率 */
    uint16_t prescaler;           /**< 预分频值，用于分频定时器时钟 */
    uint16_t clockDivision;       /**< 时钟分频，用于进一步分频定时器时钟 */
    uint16_t counterMode;         /**< 计数模式，如向上计数、向下计数等 */
    uint16_t deadTime;            /**< 死区时间，用于高级定时器的互补输出保护 */
} TIM_BaseConfig_t;

/**
  * @brief 定时器输出比较配置结构体
  * @details 用于配置定时器的PWM输出参数，包括模式、极性、脉冲宽度等
  */
typedef struct {
    uint16_t ocMode;              /**< 输出比较模式，如PWM模式1、PWM模式2等 */
    uint16_t outputState;         /**< 输出状态，使能或禁用输出 */
    uint16_t outputNState;        /**< 互补输出状态，使能或禁用互补输出 */
    uint16_t pulse;               /**< 脉冲宽度，决定PWM占空比 */
    uint16_t ocPolarity;          /**< 输出极性，高电平有效或低电平有效 */
    uint16_t ocnPolarity;         /**< 互补输出极性，高电平有效或低电平有效 */
    uint16_t ocIdleState;         /**< 输出空闲状态，定时器停止时的输出电平 */
    uint16_t ocnIdleState;        /**< 互补输出空闲状态，定时器停止时的互补输出电平 */
} TIM_OC_Config_t;

/**
  * @brief 定时器刹车和死区配置结构体
  * @details 用于配置高级定时器的刹车功能和死区时间参数
  */
typedef struct {
    uint16_t ossrState;           /**< 运行模式下的关闭状态选择 */
    uint16_t ossiState;           /**< 空闲模式下的关闭状态选择 */
    uint16_t lockLevel;           /**< 锁定级别，防止误写关键寄存器 */
    uint16_t deadTime;            /**< 死区时间，防止互补输出的上下桥臂直通 */
    uint16_t breakState;          /**< 刹车状态，使能或禁用刹车功能 */
    uint16_t breakPolarity;       /**< 刹车极性，刹车输入信号的极性 */
    uint16_t automaticOutput;     /**< 自动输出使能，刹车时自动控制输出状态 */
} TIM_BDTR_Config_t;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/**
  * @brief 电机PWM频率定义
  * @details 设置电机控制的PWM信号频率为20kHz
  */
#define MOTOR_TIM_PWM_FREQ                20000

/**
  * @brief 电机定时器预分频器定义
  * @details 设置定时器预分频值为0，即不分频
  */
#define MOTOR_TIM_PRESCALER               0

/**
  * @brief 电机定时器周期计算宏
  * @details 根据系统时钟和PWM频率计算定时器的自动重载值
  * 计算公式：周期值 = 系统时钟 / (预分频器+1) / PWM频率
  */
#define MOTOR_TIM_PERIOD                 (uint16_t)(SystemCoreClock/(MOTOR_TIM_PRESCALER+1)/MOTOR_TIM_PWM_FREQ)

/* Exported functions --------------------------------------------------------*/

/**
  * @brief TIM1定时器句柄外部声明
  * @details 高级定时器TIM1的全局句柄，用于PWM输出控制
  */
extern TIM_HandleTypeDef htim1;

/**
  * @brief TIM3定时器句柄外部声明
  * @details 通用定时器TIM3的全局句柄，预留用于其他功能
  */
extern TIM_HandleTypeDef htim3;

/**
  * @brief TIM4定时器句柄外部声明
  * @details 通用定时器TIM4的全局句柄，用于PWM输出控制
  */
extern TIM_HandleTypeDef htim4;

/* Private types -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private constants ---------------------------------------------------------*/
/* Private macros ------------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

void lm_timer_x_gpio_config(void);
void lm_timer_4_gpio_config(void);
void lm_timer_x_configuration(void);
void lm_timer_4_configuration(void);
void lm_timer_funtion_init(void);
void lm_timer_nvic_configuration(void);
void lm_timer_configuration(void);

#endif /* LM_TIMER_H */
