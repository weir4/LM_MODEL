/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_timer.c
 * 文件标识： 
 * 内容摘要： 模拟量采集驱动文件
 * 其它说明： 无
 * 当前版本： V1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月19日
 *
 *******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LM_TIMER_C
#define __LM_TIMER_C

/* Includes ------------------------------------------------------------------*/
#include "lm_timer.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/**
  * @brief TIM1定时器句柄
  * @details 高级定时器TIM1的句柄，用于PWM输出控制
  */
TIM_HandleTypeDef htim1;

/**
  * @brief TIM3定时器句柄  
  * @details 通用定时器TIM3的句柄，预留用于其他功能
  */
TIM_HandleTypeDef htim3;

/**
  * @brief TIM4定时器句柄
  * @details 通用定时器TIM4的句柄，用于PWM输出控制
  */
TIM_HandleTypeDef htim4;

/* Private constants ---------------------------------------------------------*/

/**
  * @brief 定时器GPIO配置数组
  * @details 配置TIM1相关引脚的复用功能，包含主输出和互补输出
  * 索引0: TIM1_CH1 (PA8) - 通道1主输出
  * 索引1: TIM1_CH1N (PB13) - 通道1互补输出
  * 索引2: TIM1_CH2 (PA9) - 通道2主输出
  * 索引3: TIM1_CH2N (PB14) - 通道2互补输出
  */
static const GPIO_Config timGpioConfigs[4] = {
    [0] = {
        .port = GPIOA,                   /* GPIOA端口 */
        .pin = GPIO_PIN_8,               /* 引脚8，TIM1通道1主输出 */
        .mode = GPIO_MODE_AF_PP,         /* 复用推挽输出模式 */
        .speed = GPIO_SPEED_FREQ_HIGH,   /* 高速模式，适用于PWM输出 */
    },
    [1] = {
        .port = GPIOB,                   /* GPIOB端口 */
        .pin = GPIO_PIN_13,              /* 引脚13，TIM1通道1互补输出 */
        .mode = GPIO_MODE_AF_PP,         /* 复用推挽输出模式 */
        .speed = GPIO_SPEED_FREQ_HIGH,   /* 高速模式，适用于PWM输出 */
    },
    [2] = {
        .port = GPIOA,                   /* GPIOA端口 */
        .pin = GPIO_PIN_9,               /* 引脚9，TIM1通道2主输出 */
        .mode = GPIO_MODE_AF_PP,         /* 复用推挽输出模式 */
        .speed = GPIO_SPEED_FREQ_HIGH,   /* 高速模式，适用于PWM输出 */
    },
    [3] = {
        .port = GPIOB,                   /* GPIOB端口 */
        .pin = GPIO_PIN_14,              /* 引脚14，TIM1通道2互补输出 */
        .mode = GPIO_MODE_AF_PP,         /* 复用推挽输出模式 */
        .speed = GPIO_SPEED_FREQ_HIGH,   /* 高速模式，适用于PWM输出 */
    }
};

/**
  * @brief TIM4输出比较配置
  * @details 配置TIM4通道4的PWM输出参数，用于通用定时器PWM输出
  */
static const TIM_OC_Config_t tim4OcConfig = {
    .ocMode = TIM_OCMODE_PWM1,       /* PWM模式1，在向上计数时脉冲有效 */
    .outputState = TIM_OUTPUTSTATE_ENABLE, /* 输出使能，允许PWM信号输出 */
    .pulse = 0,                      /* 初始脉冲宽度为0，即0%占空比 */
    .ocPolarity = TIM_OCPOLARITY_HIGH, /* 输出极性为高电平有效 */
};

/* Private macros ------------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
 函数名称：lm_timer_funtion_init
 功能描述：定时器功能初始化主函数，统一初始化所有定时器相关配置    
 输入参数：无   
 输出参数：无    
 返 回 值：无    
 其它说明：此函数按照顺序调用所有定时器的GPIO配置和定时器配置函数
           完成TIM1高级定时器和TIM4通用定时器的完整初始化流程
           初始化顺序：TIM1 GPIO → TIM1配置 → TIM4 GPIO → TIM4配置			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/10/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_timer_funtion_init(void)
{
    /* 初始化TIM1相关GPIO引脚配置，配置主输出和互补输出引脚 */
    lm_timer_x_gpio_config();
    
    /* 初始化TIM1高级定时器配置，包括时基、PWM通道、刹车死区等 */
    lm_timer_x_configuration();

    /* 初始化TIM4相关GPIO引脚配置，配置通道4输出引脚 */
    lm_timer_4_gpio_config();
    
    /* 初始化TIM4通用定时器配置，配置PWM输出功能 */
    lm_timer_4_configuration();
}

