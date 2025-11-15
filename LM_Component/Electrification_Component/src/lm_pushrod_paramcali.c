/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_pushrod_paramcali.c
 * 文件标识： 
 * 内容摘要： 电推杆控制
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年10月05日
 *
 *******************************************************************************/



/* Includes ------------------------------------------------------------------*/
#include "lm_pushrod_paramcali.h"
#include <stdbool.h>

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/**
  * @brief ADC1外设句柄
  * @details 用于配置和管理ADC1模块的HAL库句柄
  */
ADC_HandleTypeDef hadc1;

/**
  * @brief ADC1 DMA传输句柄
  * @details 用于配置和管理ADC1与DMA1通道1的数据传输
  */
DMA_HandleTypeDef hdma_adc1;

/**
  * @brief ADC转换组数据缓冲区
  * @details 用于DMA传输的ADC转换数据缓冲区，大小为20个采样点
  */
uint16_t ADCConverGroup[20][2];
/**
  * @brief DMA传输完成标志
  * @details 用于指示DMA传输是否完成的标志位，1表示传输完成
  */
volatile bool dma_transfer_complete = 0;

uint16_t ADCChangeValue[2];

motor_param_t  pushrod_param;

float  Average_Filtering[2];
float  Bartworth_Filtering[2];

/* Private constants ---------------------------------------------------------*/

/**
  * @brief ADC通道配置数组
  * @details 配置ADC的2个转换通道参数，包括通道号、转换顺序和采样时间
  */
static const ADC_ChannelConfig adcChannelConfig[2] = {
    [0] = {
        .ADC_Channel = ADC_CHANNEL_6,        /* 使用ADC通道6，对应PA6引脚 */
        .Rank = 1,                           /* 转换顺序为第1个 */
        .SampleTime = ADC_SAMPLETIME_1CYCLE_5  /* 采样时间为1.5个ADC时钟周期 */
    },
    [1] = {
        .ADC_Channel = ADC_CHANNEL_7,        /* 使用ADC通道7，对应PA7引脚 */
        .Rank = 2,                           /* 转换顺序为第2个 */
        .SampleTime = ADC_SAMPLETIME_13CYCLES_5, /* 采样时间为13.5个ADC时钟周期 */
    },
};

/* Private macros ------------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
 函数名称：lm_param_gpio_init
 功能描述：初始化ADC相关GPIO引脚为模拟输入模式    
 输入参数：无   
 输出参数：无    
 返 回 值：无    
 其它说明：此函数配置PA6和PA7引脚为模拟输入模式
           用于ADC通道6和通道7的电压信号采集
           模拟输入模式可避免数字信号干扰ADC采样精度			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/10/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static void lm_param_gpio_init(void)
{   
    GPIO_InitTypeDef GPIO_InitStruct = {0};  /* GPIO初始化结构体，初始化为0 */
    
    /* 使能GPIOA端口时钟，PA6和PA7引脚属于GPIOA */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    /* 配置GPIO引脚参数：同时配置PA6和PA7两个引脚 */
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;  /* 选择PA6和PA7引脚 */
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;        /* 设置为模拟输入模式 */
    
    /* 初始化GPIO引脚，应用上述配置参数 */
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/*******************************************************************************
 函数名称：lm_param_adc_config
 功能描述：配置ADC1模块的完整参数并启动DMA传输    
 输入参数：无   
 输出参数：无    
 返 回 值：无    
 其它说明：此函数完成ADC1的完整初始化流程，包括GPIO初始化、DMA初始化
           ADC基本参数配置、通道配置、校准和启动DMA传输
           使用定时器4通道4作为外部触发源，扫描模式转换2个通道			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/10/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_param_adc_config(void)
{
    /* 初始化ADC相关GPIO引脚为模拟输入模式 */
    lm_param_gpio_init();
    
    /* 初始化DMA控制器，配置ADC数据传输 */
    lm_param_dma_init();
    
    /* 配置ADC时钟分频为6分频，确定ADC模块的工作时钟 */
    __HAL_RCC_ADC_CONFIG(RCC_ADCPCLK2_DIV6);
    
    ADC_ChannelConfTypeDef sConfig = {0};  /* ADC通道配置结构体，初始化为0 */
        
    /* 使能ADC1外设时钟 */
    __HAL_RCC_ADC1_CLK_ENABLE();
    
    /* 配置ADC1基本工作参数 */
    hadc1.Instance = ADC1;                                   /* 选择ADC1外设 */
    hadc1.Init.ScanConvMode = ENABLE;                        /* 使能扫描模式，转换多个通道 */
    hadc1.Init.ContinuousConvMode = DISABLE;                 /* 禁用连续转换模式，单次转换 */
    hadc1.Init.DiscontinuousConvMode = DISABLE;              /* 禁用间断模式 */
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T4_CC4; /* 使用定时器4通道4作为外部触发源 */
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;              /* 数据右对齐 */
    hadc1.Init.NbrOfConversion = 2;		                     /* 设置转换通道数量为2个 */
		
    /* 初始化ADC1外设，应用基本参数配置 */
    HAL_ADC_Init(&hadc1);

    /* 配置ADC通道1参数：通道6，转换顺序1，采样时间1.5周期 */
    sConfig.Channel = adcChannelConfig[0].ADC_Channel;    /* 选择ADC通道6 */
    sConfig.Rank = adcChannelConfig[0].Rank;              /* 设置转换顺序为第1个 */
    sConfig.SamplingTime = adcChannelConfig[0].SampleTime; /* 设置采样时间为1.5周期 */

    /* 配置ADC通道1，应用通道参数 */
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    /* 配置ADC通道2参数：通道7，转换顺序2，采样时间13.5周期 */
    sConfig.Channel = adcChannelConfig[1].ADC_Channel;    /* 选择ADC通道7 */
    sConfig.Rank = adcChannelConfig[1].Rank;              /* 设置转换顺序为第2个 */
    sConfig.SamplingTime = adcChannelConfig[1].SampleTime; /* 设置采样时间为13.5周期 */
    
    /* 配置ADC通道2，应用通道参数 */
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    /* 执行ADC校准，提高转换精度 */
    HAL_ADCEx_Calibration_Start(&hadc1);

    /* 启动ADC DMA传输，将转换结果存储到指定缓冲区 */
    HAL_ADC_Start_DMA(&hadc1, 
                         (uint32_t*)ADCConverGroup,           /* 目标缓冲区地址 */
                         sizeof(ADCConverGroup)/sizeof(ADCConverGroup[0][0]));  /* 传输数据长度 */
}

