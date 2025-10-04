/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_dma.c
 * 文件标识： 
 * 内容摘要： 模拟量采集驱动文件
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月01日
 *
 *******************************************************************************/  
 
/* Includes ------------------------------------------------------------------*/
#include "lm_dma.h"


/* Private constants ---------------------------------------------------------*/
#define MAX_DMA_DEVICES 3  /* 最大DMA设备数量限制，防止数组越界 */


/* Private variables ---------------------------------------------------------*/
static dma_handle_t dma_devices[MAX_DMA_DEVICES];  /* DMA设备实例数组，用于存储所有创建的DMA设备 */
static uint8_t device_count = 0;                   /* 当前已创建的DMA设备数量计数器 */
static void lm_dma_interaction_hal(dma_handle_t *dev);
/*******************************************************************************
 函数名称：lm_create_dma_device
 功能描述：创建并初始化DMA设备实例，配置DMA控制器参数  
 输入参数：userconfig - 指向DMA用户配置结构体的指针，包含DMA基本配置参数
 输出参数：无
 返 回 值：成功返回DMA设备句柄指针，失败返回NULL
 其它说明：该函数会启用对应的DMA时钟，并初始化DMA控制器
 			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/09/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
dma_handle_t* lm_create_dma_device(dma_userconfig_t *userconfig)
{
    /* 检查设备数量是否已达上限或输入参数是否为空 */
    if (device_count >= MAX_DMA_DEVICES || userconfig == NULL ) {
        return NULL;  /* 设备数量已达上限或参数无效，返回NULL表示创建失败 */
    }

    /* 获取当前可用的设备句柄指针 */
    dma_handle_t *dev = &dma_devices[device_count++];
    

    __HAL_RCC_DMA2_CLK_ENABLE();  /* 使能DMA2外设时钟 */
	 


    /* 复制用户配置参数到设备结构体 */
    dev->userconfig_t.port   = userconfig->port;                           /* 设置DMA流/通道端口 */
    dev->userconfig_t.Channel    = userconfig->Channel;                    /* 设置DMA通道选择 */
    dev->userconfig_t.Direction  = userconfig->Direction;                  /* 设置数据传输方向 */
    dev->userconfig_t.PeriphInc  = userconfig->PeriphInc;                  /* 设置外设地址递增模式 */
    dev->userconfig_t.MemInc     = userconfig->MemInc;                     /* 设置存储器地址递增模式 */
    dev->userconfig_t.PeriphDataAlignment = userconfig->PeriphDataAlignment;  /* 设置外设数据对齐方式 */
    dev->userconfig_t.MemDataAlignment = userconfig->MemDataAlignment;     /* 设置存储器数据对齐方式 */
    dev->userconfig_t.Priority  = userconfig->Priority;                    /* 设置DMA通道优先级 */

    /* 设置调试配置参数（固定值） */
    dev->debugconfig_t.FIFOMode = DMA_FIFOMODE_DISABLE;                    /* 禁用FIFO模式，使用直接模式 */
    dev->debugconfig_t.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;            /* 设置FIFO阈值（虽然FIFO已禁用） */
    dev->debugconfig_t.MemBurst  = DMA_MBURST_SINGLE;                      /* 设置存储器突发传输为单次传输 */
    dev->debugconfig_t.PeriphBurst = DMA_PBURST_SINGLE;                    /* 设置外设突发传输为单次传输 */
    
    /* 调用HAL库交互函数完成DMA初始化 */
    lm_dma_interaction_hal(dev);
    
    return dev;  /* 返回初始化成功的DMA设备句柄 */
}

/*******************************************************************************
 函数名称：lm_dma_irqconfig
 功能描述：配置DMA中断优先级和使能中断  
 输入参数：handle_t - 指向DMA设备句柄的指针
           PreemptPriority - 中断抢占优先级
           SubPriority - 中断子优先级
 输出参数：无
 返 回 值：无
 其它说明：当前函数体为空，需要根据具体DMA流实现中断配置
 			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/09/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_dma_irqconfig(dma_handle_t *handle_t, uint32_t PreemptPriority, uint32_t SubPriority)
{
    /* 检查输入参数有效性：DMA设备句柄指针不能为空 */
    if(handle_t == NULL)
    {
        return;  /* 句柄为空，直接返回避免空指针异常 */
    }

    /* 注释：以下是中断配置的示例代码，需要根据具体DMA流进行实现 */
    // __HAL_DMA_ENABLE_IT(&hdma_adc1, DMA_IT_TC);                    /* 使能DMA传输完成中断 */
    // HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);                 /* 设置DMA中断优先级 */
    // HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);                         /* 使能DMA中断向量 */
}

/*******************************************************************************
 函数名称：lm_dma_interaction_hal
 功能描述：DMA与HAL库交互函数，将用户配置转换为HAL库配置并初始化DMA  
 输入参数：dev - 指向DMA设备句柄的指针
 输出参数：无
 返 回 值：无
 其它说明：该函数将用户配置参数映射到HAL DMA初始化结构体，并调用HAL初始化函数
 			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/09/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static void lm_dma_interaction_hal(dma_handle_t *dev)
{
    /* 将用户配置参数映射到HAL DMA初始化结构体 */
    dev->hdma_x.Instance             = dev->userconfig_t.port;                  /* 设置DMA实例（流/通道） */
    dev->hdma_x.Init.Channel         = dev->userconfig_t.Channel;          /* 设置DMA通道选择 */
    dev->hdma_x.Init.Direction       = dev->userconfig_t.Direction;        /* 设置数据传输方向 */
    dev->hdma_x.Init.PeriphInc       = dev->userconfig_t.PeriphInc;        /* 设置外设地址递增模式 */
    dev->hdma_x.Init.MemInc          = dev->userconfig_t.MemInc;           /* 设置存储器地址递增模式 */
    dev->hdma_x.Init.PeriphDataAlignment = dev->userconfig_t.PeriphDataAlignment;  /* 设置外设数据对齐方式 */
    dev->hdma_x.Init.MemDataAlignment = dev->userconfig_t.MemDataAlignment;  /* 设置存储器数据对齐方式 */
    dev->hdma_x.Init.Priority  = dev->userconfig_t.Priority;               /* 设置DMA通道优先级 */

    /* 设置调试相关配置参数 */
    dev->hdma_x.Init.FIFOMode = dev->debugconfig_t.FIFOMode;               /* 设置FIFO模式 */
    dev->hdma_x.Init.FIFOThreshold = dev->debugconfig_t.FIFOThreshold;     /* 设置FIFO阈值 */
    dev->hdma_x.Init.MemBurst  = dev->debugconfig_t.MemBurst;              /* 设置存储器突发传输模式 */
    dev->hdma_x.Init.PeriphBurst = dev->debugconfig_t.PeriphBurst;         /* 设置外设突发传输模式 */

    /* 调用HAL库DMA初始化函数，检查初始化结果 */
    if (HAL_DMA_Init(&dev->hdma_x) != HAL_OK)
    {
//        Error_Handler();  /* DMA初始化失败，调用错误处理函数 */
    }
}


