/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_pushrod_signal.c
 * 文件标识： 
 * 内容摘要： 模拟量采集驱动文件
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年10月05日
 *
 *******************************************************************************/

#include "lm_pushrod_signal.h"
#include "lm_pushrod_control.h"


uint8_t PushActuatorCommandCurrentSignal[4] = {0};
uint8_t PushActuatorCommandLastSignal[4] = {0};

uint16_t signal_processing;

extern MotorStateMachine motor_handle;


struct {
    uint8_t  shift_counter;        // 移位计数器
    uint8_t  scan_interval;        // 20ms扫描周期
    uint8_t  filter_samples;       // 4次采样滤波 (0x0f)
    uint8_t  accumulators[INPUT_COUNT];
} signal_state = {0,20,8,4};



static const GPIO_Config_t inputConfigs[INPUT_COUNT] = {
    [INPUT_UP_SIGNAL] = {
        .port = GPIOB,
        .pin = GPIO_PIN_6,
        .mode = GPIO_MODE_INPUT
    },
    [INPUT_DOWN_SIGNAL] = {
        .port = GPIOB,
        .pin = GPIO_PIN_4,
        .mode = GPIO_MODE_INPUT
    },
    [INPUT_DOWN_LIMIT] = {
        .port = GPIOA,
        .pin = GPIO_PIN_10,
        .mode = GPIO_MODE_IT_RISING_FALLING
    },
    [INPUT_UP_LIMIT] = {
        .port = GPIOA,
        .pin =  GPIO_PIN_11,
        .mode = GPIO_MODE_IT_RISING_FALLING
    }
};


