/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_485.c
 * 文件标识： 
 * 内容摘要： 模拟量采集驱动文件
 * 其它说明： 无
 * 当前版本： RS485 V1.0.0
 * 作    者：    Qiguo_Cui                   
 * 完成日期： 2025年09月01日
 *
 * 主应用层  发送请求
 *     ↓
 * RS485_SendReadRequest()          // 发送读取请求
 *     ├── BuildReadRequestFrame()   // 构建请求帧
 *     ├── RS485_TX_Set(1)          // 设置为发送模式  
 *     ├── HAL_UART_Transmit()      // 发送数据
 *     └── RS485_TX_Set(0)          // 切换为接收模式
 * 
 * 中断处理
 *     ↓
 * USART2_IRQHandler()              // 中断入口
 *     └── RS485_IdleHandler()      // 空闲中断处理
 *         ├── HAL_UART_DMAStop()   // 停止DMA
 *         ├── 计算接收数据长度
 *         └── HAL_UART_Receive_DMA() // 重启DMA接收
 * 
 * 数据处理
 *     ↓
 * RS485_IsResponseReady()          // 检查数据就绪
 *     ↓
 * RS485_ParseResponse()            // 解析响应
 *     └── VerifyFrameCRC()         // CRC校验
 *         └── lm_crc16_mpdbus()       // CRC计算
 *     ↓
 * RS485_ClearResponseFlag()        // 清除标志
 *******************************************************************************/


/* Includes ------------------------------------------------------------------*/
#include "lm_485.h"
#include "pcf8574.h"
#include "delay.h"
#include "lm_16crc.h"
#include <string.h>


/* Private variables ---------------------------------------------------------*/
/* USART2的HAL库句柄，用于配置和控制USART2串口通信，专门用于RS485通信 */
UART_HandleTypeDef USART2_RS485Handler;  // USART2句柄(用于RS485)

/* DMA控制器句柄，用于配置DMA数据传输，实现USART2接收数据的DMA传输 */
DMA_HandleTypeDef  UART2ReviecDMA_Handler;  // 用于DMA接收数据

/* 数据解析缓冲区，存储待解析的接收数据，volatile确保编译器不优化此变量 */
volatile uint8_t rx_analysis_buffer[RX_BUFFER_SIZE];

/* 原始数据接收缓冲区，DMA直接写入的原始接收数据存储区 */
uint8_t rx_buffer[RX_BUFFER_SIZE];

/* 数据发送缓冲区，存储待通过USART2发送的数据，static限制作用域在当前文件 */
static uint8_t tx_buffer[TX_BUFFER_SIZE];

/* 接收数据长度计数器，记录当前接收到的数据字节数 */
volatile uint16_t rx_data_length = 0;

/* 帧处理标志位，标识是否收到完整数据帧需要处理（通常置1表示有待处理数据） */
static volatile uint8_t frame_deal = 0;

/* 接收超时标志位，用于检测数据接收是否超时（如：在一段时间内没有收到新数据） */
static volatile uint8_t reception_timeout = 0;


LM_RS485_Responsestatus_t response;
LM_RS485_Status_t status;
Battery_Param_t   battery_data;
uint32_t last_request_time = 0;
/**********************01_初始化函数******************************************
void RS485_Init(u32 bound);
void lm_gpio_init(void);
void lm_rs485_chipselec(void);
void lm_rs485_chipSetLevel(GPIO_PinState state);
******************************************************************************/

