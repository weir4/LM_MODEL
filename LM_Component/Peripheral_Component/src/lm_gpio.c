/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_gpio.c
 * 文件标识： 
 * 内容摘要： GPIO驱动文件
 * 其它说明： 无
 * 当前版本： V1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月19日
 *
 *******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "lm_gpio.h"

/* Exported constants --------------------------------------------------------*/
#define MAX_GPIO_DEVICES 5

/* Private variables ---------------------------------------------------------*/
static gpio_handle_t gpio_devices[MAX_GPIO_DEVICES];
static uint8_t device_count = 0;
static void lm_gpio_interaction_hal(gpio_userconfig_t *userconfig_t);
/* Exported functions --------------------------------------------------------*/

/*******************************************************************************
函数名称：lm_create_gpio_device
功能描述：创建并初始化一个GPIO设备，将用户配置参数复制到设备句柄中，并调用HAL层初始化函数
输入参数：userconfig - GPIO用户配置参数结构体指针，包含端口、引脚、模式、上下拉和速度等配置信息
          具体包括：
          - port:  GPIO端口指针（如GPIOA, GPIOB等）
          - Pin:   GPIO引脚号（使用GPIO_PIN_x宏定义）
          - Mode:  GPIO工作模式（输入、输出、复用功能等）
          - Pull:  上下拉电阻配置（上拉、下拉、无上下拉）
          - Speed: GPIO输出速度（低速、中速、高速、超高速）
输出参数：无
返 回 值：gpio_handle_t* - 成功返回GPIO设备句柄指针，失败返回NULL
          返回值说明：
          - 非NULL: 创建成功，返回指向设备句柄的指针
          - NULL:   创建失败（设备数量已达上限或参数为空）
其它说明：1. 该函数会启用GPIOB时钟（需根据实际使用端口修改）
         2. 设备数量上限由MAX_GPIO_DEVICES宏定义控制
         3. 函数内部会自增设备计数器，无需外部管理
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/21     V1.00          Qiguo_Cui          创建
*******************************************************************************/

gpio_handle_t* lm_create_gpio_device(gpio_userconfig_t *userconfig) 
{
    /* 检查设备数量是否已达上限或输入参数是否为空 */
    if (device_count >= MAX_GPIO_DEVICES || userconfig == NULL ) {
        return NULL;  /* 设备数量已达上限或参数无效，返回NULL表示创建失败 */
    }

    /* 获取当前可用的设备句柄指针 */
    gpio_handle_t *dev = &gpio_devices[device_count++];
    
    /* 复制用户配置参数到设备句柄中 */
    dev->userconfig_t.port  = userconfig->port;   /* 设置GPIO端口 */
    dev->userconfig_t.Pin   = userconfig->Pin;    /* 设置GPIO引脚号 */
    dev->userconfig_t.Mode  = userconfig->Mode;   /* 设置GPIO工作模式 */
    dev->userconfig_t.Pull  = userconfig->Pull;   /* 设置上下拉电阻配置 */
    dev->userconfig_t.Speed = userconfig->Speed;  /* 设置GPIO输出速度 */
    
    /* 调用HAL层初始化函数，配置GPIO硬件寄存器 */
    lm_gpio_interaction_hal(userconfig);
    
    /* 返回创建成功的设备句柄指针 */
    return dev;
}


/*******************************************************************************
函数名称：lm_gpio_write
功能描述：向指定的GPIO引脚写入高低电平状态，控制引脚输出
输入参数：handle_t - GPIO设备句柄指针，包含GPIO端口和引脚配置信息
          state   - 引脚状态，可选值：
                   GPIO_PIN_SET   : 设置引脚为高电平
                   GPIO_PIN_RESET : 设置引脚为低电平
输出参数：无
返 回 值：无
其它说明：1. 如果传入的句柄指针为空，函数将直接返回不执行任何操作
         2. 该函数底层调用HAL库的HAL_GPIO_WritePin函数实现
         3. 使用前需确保GPIO已正确初始化为输出模式
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/21     V1.00          Qiguo_Cui          创建
*******************************************************************************/
void lm_gpio_write(gpio_handle_t *handle_t, GPIO_PinState state)
{
    /* 检查输入参数有效性：GPIO设备句柄指针不能为空 */
    if((gpio_handle_t*)handle_t == NULL)
    {
        /* 句柄为空，直接返回避免空指针异常 */
        return;
    }
    
    /* 调用HAL库函数向指定GPIO引脚写入电平状态 */
//    HAL_GPIO_WritePin(
//        handle_t->userconfig_t.port,    /* GPIO端口地址，如GPIOA、GPIOB等 */
//        handle_t->userconfig_t.Pin,     /* GPIO引脚号，使用GPIO_PIN_x宏定义 */
//        state                           /* 要设置的电平状态：高电平或低电平 */
//    );
}