/*******************************************************************************
 函数名称：lm_param_dma_init
 功能描述：初始化DMA控制器，配置ADC数据传输通道    
 输入参数：无   
 输出参数：无    
 返 回 值：无    
 其它说明：此函数配置DMA1通道1用于ADC数据传输
           设置数据传输方向、地址递增、数据对齐、循环模式等参数
           启用传输完成中断并配置NVIC中断优先级			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/10/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_param_dma_init(void)
{
    /* 使能DMA1控制器时钟 */
    __HAL_RCC_DMA1_CLK_ENABLE();
    
    /* 配置DMA1通道1的基本参数 */
    hdma_adc1.Instance = DMA1_Channel1;                     /* 选择DMA1通道1 */
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;        /* 传输方向：外设到内存 */
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;            /* 外设地址不递增 */
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;                /* 内存地址递增 */
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;  /* 外设数据半字对齐 */
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;     /* 内存数据半字对齐 */
    hdma_adc1.Init.Mode = DMA_CIRCULAR;                     /* 循环模式，持续传输 */
    hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;            /* 高优先级 */
    
    /* 初始化DMA通道，应用配置参数 */
    HAL_DMA_Init(&hdma_adc1);
             
    /* 使能DMA传输完成中断 */
    __HAL_DMA_ENABLE_IT(&hdma_adc1, DMA_IT_TC);
    
    /* 将DMA句柄与ADC句柄关联，建立数据传输链接 */
    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);
    
    /* 配置DMA中断优先级：抢占优先级9，子优先级0 */
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 9, 0);
    
    /* 使能DMA1通道1中断 */
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

/*******************************************************************************
 函数名称：DMA1_Channel1_IRQHandler
 功能描述：DMA1通道1中断服务函数    
 输入参数：无   
 输出参数：无    
 返 回 值：无    
 其它说明：此函数处理DMA1通道1的中断请求
           调用HAL库的DMA中断处理函数来自动处理中断标志和回调函数
           当中断发生时，系统自动调用此函数			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/10/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void DMA1_Channel1_IRQHandler(void)
{
    /* 调用HAL库的DMA中断处理函数，自动清除中断标志并调用回调函数 */
    HAL_DMA_IRQHandler(&hdma_adc1);
}

/*******************************************************************************
 函数名称：HAL_ADC_ConvCpltCallback
 功能描述：ADC转换完成回调函数    
 输入参数：hadc - 触发回调的ADC句柄指针   
 输出参数：无    
 返 回 值：无    
 其它说明：此函数在ADC转换完成并通过DMA传输数据后由HAL库自动调用
           检查DMA通道并设置传输完成标志，供主程序查询使用			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/10/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    /* 检查触发回调的ADC是否使用DMA1通道1进行数据传输 */
    if (hadc->DMA_Handle->Instance == DMA1_Channel1 && dma_transfer_complete == false)
    {
        /* 设置DMA传输完成标志，通知主程序可以处理ADC数据 */
			  dma_transfer_complete = true;
    }
}


void lm_param_deal(void)
{   
	  if(dma_transfer_complete == false)
		{
			return;
		}
		
	  lm_adc_filter();
	  float rawAdcValue =ADCChangeValue[1];
		float voltage = rawAdcValue * 3.3f / 4096.0f;
		pushrod_param.Current_Bus = (voltage / 10) / 0.01;
	
		float batteryRawValue = ADCChangeValue[0];
		float batteryVoltage = batteryRawValue * 3.3f / 4095;
		pushrod_param.Voltage_Power= batteryVoltage * 52.648;

		dma_transfer_complete = false;
}	

void lm_adc_filter(void)
{
    for (int ch = 0; ch < 2; ch++) {
        uint32_t sum = 0;
        for (int i = 0; i < 10; i++) {
            sum += ADCConverGroup[i][ch];
        }
				ADCChangeValue[ch] = sum/10;
    }
}