/*******************************************************************************
 * 函数名称：RS485_Init
 * 功能描述：初始化RS485通信模块
 *           配置USART2和DMA用于RS485通信，包括GPIO、UART、DMA和中断
 * 输入参数：
 *   - bound: 通信波特率，单位bps
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 初始化PCF8574用于RE端控制
 *   - 配置USART2为8位数据、1位停止位、无校验位
 *   - 配置DMA1_Stream5通道4用于UART接收
 *   - 使能空闲中断和错误中断
 *   - 初始设置为发送模式
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_rs485_init(u32 bound)
{
    GPIO_InitTypeDef GPIO_Initure;  /* GPIO初始化结构体 */
    
    /* 初始化PCF8574，用于控制RS485的RE端 */
    PCF8574_Init();                         
    
    /* 使能GPIOA和USART2时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();          
    __HAL_RCC_USART2_CLK_ENABLE();         
    __HAL_RCC_DMA1_CLK_ENABLE();   
	
    /* 配置USART2引脚(PA2, PA3) */
    GPIO_Initure.Pin = GPIO_PIN_2 | GPIO_PIN_3; /* 选择TX和RX引脚 */
    GPIO_Initure.Mode = GPIO_MODE_AF_PP;        /* 复用推挽输出模式 */
    GPIO_Initure.Pull = GPIO_PULLUP;            /* 上拉电阻 */
    GPIO_Initure.Speed = GPIO_SPEED_HIGH;       /* 高速模式 */
    GPIO_Initure.Alternate = GPIO_AF7_USART2;   /* 复用为USART2 */
    HAL_GPIO_Init(GPIOA, &GPIO_Initure);        /* 应用配置 */
    
    /* 配置USART2参数 */
    USART2_RS485Handler.Instance = USART2;                      /* 选择USART2 */
    USART2_RS485Handler.Init.BaudRate = bound;                  /* 设置波特率 */
    USART2_RS485Handler.Init.WordLength = UART_WORDLENGTH_8B;   /* 8位数据位 */
    USART2_RS485Handler.Init.StopBits = UART_STOPBITS_1;        /* 1位停止位 */
    USART2_RS485Handler.Init.Parity = UART_PARITY_NONE;         /* 无校验位 */
    USART2_RS485Handler.Init.HwFlowCtl = UART_HWCONTROL_NONE;   /* 无硬件流控 */
    USART2_RS485Handler.Init.Mode = UART_MODE_TX_RX;            /* 收发模式 */
    HAL_UART_Init(&USART2_RS485Handler);                        /* 初始化UART */
    
    /* 禁用发送完成中断 */
    __HAL_UART_DISABLE_IT(&USART2_RS485Handler, UART_IT_TC);
    
    /* 使能DMA1时钟 */

    
    /* 配置DMA接收参数 */
    UART2ReviecDMA_Handler.Instance = DMA1_Stream5;                    /* 选择DMA1数据流5 */
    UART2ReviecDMA_Handler.Init.Channel = DMA_CHANNEL_4;               /* 选择通道4 */
    UART2ReviecDMA_Handler.Init.Direction = DMA_PERIPH_TO_MEMORY;      /* 外设到存储器 */
    UART2ReviecDMA_Handler.Init.PeriphInc = DMA_PINC_DISABLE;          /* 外设地址固定 */
    UART2ReviecDMA_Handler.Init.MemInc = DMA_MINC_ENABLE;              /* 存储器地址递增 */
    UART2ReviecDMA_Handler.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;  /* 外设字节对齐 */
    UART2ReviecDMA_Handler.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;     /* 存储器字节对齐 */
    UART2ReviecDMA_Handler.Init.Mode = DMA_CIRCULAR;                   /* 循环模式 */
    UART2ReviecDMA_Handler.Init.Priority = DMA_PRIORITY_MEDIUM;        /* 中等优先级 */
    UART2ReviecDMA_Handler.Init.FIFOMode = DMA_FIFOMODE_DISABLE;       /* 禁用FIFO */
    UART2ReviecDMA_Handler.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL; /* FIFO阈值 */
    UART2ReviecDMA_Handler.Init.MemBurst = DMA_MBURST_SINGLE;          /* 存储器单次传输 */
    UART2ReviecDMA_Handler.Init.PeriphBurst = DMA_PBURST_SINGLE;       /* 外设单次传输 */
    
    /* 重新初始化DMA */
    HAL_DMA_DeInit(&UART2ReviecDMA_Handler);   
    HAL_DMA_Init(&UART2ReviecDMA_Handler);
    
    /* 关联DMA到UART接收 */
    __HAL_LINKDMA(&USART2_RS485Handler, hdmarx, UART2ReviecDMA_Handler);
    
    /* 使能UART空闲中断和错误中断 */
    __HAL_UART_ENABLE_IT(&USART2_RS485Handler, UART_IT_IDLE);	
    __HAL_UART_ENABLE_IT(&USART2_RS485Handler, UART_IT_ERR);
    
    /* 配置和使能USART2中断 */
    HAL_NVIC_EnableIRQ(USART2_IRQn);                    /* 使能USART2中断 */
    HAL_NVIC_SetPriority(USART2_IRQn, 3, 3);            /* 设置中断优先级 */

    /* 启动DMA接收 */
    HAL_UART_Receive_DMA(&USART2_RS485Handler, rx_buffer, RX_BUFFER_SIZE);
    
    /* 初始设置为发送模式 */
    lm_rs485_tx_set(1);                                    
}