/*******************************************************************************
 函数名称：lm_signal_init
 功能描述：初始化所有输入信号的GPIO配置和滤波算法    
 输入参数：无   
 输出参数：无    
 返 回 值：无    
 其它说明： 
    - 配置JTAG引脚为禁用状态，释放相关GPIO引脚
    - 使能GPIOA和GPIOB时钟
    - 遍历所有输入信号，配置GPIO模式和初始化滤波算法
    - 每个信号关联对应的GPIO端口和引脚			   
 			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/09/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_signal_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;  // 定义GPIO初始化结构体
    
    // 配置JTAG禁用（仅需执行一次），释放PA15、PB3、PB4用于普通GPIO功能
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
    
	  __HAL_RCC_GPIOA_CLK_ENABLE();   // 使能GPIOA时钟，用于控制GPIOA相关引脚
    __HAL_RCC_GPIOB_CLK_ENABLE();   // 使能GPIOB时钟，用于控制GPIOB相关引脚
	
    // 遍历所有输入信号进行初始化，INPUT_COUNT为定义的输入信号总数
    for (int i = 0; i < INPUT_COUNT; i++) {
        const GPIO_Config_t* cfg = &inputConfigs[i];  // 获取当前信号的GPIO配置结构体指针
        
			
        GPIO_InitStructure.Pin     = cfg->pin;        // 设置GPIO引脚号
        GPIO_InitStructure.Speed   = GPIO_SPEED_FREQ_HIGH;  // 设置GPIO输出速度为高速
        GPIO_InitStructure.Mode    = cfg->mode;       // 设置GPIO工作模式（输入/输出等）
				
				if(i == 2 || i == 3)
				{
					GPIO_InitStructure.Pull  = GPIO_PULLUP;
				}
				
        HAL_GPIO_Init(cfg->port, &GPIO_InitStructure);      // 初始化GPIO引脚配置
    }
		HAL_NVIC_SetPriority(EXTI15_10_IRQn, 10, 0);      // PA10和PA11共用EXTI15_10中断线
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);              // 使能中断
}

/*******************************************************************************
 函数名称：lm_signal_deal
 功能描述：处理所有输入信号的滤波结果并更新执行器命令信号    
 输入参数：无   
 输出参数：无    
 返 回 值：无    
 其它说明：
    - 调用滤波算法获取4个信号的稳定状态结果
    - 将8位结果按位分解为4个独立的执行器命令信号
    - 更新PushActuatorCommandCurrentSignal数组中的当前信号状态			   
 			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/09/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_signal_deal(void)
{
	  int system_time = HAL_GetTick();
    if ((system_time-signal_processing) < signal_state.scan_interval) {
        return;
    }
    
    signal_processing = system_time;
    
    // 处理所有信号
    for (int i = 0; i < INPUT_COUNT; i++) {
        
        // 读取GPIO状态并更新累加器
			InputState state = lm_input_read((InputSignalType)i);
        if (state == INPUT_INACTIVE) {
					  signal_state.accumulators[i] &= ~(1 << signal_state.shift_counter);
        } else {
            signal_state.accumulators[i] |= (1 << signal_state.shift_counter);
        }
    }
    
    // 更新移位计数器
    signal_state.shift_counter++;
    
    // 检查是否完成一个滤波周期
    if (signal_state.shift_counter >= 8) {
        signal_state.shift_counter = 0;
        
        // 更新所有信号值
        for (int i = 0; i < INPUT_UP_LIMIT; i++) {            
            // 滤波处理：检查低4位是否全为1
           PushActuatorCommandCurrentSignal[i] = ((signal_state.accumulators[i] & 0x0F) == 0x0F) ? 
                             INPUT_ACTIVE : INPUT_INACTIVE;
        }
    }
}

/*******************************************************************************
 函数名称：lm_input_read
 功能描述：读取指定输入信号的当前状态    
 输入参数：signal - 输入信号类型枚举值，指定要读取的输入通道   
 输出参数：无    
 返 回 值：InputState - 输入信号状态，返回INPUT_ACTIVE(激活)或INPUT_INACTIVE(非激活)    
 其它说明：此函数通过查询GPIO引脚电平来获取输入信号状态
           当引脚为低电平时返回INPUT_ACTIVE，高电平时返回INPUT_INACTIVE
           包含参数有效性检查，防止数组越界访问			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/10/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/

InputState lm_input_read(InputSignalType signal) {
    /* 检查输入信号参数是否在有效范围内，防止数组越界访问 */
    if (signal >= INPUT_COUNT) 
        return INPUT_INACTIVE;
    
    /* 根据信号类型获取对应的GPIO配置信息 */
    const GPIO_Config_t* cfg = &inputConfigs[signal];
    
    /* 读取GPIO引脚电平并转换为逻辑状态：低电平为激活，高电平为非激活 */
    return (HAL_GPIO_ReadPin(cfg->port, cfg->pin) == Bit_SET) ? 
           INPUT_INACTIVE : INPUT_ACTIVE;
}

/*******************************************************************************
 函数名称：EXTI15_10_IRQHandler
 功能描述：外部中断15-10线中断服务函数    
 输入参数：无   
 输出参数：无    
 返 回 值：无    
 其它说明：此函数处理GPIO引脚10和11的外部中断请求
           调用HAL库的标准中断处理函数来自动清除中断标志位
           并触发对应的回调函数执行具体的中断处理逻辑			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/10/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void EXTI15_10_IRQHandler(void)
{
    /* 调用HAL库的GPIO外部中断处理函数处理引脚10中断，自动清除中断标志 */
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_10);
    
    /* 调用HAL库的GPIO外部中断处理函数处理引脚11中断，自动清除中断标志 */
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_11);
}