/*******************************************************************************
 函数名称：lm_timer_x_gpio_config
 功能描述：配置TIM1高级定时器相关GPIO引脚复用功能    
 输入参数：无   
 输出参数：无    
 返 回 值：无    
 其它说明：此函数配置TIM1通道1、通道1N、通道2、通道2N的GPIO引脚
           使用复用推挽输出模式，启用相关外设时钟
           配置的引脚包括：PA8(TIM1_CH1)、PB13(TIM1_CH1N)、PA9(TIM1_CH2)、PB14(TIM1_CH2N)			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/10/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_timer_x_gpio_config(void) 
{
    GPIO_InitTypeDef GPIO_InitStruct;    /* GPIO初始化结构体，用于配置引脚参数 */
    
    /* 使能GPIOA和GPIOB时钟，这两个端口用于TIM1的PWM输出引脚 */
    __HAL_RCC_GPIOA_CLK_ENABLE();        /* 使能GPIOA端口时钟，用于PA8和PA9引脚 */
    __HAL_RCC_GPIOB_CLK_ENABLE();        /* 使能GPIOB端口时钟，用于PB13和PB14引脚 */
	
    /* 使能TIM1和TIM4外设时钟，TIM1为高级定时器，TIM4为通用定时器 */
    __HAL_RCC_TIM1_CLK_ENABLE();         /* 使能TIM1高级定时器时钟，APB2总线 */
    __HAL_RCC_TIM4_CLK_ENABLE();         /* 使能TIM4通用定时器时钟，APB1总线 */
    
    /* 循环配置所有定时器GPIO引脚，遍历timGpioConfigs数组中的4个引脚配置 */
    for (int i = 0; i < sizeof(timGpioConfigs)/sizeof(timGpioConfigs[0]); i++) {
        GPIO_InitStruct.Pin = timGpioConfigs[i].pin;        /* 设置引脚号，从配置数组中获取 */
        GPIO_InitStruct.Mode = timGpioConfigs[i].mode;      /* 设置引脚模式为复用推挽输出 */
        GPIO_InitStruct.Speed = timGpioConfigs[i].speed;    /* 设置引脚速度为高速模式 */
        HAL_GPIO_Init(timGpioConfigs[i].port, &GPIO_InitStruct);  /* 初始化GPIO引脚，应用配置参数 */
    }
}

/*******************************************************************************
 函数名称：lm_timer_x_configuration
 功能描述：配置TIM1高级定时器的完整参数，包括时基、PWM输出、刹车死区等    
 输入参数：无   
 输出参数：无    
 返 回 值：无    
 其它说明：此函数详细配置TIM1的时基单元、PWM输出通道、刹车死区时间参数
           启动定时器和所有PWM输出，包括主输出和互补输出
           配置参数：预分频器0、周期值999、向上计数、使能自动重载预装载			   
 修改日期      版本号          修改人            修改人          修改内容
 -----------------------------------------------------------------------------
 2025/10/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_timer_x_configuration(void)
{
    TIM_OC_InitTypeDef sConfigOC;                          /* 输出比较配置结构体，用于PWM通道配置 */
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig;   /* 刹车死区配置结构体，用于高级定时器保护功能 */

    /* 初始化TIM1时基单元配置，设置定时器基本工作参数 */
    htim1.Instance = TIM1;                                 /* 选择TIM1高级定时器 */
    htim1.Init.Prescaler = 0;                              /* 预分频器为0，定时器时钟不分频 */
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;           /* 向上计数模式，计数器从0增加到自动重载值 */
    htim1.Init.Period = 999;                               /* 自动重载值，决定PWM频率和定时器周期 */
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;     /* 时钟分频，不分频，使用原始时钟频率 */
    htim1.Init.RepetitionCounter = 0;                      /* 重复计数器，高级定时器特有，用于生成更新事件 */
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;  /* 使能自动重载预装载，避免运行时修改影响 */
    
    /* 初始化定时器时基单元，应用上述配置参数 */
    HAL_TIM_Base_Init(&htim1);

    /* 配置输出比较参数，设置PWM输出的详细特性 */
    sConfigOC.OCMode = TIM_OCMODE_PWM1;                    /* PWM模式1，在计数器小于比较值时输出有效电平 */
    sConfigOC.Pulse = 0;                                   /* 初始占空比为0%，比较寄存器初始值 */
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;            /* 主输出高电平有效，有效时为高电平 */
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;          /* 互补输出高电平有效，有效时为高电平 */
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;             /* 禁用快速模式，正常PWM输出 */
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;         /* 空闲时主输出为低电平，刹车或关闭时的状态 */
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;       /* 空闲时互补输出为低电平，刹车或关闭时的状态 */

    /* 配置TIM1通道1和通道2的PWM输出，应用相同的输出比较配置 */
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1);   /* 配置通道1的PWM输出参数 */
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2);   /* 配置通道2的PWM输出参数 */
	 
    /* 配置刹车和死区时间参数，设置高级定时器的保护功能 */
    sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_ENABLE;         /* 运行模式下关闭状态选择，启用运行状态关闭 */
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_ENABLE;        /* 空闲模式下关闭状态选择，启用空闲状态关闭 */
    sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_1;               /* 锁定级别1，防止误写关键寄存器 */
    sBreakDeadTimeConfig.DeadTime = 72;                             /* 死区时间设置，防止上下桥臂直通，单位取决于时钟 */
    sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;            /* 禁用刹车功能，不使用外部刹车信号 */
    sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;    /* 刹车极性为高电平有效，配置为高电平触发 */
    sBreakDeadTimeConfig.BreakFilter = 0;                           /* 刹车输入滤波器，无滤波 */
    sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;  /* 禁用自动输出，手动控制输出状态 */
																						
    /* 配置刹车和死区时间，应用上述保护功能参数 */
    HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig);

    /* 启动定时器基础功能，开始计数器计数 */
    HAL_TIM_Base_Start(&htim1);
    
    /* 使能主输出，高级定时器必需，否则PWM输出被禁止 */
    __HAL_TIM_MOE_ENABLE(&htim1);
    
    /* 启动PWM输出，使能各个通道的PWM信号生成 */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);              /* 启动通道1主输出PWM信号 */
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);           /* 启动通道1互补输出PWM信号 */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);              /* 启动通道2主输出PWM信号 */
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);           /* 启动通道2互补输出PWM信号 */
}