/*******************************************************************************
 * 函数名称：lm_gpio_init
 * 功能描述：初始化LM模块的GPIO引脚配置
 *           配置USART2对应的GPIO引脚为复用功能，用于RS485通信
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 初始化PCF8574用于控制RS485的RE使能端
 *   - 配置PA2和PA3为USART2的TX和RX引脚
 *   - 设置引脚为复用推挽输出模式，上拉，高速
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_rs485_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_Initure;  /* GPIO初始化结构体 */

    /* 初始化PCF8574，用于控制RS485的RE端 */
    PCF8574_Init();                         
    
    /* 使能GPIOA时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();           
    
    /* 配置PA2(TX)和PA3(RX)引脚 */
    GPIO_Initure.Pin = GPIO_PIN_2 | GPIO_PIN_3; /* 选择PA2和PA3引脚 */
    GPIO_Initure.Mode = GPIO_MODE_AF_PP;        /* 设置为复用推挽输出模式 */
    GPIO_Initure.Pull = GPIO_PULLUP;            /* 使能上拉电阻 */
    GPIO_Initure.Speed = GPIO_SPEED_HIGH;       /* 设置为高速模式 */
    GPIO_Initure.Alternate = GPIO_AF7_USART2;   /* 复用为USART2功能 */
    
    /* 应用GPIO配置 */
    HAL_GPIO_Init(GPIOA, &GPIO_Initure);        
}

/*******************************************************************************
 * 函数名称：lm_rs485_chipselec
 * 功能描述：初始化RS485芯片选择引脚
 *           配置PB9引脚为输出模式，用于RS485芯片的片选控制
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 配置PB9为推挽输出模式，无上下拉，低速输出
 *   - 该引脚用于控制RS485收发器的使能或片选
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_rs485_chipselec(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};  /* GPIO初始化结构体 */
    
    /* 使能GPIOB时钟 */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    /* 配置PB9引脚参数 */
    GPIO_InitStruct.Pin = GPIO_PIN_9;                /* 选择PB9引脚 */
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;      /* 推挽输出模式 */
    GPIO_InitStruct.Pull = GPIO_NOPULL;              /* 无上下拉电阻 */
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;     /* 低速输出 */
    
    /* 应用GPIO配置 */
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/*******************************************************************************
 * 函数名称：lm_rs485_chipSetLevel
 * 功能描述：设置RS485芯片选择引脚的电平状态
 *           控制PB9引脚的输出电平，用于RS485芯片的使能控制
 * 输入参数：
 *   - state: 引脚电平状态
 *     GPIO_PIN_SET   - 设置引脚为高电平
 *     GPIO_PIN_RESET - 设置引脚为低电平
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 通过控制PB9引脚电平来使能或禁用RS485收发器
 *   - 通常用于多RS485设备切换或电源管理
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_rs485_chipSetLevel(GPIO_PinState state)
{
    /* 设置PB9引脚的电平状态 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, state);
}


/**********************02_中断函数******************************************
void USART2_IRQHandler(void);
void RS485_IdleHandler(UART_HandleTypeDef * type);
******************************************************************************/