/*******************************************************************************
 函数名称：HAL_GPIO_EXTI_Callback
 功能描述：GPIO外部中断通用回调函数，处理具体的中断逻辑    
 输入参数：GPIO_Pin - 触发中断的GPIO引脚编号   
 输出参数：无    
 返 回 值：无    
 其它说明：此函数由HAL_GPIO_EXTI_IRQHandler自动调用
           实现软件消抖功能，防止机械开关抖动导致的多次误触发
           根据不同的引脚号分发到对应的专用中断处理函数			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/10/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    /* 定义静态变量记录各引脚上次有效中断时间，用于软件消抖 */
    static uint32_t last_interrupt_time_10 = 0;  /* PA10引脚上次中断时间戳 */
    static uint32_t last_interrupt_time_11 = 0;  /* PA11引脚上次中断时间戳 */
    
    /* 获取当前系统时间戳，单位为毫秒 */
    uint32_t current_time = HAL_GetTick();
    
    /* 根据触发中断的具体引脚号进行分别处理 */
    switch(GPIO_Pin)
    {
        case GPIO_PIN_10:
            /* 软件消抖处理：检查距离上次PA10中断是否超过50ms，避免开关抖动误触发 */
            if((current_time - last_interrupt_time_10) > 50)
            {
                /* 调用PA10专用中断处理函数执行具体的业务逻辑 */
                lm_pa10_interrupt_handler();
                
                /* 更新PA10最后一次有效中断时间戳为当前时间 */
                last_interrupt_time_10 = current_time;
            }
            break;
            
        case GPIO_PIN_11:
            /* 软件消抖处理：检查距离上次PA11中断是否超过50ms，避免开关抖动误触发 */
            if((current_time - last_interrupt_time_11) > 50)
            {
                /* 调用PA11专用中断处理函数执行具体的业务逻辑 */
                lm_pa11_interrupt_handler();
                
                /* 更新PA11最后一次有效中断时间戳为当前时间 */
                last_interrupt_time_11 = current_time;
            }
            break;
    }
}

/*******************************************************************************
 函数名称：lm_pa10_interrupt_handler
 功能描述：PA10引脚中断专用处理函数，处理反向限位信号    
 输入参数：无   
 输出参数：无    
 返 回 值：无    
 其它说明：此函数读取PA10引脚当前电平状态，更新电机反向限位信号状态
           将限位信号状态同步到推杆执行器命令信号数组中
           低电平表示限位触发(激活)，高电平表示限位未触发(非激活)			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/10/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_pa10_interrupt_handler()
{
    /* 读取PA10引脚当前的电平状态 */
	 bool current_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10);
	 
	 /* 根据引脚电平状态更新反向限位信号 */
	 if(current_state)
	 {
         /* 引脚为高电平，限位未触发，设置反向限位信号为非激活状态 */
		   motor_handle.signals.reverse_limit =  INPUT_INACTIVE;
	 }
	 else
	 {
         /* 引脚为低电平，限位触发，设置反向限位信号为激活状态 */
		    motor_handle.signals.reverse_limit =  INPUT_ACTIVE;
	 }
	 
	 /* 将反向限位信号状态同步到推杆执行器命令信号数组的第4个元素（索引3） */
	 PushActuatorCommandCurrentSignal[3] = motor_handle.signals.reverse_limit;
}

/*******************************************************************************
 函数名称：lm_pa11_interrupt_handler
 功能描述：PA11引脚中断专用处理函数，处理正向限位信号    
 输入参数：无   
 输出参数：无    
 返 回 值：无    
 其它说明：此函数读取PA11引脚当前电平状态，更新电机正向限位信号状态
           将限位信号状态同步到推杆执行器命令信号数组中
           低电平表示限位触发(激活)，高电平表示限位未触发(非激活)			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/10/21     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void lm_pa11_interrupt_handler()
{
    /* 读取PA11引脚当前的电平状态 */
	 bool current_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11);
	 
	 /* 根据引脚电平状态更新正向限位信号 */
	 if(current_state)
	 {
         /* 引脚为高电平，限位未触发，设置正向限位信号为非激活状态 */
		   motor_handle.signals.forward_limit =  INPUT_INACTIVE;
	 }   
	 else
	 {
         /* 引脚为低电平，限位触发，设置正向限位信号为激活状态 */
		   motor_handle.signals.forward_limit =  INPUT_ACTIVE;
	 }
	 
	 /* 将正向限位信号状态同步到推杆执行器命令信号数组的第3个元素（索引2） */
	 PushActuatorCommandCurrentSignal[2] = motor_handle.signals.forward_limit;
}

