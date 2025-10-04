/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_adc.c
 * 文件标识： 
 * 内容摘要： 模拟量采集驱动文件
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月01日
 *
 *******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "lm_adc.h"
/* Private constants ---------------------------------------------------------*/
#define MAX_ADC_DEVICES 3

/* Private variables ---------------------------------------------------------*/
static adc_handle_t adc_devices[MAX_ADC_DEVICES];
static uint8_t device_count = 0;

/* Private functions ---------------------------------------------------------*/
static void lm_adc_interaction_hal(adc_handle_t *dev);

/* Exported functions --------------------------------------------------------*/

/*******************************************************************************
 函数名称：lm_create_adc_device
 功能描述：创建并初始化ADC设备实例，配置ADC基本参数
 输入参数：userconfig - 指向ADC用户配置结构体的指针，包含ADC基本配置参数
 输出参数：无
 返 回 值：成功返回ADC设备句柄指针，失败返回NULL
 其它说明：该函数会分配ADC设备实例并初始化用户配置参数
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/09/26     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
adc_handle_t* lm_create_adc_device(adc_userconfig_t *userconfig)
{
    /* 检查设备数量是否已达上限或输入参数是否为空 */
    if (device_count >= MAX_ADC_DEVICES || userconfig == NULL ) {
        return NULL;  /* 设备数量已达上限或参数无效，返回NULL表示创建失败 */
    }

    /* 获取当前可用的设备句柄指针 */
    adc_handle_t *dev = &adc_devices[device_count++];
    
    /* 将用户配置参数复制到设备结构体中 */
    dev->userconfig_t.port             =  userconfig->port;               /* 设置ADC外设端口（如ADC1、ADC2等） */
    dev->userconfig_t.ClockPrescaler   = userconfig->ClockPrescaler;      /* 设置ADC时钟预分频系数 */
    dev->userconfig_t.Resolution       = userconfig->Resolution;          /* 设置ADC分辨率（如12位、10位等） */
    dev->userconfig_t.ScanConvMode     = userconfig->ScanConvMode;        /* 设置扫描模式（启用/禁用多通道扫描） */
    dev->userconfig_t.EOCSelection     = userconfig->EOCSelection;        /* 设置转换结束标志选择方式 */
    dev->userconfig_t.ContinuousConvMode = userconfig->ContinuousConvMode;/* 设置连续转换模式（单次/连续转换） */
    dev->userconfig_t.ExternalTrigConv = userconfig->ExternalTrigConv;    /* 设置外部触发源（如定时器触发等） */
    dev->userconfig_t.NbrOfConversion  = userconfig->NbrOfConversion;     /* 设置转换通道数量 */

    /* 调用HAL库交互函数完成ADC硬件初始化 */
    lm_adc_interaction_hal(dev);
    
    return dev;  /* 返回初始化成功的ADC设备句柄指针 */
}

/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
 函数名称：lm_adc_interaction_hal
 功能描述：ADC与HAL库交互初始化，配置ADC硬件参数
 输入参数：dev - 指向ADC设备句柄的指针
 输出参数：无
 返 回 值：无
 其它说明：该函数将用户配置转换为HAL库配置并初始化ADC外设
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/09/26     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_adc_interaction_hal(adc_handle_t *dev)
{
    /* 配置ADC实例和基本参数 */
    dev->hadc_x.Instance = dev->userconfig_t.port;                        /* 设置ADC外设实例（如ADC1、ADC2等） */
    dev->hadc_x.Init.ClockPrescaler = dev->userconfig_t.ClockPrescaler;   /* 设置ADC时钟预分频 */
    dev->hadc_x.Init.Resolution = dev->userconfig_t.Resolution;           /* 设置ADC分辨率 */
    dev->hadc_x.Init.ScanConvMode = dev->userconfig_t.ScanConvMode;       /* 设置扫描转换模式 */
    dev->hadc_x.Init.EOCSelection = dev->userconfig_t.EOCSelection;       /* 设置转换结束标志选择 */
    dev->hadc_x.Init.ContinuousConvMode = dev->userconfig_t.ContinuousConvMode;  /* 设置连续转换模式 */
    dev->hadc_x.Init.ExternalTrigConv = dev->userconfig_t.ExternalTrigConv;      /* 设置外部触发转换 */
    dev->hadc_x.Init.NbrOfConversion =  dev->userconfig_t.NbrOfConversion;       /* 设置转换通道数量 */

    /* 配置固定的ADC参数（这些参数通常不需要用户修改） */
    dev->hadc_x.Init.DiscontinuousConvMode = DISABLE;                     /* 禁用不连续转换模式 */
    dev->hadc_x.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;/* 设置外部触发边沿为无触发 */
    dev->hadc_x.Init.DataAlign = ADC_DATAALIGN_RIGHT;                     /* 设置数据对齐方式为右对齐 */
    dev->hadc_x.Init.DMAContinuousRequests = ENABLE;                      /* 使能DMA连续请求 */
    //dev->hadc_x.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;                  /* 设置溢出时数据被覆盖 */

    /* 调用HAL库ADC初始化函数并检查返回值 */
    if (HAL_ADC_Init(&dev->hadc_x) != HAL_OK)
    {
//        Error_Handler();  /* ADC初始化失败，调用错误处理函数 */
    }
}