/*******************************************************************************
 * 函数名称：RS485_IdleHandler
 * 功能描述：处理RS485通信中的UART空闲中断，用于帧接收完成检测
 *           当检测到UART空闲中断时，停止DMA接收并计算接收数据长度
 *           如果接收到的数据长度符合要求且未在处理中，则设置帧处理标志
 * 输入参数：
 *   - type: UART句柄指针，指向触发空闲中断的UART实例
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数在UART空闲中断中调用，用于检测一帧数据的接收完成
 *   - 通过DMA计数器计算实际接收数据长度
 *   - 设置frame_deal标志通知主程序有新数据需要处理
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_rs485_idlehandler(UART_HandleTypeDef * type)
{
    /* 检查是否USART2触发的空闲中断 */
    if (type->Instance == USART2_RS485Handler.Instance)
    {
        /* 清除空闲中断标志位，防止重复进入中断 */
        __HAL_UART_CLEAR_IDLEFLAG(type);
                
        /* 停止DMA接收，防止数据被覆盖 */
        HAL_UART_DMAStop(type);
        
        /* 计算实际接收到的数据长度：缓冲区大小减去DMA剩余计数器值 */
        rx_data_length = RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(type->hdmarx);
        
        /* 检查数据长度是否符合预期且当前没有在处理帧数据 */
        if(rx_data_length == 31 && frame_deal == 0)
        {
            /* 设置帧处理标志，通知主程序有新数据需要解析 */
            frame_deal = 1;
					  
        }
        
        /* 重新启动DMA接收，准备接收下一帧数据 */
        HAL_UART_Receive_DMA(type, rx_buffer, RX_BUFFER_SIZE);
    }
}

/*******************************************************************************
 * 函数名称：USART2_IRQHandler
 * 功能描述：USART2中断服务函数，主要处理空闲中断
 *           检测UART空闲中断标志并调用相应的处理函数
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数为USART2的中断服务例程，由硬件中断触发
 *   - 主要处理UART空闲中断，用于检测数据帧接收完成
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void USART2_IRQHandler(void)
{
    /* 检查是否发生UART空闲中断 */
    if (__HAL_UART_GET_FLAG(&USART2_RS485Handler, UART_FLAG_IDLE) != RESET)
    {
        /* 调用空闲中断处理函数进行数据帧处理 */
        lm_rs485_idlehandler(&USART2_RS485Handler);
    }
}

/**********************03_数据收发函数******************************************
uint8_t RS485_IsResponseReady(void);
static uint16_t BuildReadRequestFrame(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count);
static RS485_Status VerifyFrameCRC(uint8_t *data, uint16_t length);

******************************************************************************/


/*******************************************************************************
 * 函数名称：RS485_IsResponseReady
 * 功能描述：检查RS485响应数据是否准备就绪
 *           返回帧处理标志状态，指示是否有新接收的数据需要处理
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：
 *   - uint8_t: 响应准备状态
 *     1 - 有新的响应数据需要处理
 *     0 - 没有新的响应数据
 * 其它说明：
 *   - 此函数用于主程序轮询检查是否有新数据到达
 *   - 当frame_deal为1时表示有完整帧需要解析
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
uint8_t lm_rs485_isresponseready(void)
{
    /* 返回帧处理标志状态 */
    return frame_deal;
}

/*******************************************************************************
 * 函数名称：BuildReadRequestFrame
 * 功能描述：构建Modbus RTU读寄存器请求帧
 *           按照Modbus RTU协议格式构建读取保持寄存器的请求帧
 * 输入参数：
 *   - slave_addr: 从站地址，指定要通信的Modbus从站设备地址
 *   - start_reg:  起始寄存器地址，指定要读取的起始寄存器地址
 *   - reg_count:  寄存器数量，指定要连续读取的寄存器数量
 * 输出参数：无
 * 返 回 值：
 *   - uint16_t: 构建的请求帧总长度（字节数）
 * 其它说明：
 *   - 帧格式：地址(1) + 功能码(1) + 起始地址(2) + 寄存器数量(2) + CRC(2)
 *   - 功能码固定为0x03（读保持寄存器）
 *   - CRC校验码采用Modbus CRC16算法
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static uint16_t lm_rs485_buildreadrequestframe(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count)
{
    /* 构建Modbus RTU请求帧 */
    tx_buffer[0] = slave_addr;                              /* 从站地址 */
    tx_buffer[1] = RS485_READ_FUNC_CODE;                    /* 功能码：读保持寄存器 */
    tx_buffer[2] = (start_reg >> 8) & 0xFF;                 /* 起始地址高字节 */
    tx_buffer[3] = start_reg & 0xFF;                        /* 起始地址低字节 */
    tx_buffer[4] = (reg_count >> 8) & 0xFF;                 /* 寄存器数量高字节 */
    tx_buffer[5] = reg_count & 0xFF;                        /* 寄存器数量低字节 */
    
    /* 添加CRC校验码（此处为硬编码示例，实际应计算） */
    tx_buffer[6] = 0x84;                                    /* CRC低字节 */
    tx_buffer[7] = 0x96;                                    /* CRC高字节 */
    
    return 8; /* 返回帧总长度 */
}