/*******************************************************************************
 函数名称：lm_timer_4_configuration
 功能描述：配置TIM4通用定时器的PWM输出功能    
 输入参数：无   
 输出参数：无    
 返 回 值：无    
 其它说明：此函数配置TIM4通道4的PWM输出，使用预定义的输出比较配置参数
           启动定时器和PWM输出，适用于简单的PWM信号生成
           配置参数与TIM1类似，但不包含互补输出和刹车死区功能			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/10/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_timer_4_configuration(void)
{
    TIM_OC_InitTypeDef sConfigOC;        /* 输出比较配置结构体，用于TIM4的PWM通道配置 */
    
    /* 使能TIM4时钟，TIM4挂载在APB1总线上 */
    __HAL_RCC_TIM4_CLK_ENABLE();         /* 使能TIM4通用定时器时钟 */
    
    /* 配置TIM4时基参数，设置定时器基本工作模式 */
    htim4.Instance = TIM4;               /* 选择TIM4通用定时器 */
    htim4.Init.Prescaler = 0;            /* 预分频器为0，定时器时钟不分频 */
    htim4.Init.CounterMode = TIM_COUNTERMODE_UP;     /* 向上计数模式，计数器从0增加到自动重载值 */
    htim4.Init.Period = 999;             /* 自动重载值，决定PWM频率和定时器周期 */
    htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;  /* 时钟分频，不分频，使用原始时钟频率 */
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;  /* 使能自动重载预装载，缓冲ARR寄存器 */
    
    /* 初始化定时器时基单元，应用上述配置参数 */
    HAL_TIM_Base_Init(&htim4);
    
    /* 初始化PWM模式，为PWM输出配置做准备 */
    HAL_TIM_PWM_Init(&htim4);
    
    /* 配置输出比较参数 - 通道4，使用预定义的配置参数 */
    sConfigOC.OCMode = tim4OcConfig.ocMode;          /* 使用预定义的PWM模式1 */
    sConfigOC.Pulse = tim4OcConfig.pulse;            /* 使用预定义的脉冲宽度，初始为0 */
    sConfigOC.OCPolarity = tim4OcConfig.ocPolarity;  /* 使用预定义的输出极性，高电平有效 */
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;       /* 禁用快速模式，正常PWM输出 */
    
    /* 配置TIM4通道4的PWM输出，应用输出比较参数 */
    HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_4);
    
    /* 启动定时器基础功能，开始计数器计数 */
    HAL_TIM_Base_Start(&htim4);
    
    /* 启动PWM输出，使能通道4的PWM信号生成 */
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);        /* 启动TIM4通道4的PWM输出 */
}

/*******************************************************************************
 函数名称：lm_timer_4_gpio_config
 功能描述：配置TIM4通道4的GPIO引脚复用功能    
 输入参数：无   
 输出参数：无    
 返 回 值：无    
 其它说明：此函数配置GPIOB引脚9为TIM4通道4的复用功能输出
           使用推挽输出模式，高速模式，无上下拉电阻
           引脚对应关系：GPIOB Pin9 → TIM4 Channel4			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/10/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_timer_4_gpio_config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};    /* GPIO初始化结构体，初始化为0 */
    
    /* 使能TIM4时钟，TIM4挂载在APB1总线上 */
    __HAL_RCC_TIM4_CLK_ENABLE();               /* 使能TIM4通用定时器时钟 */
    
    /* 使能GPIOB时钟，GPIOB挂载在APB2总线上 */
    __HAL_RCC_GPIOB_CLK_ENABLE();              /* 使能GPIOB端口时钟 */
    
    /* 配置GPIOB Pin 9为TIM4通道4的复用推挽输出 */
    GPIO_InitStruct.Pin = GPIO_PIN_9;          /* 选择引脚9，TIM4通道4对应引脚 */
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;    /* 复用推挽输出模式，用于定时器输出 */
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;  /* 高速模式，适用于PWM信号输出 */
    GPIO_InitStruct.Pull = GPIO_NOPULL;        /* 无上下拉电阻，浮空输入模式 */
    
    /* 初始化GPIO引脚，应用上述配置参数 */
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

#endif /* __LM_TIMER_C */