/*******************************************************************************
函数名称：lm_gpio_toggle
功能描述：翻转指定GPIO引脚的电平状态（高电平变低电平，低电平变高电平）
输入参数：handle_t - GPIO设备句柄指针，包含GPIO端口和引脚配置信息
          具体包含：
          - userconfig_t.port: GPIO端口基地址（如GPIOA、GPIOB等）
          - userconfig_t.Pin:  GPIO引脚号（使用GPIO_PIN_x宏定义）
输出参数：无
返 回 值：无
其它说明：1. 如果传入的句柄指针为空，函数将直接返回不执行任何操作
         2. 该函数底层调用HAL库的HAL_GPIO_TogglePin函数实现
         3. 使用前需确保GPIO已正确初始化为输出模式
         4. 翻转操作是原子性的，适用于需要快速切换电平的场景
         5. 常用于LED闪烁、方波生成等应用
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/21     V1.00          Qiguo_Cui          创建
*******************************************************************************/
void lm_gpio_toggle(gpio_handle_t *handle_t) 
{
    /* 检查输入参数有效性：GPIO设备句柄指针不能为空 */
    if(handle_t == NULL)
    {
        /* 句柄为空，直接返回避免空指针异常 */
        return;
    }

    /* 调用HAL库函数翻转指定GPIO引脚的电平状态 */
//    HAL_GPIO_TogglePin(
//        handle_t->userconfig_t.port,    /* GPIO端口地址，如GPIOA、GPIOB等 */
//        handle_t->userconfig_t.Pin      /* GPIO引脚号，使用GPIO_PIN_x宏定义 */
//    );
}

/*******************************************************************************
函数名称：lm_gpio_irqconfig
功能描述：配置GPIO引脚的中断优先级参数（函数框架，待实现具体中断配置逻辑）
输入参数：handle_t        - GPIO设备句柄指针，包含GPIO端口和引脚配置信息
          具体包含：
          - userconfig_t.port: GPIO端口基地址（如GPIOA、GPIOB等）
          - userconfig_t.Pin:  GPIO引脚号（使用GPIO_PIN_x宏定义）
          PreemptPriority - 中断抢占优先级，范围取决于NVIC优先级分组设置
          SubPriority     - 中断子优先级，范围取决于NVIC优先级分组设置
输出参数：无
返 回 值：无
其它说明：1. 如果传入的句柄指针为空，函数将直接返回不执行任何操作
         2. 当前函数仅为框架，需要根据具体需求实现中断配置逻辑
         3. 使用时需要先配置GPIO为中断模式，再调用此函数设置优先级
         4. 需要配合NVIC_Init函数完成完整的中断配置
         5. 抢占优先级和子优先级的取值范围由NVIC_PriorityGroupConfig决定
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/21     V1.00          Qiguo_Cui          创建
*******************************************************************************/
void lm_gpio_irqconfig(gpio_handle_t *handle_t, uint32_t PreemptPriority, uint32_t SubPriority)
{
    /* 检查输入参数有效性：GPIO设备句柄指针不能为空 */
    if(handle_t == NULL)
    {
        /* 句柄为空，直接返回避免空指针异常 */
        return;
    }
    
    /* 待实现：具体的GPIO中断配置逻辑 */
    /* 需要在此处添加以下功能：*/
    /* 1. 配置NVIC中断通道 */
    /* 2. 设置中断优先级分组 */
    /* 3. 设置抢占优先级和子优先级 */
    /* 4. 使能NVIC中断通道 */
}
/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
函数名称：lm_gpio_interaction_hal
功能描述：GPIO与HAL层交互初始化函数，配置GPIO硬件寄存器参数
输入参数：userconfig - GPIO用户配置参数结构体指针，包含以下配置信息：
          - port:  GPIO端口指针（如GPIOA, GPIOB, GPIOC等）
          - Pin:   GPIO引脚号（使用GPIO_PIN_x宏定义，支持多引脚组合）
          - Mode:  GPIO工作模式（输入、输出、复用功能、模拟模式等）
          - Pull:  上下拉电阻配置（上拉、下拉、无上下拉）
          - Speed: GPIO输出速度（低速、中速、高速、超高速）
输出参数：无
返 回 值：无
其它说明：1. 函数内部会启用GPIOB时钟（注意：如需其他端口需修改使能语句）
         2. 该函数为静态函数，仅在当前文件内可见
         3. 使用HAL库的GPIO初始化结构体和初始化函数
         4. 配置完成后GPIO引脚将按照指定参数工作
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/21     V1.00          Qiguo_Cui          创建
*******************************************************************************/
static void lm_gpio_interaction_hal(gpio_userconfig_t *userconfig)
{
    /* 定义GPIO初始化结构体，用于存储GPIO配置参数 */
    GPIO_InitTypeDef GPIO_Initure;
    
    /* 使能GPIOB端口时钟（注意：如需其他端口需相应修改） */
    __HAL_RCC_GPIOB_CLK_ENABLE();           
	
    /* 配置GPIO引脚号：支持单个引脚或多个引脚组合 */
    GPIO_Initure.Pin = userconfig->Pin; 
    
    /* 配置GPIO工作模式：输入、输出、复用、模拟等模式 */
    GPIO_Initure.Mode = userconfig->Mode; 
    
    /* 配置GPIO上下拉电阻：上拉、下拉或无上下拉 */
    GPIO_Initure.Pull = userconfig->Pull;         
    
    /* 配置GPIO输出速度：低速、中速、高速或超高速 */
    GPIO_Initure.Speed = userconfig->Speed;
    
    /* 调用HAL库函数初始化GPIO，将配置参数写入硬件寄存器 */
    HAL_GPIO_Init(userconfig->port, &GPIO_Initure);
}