/*******************************************************************************
 * 函数名称：RS485_SendReadRequest
 * 功能描述：发送Modbus RTU读寄存器请求
 *           构建并发送读取保持寄存器的请求帧，等待发送完成
 * 输入参数：
 *   - slave_addr: 从站地址，目标Modbus设备地址
 *   - start_reg:  起始寄存器地址，要读取的寄存器起始地址
 *   - reg_count:  寄存器数量，要连续读取的寄存器数量
 * 输出参数：无
 * 返 回 值：
 *   - RS485_Status: 发送操作状态
 *     RS485_OK      - 发送成功
 *     RS485_ERR_XXX - 发送失败的具体错误代码
 * 其它说明：
 *   - 先切换到发送模式，发送完成后切换回接收模式
 *   - 使用阻塞方式发送，等待发送完成
 *   - 重置接收状态准备接收响应
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
LM_RS485_Status_t lm_rs485_sendreadrequest(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count)
{
    /* 构建Modbus读寄存器请求帧 */
    uint16_t frame_length = lm_rs485_buildreadrequestframe(slave_addr, start_reg, reg_count);
    
    /* 设置为发送模式，使能RS485驱动器 */
    lm_rs485_tx_set(1);
    
    /* 使用HAL库发送请求帧（阻塞方式） */
    HAL_StatusTypeDef hal_status = HAL_UART_Transmit(&USART2_RS485Handler, tx_buffer, frame_length, RS485_TX_TIMECIRC);
    
    /* 等待最后一个字节发送完成 */
    while (__HAL_UART_GET_FLAG(&USART2_RS485Handler, UART_FLAG_TC) == RESET) {
        /* 等待发送完成标志置位 */
    }
    
    /* 切换回接收模式，禁用RS485驱动器 */
    lm_rs485_tx_set(0);
    
    /* 返回操作状态 */
    return RS485_OK;
}

/**********************04_数据解释函数及其他******************************************
RS485_Status RS485_ParseResponse(RS485_Response *response);
static RS485_Status VerifyFrameCRC(uint8_t *data, uint16_t length);
RS485_Status RS485_SendReadRequest(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count);
void RS485_ClearResponseFlag(void);
******************************************************************************/