/*******************************************************************************
 函数名称：HAL_ADC_MspInit
 功能描述：HAL库ADC MSP初始化回调函数，配置ADC底层硬件
 输入参数：hadc - HAL库ADC句柄指针
 输出参数：无
 返 回 值：无
 其它说明：该函数由HAL_ADC_Init自动调用，用于配置ADC时钟和GPIO等底层硬件
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/09/26     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};  /* 定义GPIO初始化结构体并清零 */
    
    /* 根据ADC实例进行不同的硬件配置 */
    if(hadc->Instance == ADC1)  /* 判断是否为ADC1实例 */
    {
        /* 使能ADC1和GPIOA时钟 */
        __HAL_RCC_ADC1_CLK_ENABLE();   /* 使能ADC1外设时钟 */
        __HAL_RCC_GPIOA_CLK_ENABLE();  /* 使能GPIOA端口时钟 */
        
        /* 配置ADC引脚为模拟输入模式 */
        GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;  /* 设置PA0和PA1为ADC引脚 */
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;        /* 设置GPIO模式为模拟输入 */
        GPIO_InitStruct.Pull = GPIO_NOPULL;             /* 设置无上拉下拉电阻 */
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);         /* 初始化GPIOA配置 */
        
        /* 注释：以下是DMA配置的示例代码，需要时取消注释 */
        //HAL_DMA_Init(&hdma_adc1);  /* 初始化DMA用于ADC数据传输 */
        
        /* 注释：连接ADC和DMA（M4中用法与M3相同） */
        //HAL_LINKDMA(hadc, DMA_Handle, hdma_adc1);  /* 将ADC与DMA关联 */
        
        /* 注释：中断配置示例 */
        //HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);  /* 设置DMA中断优先级 */
        //HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);          /* 使能DMA中断 */
    }
    
    /* 为ADC2预留配置空间 */
    if(hadc->Instance == ADC2)  /* 判断是否为ADC2实例 */
    {
        ;  /* 空语句，ADC2配置待实现 */
    }

    /* 为ADC3预留配置空间 */
    if(hadc->Instance == ADC3)  /* 判断是否为ADC3实例 */
    {
        ;  /* 空语句，ADC3配置待实现 */
    }
}

/* Exported functions --------------------------------------------------------*/

/*******************************************************************************
 函数名称：lm_adc_config_channal
 功能描述：配置ADC通道参数，设置转换顺序和采样时间
 输入参数：dev - ADC设备句柄指针
           channelconfig - 通道配置参数指针
 输出参数：无
 返 回 值：无
 其它说明：该函数用于配置单个ADC通道的转换参数
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/09/26     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_adc_config_channal(adc_handle_t *dev, adc_ChannelConf * channelconfig)
{
    /* 检查输入参数有效性 */
    if(dev == NULL || channelconfig == NULL)
    {
        return;  /* 参数为空，直接返回避免空指针异常 */
    }

    ADC_ChannelConfTypeDef sConfig = {0};  /* 定义HAL库通道配置结构体并清零 */
    sConfig.Channel = channelconfig->Channel;      /* 设置ADC通道编号（如ADC_CHANNEL_0等） */
    sConfig.Rank = channelconfig->Rank;            /* 设置转换顺序（在多通道扫描中的排名） */
    sConfig.SamplingTime = channelconfig->SamplingTime;  /* 设置采样时间（影响转换精度） */
    
    /* 调用HAL库函数配置ADC通道 */
    HAL_ADC_ConfigChannel(&dev->hadc_x, &sConfig);
}

/*******************************************************************************
 函数名称：lm_adc_start_dma
 功能描述：启动ADC的DMA传输，开始数据采集
 输入参数：dev - ADC设备句柄指针
           pData - 数据存储缓冲区指针
           Length - 数据长度（转换次数）
 输出参数：无
 返 回 值：无
 其它说明：该函数启动ADC并通过DMA将转换结果传输到指定缓冲区
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/09/26     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_adc_start_dma(adc_handle_t *dev, uint32_t* pData, uint32_t Length)
{
    /* 检查输入参数有效性 */
    if(dev == NULL)
    {
        return;  /* 设备句柄为空，直接返回避免空指针异常 */
    }
    
    /* 启动ADC的DMA传输 */
    HAL_ADC_Start_DMA(&dev->hadc_x, pData, Length);  /* 启动ADC并通过DMA传输数据到指定缓冲区 */
}

/*******************************************************************************
 函数名称：lm_adc_bind_dma
 功能描述：绑定ADC和DMA，建立数据传输关联
 输入参数：无
 输出参数：无
 返 回 值：无
 其它说明：该函数用于关联ADC和DMA，当前为预留函数待实现
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/09/26     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_adc_bind_dma()
{
    /* 注释：关联ADC和DMA的示例代码，需要时取消注释并完善参数 */
    //HAL_LINKDMA(hadc, DMA_Handle, hdma_adc1);  /* 将ADC与DMA句柄关联 */
}