/*******************************************************************************
 * 函数名称：RS485_ParseResponse
 * 功能描述：解析Modbus RTU响应帧
 *           对接收到的Modbus响应帧进行完整解析和验证
 * 输入参数：
 *   - response: 响应结构体指针，用于存储解析后的响应数据
 * 输出参数：
 *   - response: 解析后的响应数据填充到该结构体中
 * 返 回 值：
 *   - RS485_Status: 解析操作状态
 *     RS485_OK                    - 解析成功
 *     RS485_ERR_FRAME_TOO_SHORT   - 帧长度过短
 *     RS485_ERR_ADDR_MISMATCH     - 地址不匹配
 *     RS485_ERR_FUNC_MISMATCH     - 功能码不匹配
 *     RS485_ERR_INVALID_LENGTH    - 数据长度无效
 *     RS485_ERR_CRC_MISMATCH      - CRC校验失败
 * 其它说明：
 *   - 进行完整的Modbus RTU帧验证：地址、功能码、长度、CRC
 *   - 解析成功后将响应数据填充到response结构体
 *   - 支持读保持寄存器功能码(0x03)的响应解析
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
LM_RS485_Status_t lm_rs485_parseresponse(LM_RS485_Responsestatus_t *response)
{
    /* 检查响应结构体指针有效性 */
    if (response == NULL) {
        return RS485_ERR_INVALID_LENGTH;
    }
    
    /* 基本帧长度检查（最小响应帧长度为5字节） */
    if (rx_data_length < 5) {
        response->status = RS485_ERR_FRAME_TOO_SHORT;
        return RS485_ERR_FRAME_TOO_SHORT;
    }
    
    /* 检查从站地址是否匹配 */
    response->slave_addr = rx_buffer[0];
    if (response->slave_addr != RS485_ADDRESS) {
        response->status = RS485_ERR_ADDR_MISMATCH;
        return RS485_ERR_ADDR_MISMATCH;
    }
    
    /* 检查功能码是否正确 */
    response->func_code = rx_buffer[1];
    if (response->func_code != RS485_READ_FUNC_CODE) {
        response->status = RS485_ERR_FUNC_MISMATCH;
        return RS485_ERR_FUNC_MISMATCH;
    }
    
    /* 检查数据字节数是否有效 */
    response->byte_count = rx_buffer[2];
    if (response->byte_count == 0 || response->byte_count > (rx_data_length - 5)) {
        response->status = RS485_ERR_INVALID_LENGTH;
        return RS485_ERR_INVALID_LENGTH;
    }
    
    /* 验证CRC校验码 */
    LM_RS485_Status_t crc_status = lm_rs485_verifyframecrc(rx_buffer, rx_data_length);
    if (crc_status != RS485_OK) {
        response->status = crc_status;
        return crc_status;
    }
    
    /* 提取响应数据 */
    response->data = &rx_buffer[3];                         /* 数据起始位置（跳过地址、功能码、字节数） */
    response->data_length = response->byte_count;           /* 数据长度 */
    response->crc = (rx_buffer[rx_data_length-1] << 8) | rx_buffer[rx_data_length-2];  /* CRC校验码 */
    response->status = RS485_OK;                            /* 设置状态为成功 */
    lm_rs485_DataPara(response->data,response->data_length);
    /* 返回解析成功状态 */
    return RS485_OK;
}

/*******************************************************************************
 * 函数名称：VerifyFrameCRC
 * 功能描述：验证Modbus RTU帧的CRC校验码
 *           计算接收数据的CRC值并与帧中的CRC字段进行比较
 * 输入参数：
 *   - data:   待验证的数据帧指针
 *   - length: 数据帧总长度（包括CRC字段）
 * 输出参数：无
 * 返 回 值：
 *   - RS485_Status: CRC验证结果
 *     RS485_OK               - CRC验证通过
 *     RS485_ERR_CRC_MISMATCH - CRC校验失败
 *     RS485_ERR_FRAME_TOO_SHORT - 帧长度过短
 * 其它说明：
 *   - 使用Modbus CRC16算法进行计算
 *   - 比较计算出的CRC与帧中最后2字节的CRC值
 *   - 帧格式要求长度至少为2字节（CRC字段）
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static LM_RS485_Status_t lm_rs485_verifyframecrc(uint8_t *data, uint16_t length)
{
    /* 检查帧长度是否足够包含CRC字段 */
    if (length < 2) {
        return RS485_ERR_FRAME_TOO_SHORT;
    }
    
    /* 提取接收到的CRC校验码（小端格式） */
    uint16_t received_crc = (data[length-1] << 8) | data[length-2];
    
    /* 计算数据的CRC校验码（排除最后2字节的CRC字段） */
    uint16_t calculated_crc = lm_crc16_modbus(data, length - 2);
    
    /* 比较计算CRC和接收CRC */
    if (received_crc != calculated_crc) {
        return RS485_ERR_CRC_MISMATCH;  /* CRC校验失败 */
    }
    
    return RS485_OK;  /* CRC验证通过 */
}

/*******************************************************************************
 * 函数名称：RS485_ClearResponseFlag
 * 功能描述：清除RS485响应标志和接收缓冲区
 *           重置接收状态，准备接收新的数据帧
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 清除接收数据长度计数器
 *   - 清空接收缓冲区
 *   - 重置帧处理标志
 *   - 通常在处理完一帧数据后调用
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_rs485_clearresponseflag(void)
{
    /* 重置接收数据长度 */
    rx_data_length = 0;
    
    /* 清空接收缓冲区 */
    memset(rx_buffer, 0, RX_BUFFER_SIZE);
    
    /* 重置帧处理标志 */
    frame_deal = 0;
}

/*******************************************************************************
 * 函数名称：RS485_TX_Set
 * 功能描述：设置RS485收发器的工作模式
 *           通过PCF8574控制RS485芯片的RE引脚，切换发送/接收模式
 * 输入参数：
 *   - en: 模式控制标志
 *     0 - 设置为接收模式（RE=0）
 *     1 - 设置为发送模式（RE=1）
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 发送模式：使能RS485驱动器，可以发送数据
 *   - 接收模式：禁用RS485驱动器，可以接收数据
 *   - 通过PCF8574扩展IO控制RE引脚
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_rs485_tx_set(u8 en)
{
    /* 通过PCF8574设置RS485的RE引脚状态 */
    PCF8574_WriteBit(RS485_RE_IO, en);
}



/*******************************************************************************
函数名称：lm_rs485_DealData
功能描述：RS485通信数据处理函数，负责定时发送数据请求并处理接收到的响应
输入参数：无
输出参数：无  
返 回 值：无
其它说明：1.通过超时机制控制请求发送频率，防止总线拥塞
          2.自动检测响应就绪状态并进行数据处理
          3.使用HAL_GetTick()获取系统时间戳进行超时判断
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          创建
*******************************************************************************/
void lm_rs485_DealData(void)
{
    /* 检查是否达到发送超时时间且响应未就绪 */
    if ((HAL_GetTick() - last_request_time > RS485_TX_TIMEOUT) && (!lm_rs485_isresponseready())) {
        /* 发送读数据请求 */
        status = lm_rs485_sendreadrequest(RS485_ADDRESS, RS485_StartAddress, RS485_AddressNumber);
        /* 更新最后请求时间戳 */
        last_request_time = HAL_GetTick();
    }
     
    /* 检查响应是否就绪 */
    if (lm_rs485_isresponseready())
    {
        /* 解析接收到的响应数据 */
        status = lm_rs485_parseresponse(&response);
        /* 清除响应就绪标志 */
        lm_rs485_clearresponseflag(); 
    }         
}


void lm_rs485_DataPara(uint8_t *data,uint8_t lenth)
{
	if(data == NULL)
	{
		return;
	}
	
	battery_data.I_MAX_Discharge = merge_bytes_big_endian(data[0],data[1]);
	battery_data.V_MAX_Recharge  = merge_bytes_big_endian(data[2],data[3]);
	battery_data.I_MAX_Recharge  = merge_bytes_big_endian(data[4],data[5]);
	battery_data.I_MAX_Feedback  = merge_bytes_big_endian(data[6],data[7]);
	battery_data.V_TOTAL_BatteryPack  = merge_bytes_big_endian(data[8],data[9]);
	battery_data.Fault_Code_0x0A08  = merge_bytes_big_endian(data[10],data[11]);
	battery_data.Fault_Code_0x0A09  = merge_bytes_big_endian(data[12],data[13]);
	battery_data.Fault_Code_0x0A0A  = merge_bytes_big_endian(data[14],data[15]);
	battery_data.Fault_Code_0x0A0B  = merge_bytes_big_endian(data[16],data[17]);
	battery_data.Battery_Charging_Status    = merge_bytes_big_endian(data[18],data[19]);
	
}


uint16_t merge_bytes_big_endian(uint8_t high_byte, uint8_t low_byte)
{
    return ((uint16_t)high_byte << 8) | low_byte;
}

