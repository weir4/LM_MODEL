/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_common_function.h
 * 文件标识： 
 * 内容摘要： 动态空间管理
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者：    Qiguo_Cui                   
 * 完成日期： 2025年10月23日
 *
 *******************************************************************************/


#include "lm_floatvar_limit.h"

/*******************************************************************************
 函数名称： lm_floatvarlimit_Init
 功能描述： 初始化浮点变量限制模块的静态内存池，配置内存对齐参数和任务管理参数    
 输入参数：   
   - lm_function_name: 函数名称描述字符串，用于标识内存池功能
   - memory_pools: 指向静态内存池结构体的指针，用于存储初始化后的内存池信息
 输出参数：    
   - memory_pools: 初始化后的内存池结构体，包含对齐地址、可用大小等参数
 返 回 值：    无
 其它说明：    该函数负责初始化内存池，包括地址对齐处理、空间大小计算和基本参数配置			   
 			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/11/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/  
void lm_floatvarlimit_Init(char *lm_function_name, static_memory_pool_t *memory_pools)
{
    // 定义静态内存池数组，总大小为TOTAL_MEMORY_SIZE_LIMIT
    static uint8_t memory_pool[TOTAL_MEMORY_SIZE_LIMIT];

    // 对内存池地址进行向上对齐处理，获取对齐后的起始地址
    uint8_t* aligned_addr = (uint8_t*)ALIGN_UP(memory_pool, MEMORY_ALIGNMENT_LIMIT);
    // 计算对齐所需的填充字节数
    size_t padding = aligned_addr - memory_pool;
    // 计算对齐后可用的实际内存大小
    size_t usable_size = TOTAL_MEMORY_SIZE_LIMIT - padding;
    
    // 检查对齐后是否有足够的空间容纳所有任务
    if (usable_size <  SIGNALTASKSIZE_LIMIT * TASKCOUNT_LIMIT) {
        // 可用空间不足，直接返回初始化失败
        return;
    }

    // 设置内存池的基础地址（原始未对齐地址）
    memory_pools->base_address = memory_pool;  
    // 设置内存池的对齐后地址
    memory_pools->aligned_address = aligned_addr;
    // 设置内存池的总分配大小
    memory_pools->total_size = TOTAL_MEMORY_SIZE_LIMIT;
    // 设置内存池实际可用大小
    memory_pools->usable_size = usable_size;
    // 设置每个内存块的大小（单个滤波器任务大小）
    memory_pools->block_size = SIGNALTASKSIZE_LIMIT;
    // 设置内存池中块的总数量
    memory_pools->block_count = TASKCOUNT_LIMIT;
    // 初始化已使用块计数为0
    memory_pools->used_blocks = 0;
    // 初始化空闲块计数为总任务数
    memory_pools->free_blocks = TASKCOUNT_LIMIT;
    // 设置下一个所有者ID起始值
    memory_pools->next_owner_id = 1;
    // 记录内存对齐要求
    memory_pools->alignment = MEMORY_ALIGNMENT_LIMIT;
    // 记录对齐填充字节数
    memory_pools->alignment_padding = padding;
    // 设置内存池的功能描述信息
    memory_pools->memory_descrition = lm_function_name;
    
    // 调用静态内存池初始化函数，检查初始化结果
    if(false == lm_static_memorypool_init(memory_pools,TASKCOUNT_LIMIT))
    {
        // 内存池初始化失败，直接返回
        return;
    };
}

/*******************************************************************************
 函数名称： lm_floatvarlimit_register
 功能描述： 注册并初始化一个浮点变量限制器任务，分配内存并设置初始参数    
 输入参数：   
   - taskname: 任务名称字符串，用于标识和分配内存
   - initial_value: 初始输出值，限制器的起始输出值
   - tskperiod: 任务周期时间，用于计算每个周期的最大变化量
   - max_rise_rate: 最大上升速率，单位时间内允许的最大正向变化率
   - max_fall_rate: 最大下降速率，单位时间内允许的最大负向变化率
 输出参数：    无  
 返 回 值：    
   - fvarlimit_t*: 成功时返回初始化完成的限制器指针，失败时返回NULL
 其它说明：    该函数会从静态内存池中分配内存，并配置限制器的各项参数			   
 			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/11/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/  
fvarlimit_t *lm_floatvarlimit_register(char *taskname, float initial_value, float tskperiod, float max_rise_rate, float max_fall_rate)
{
    // 检查任务名称参数是否有效
    if(taskname == NULL)
    {
        // 任务名称为空，返回空指针
        return NULL;
    }
    
    // 从滤波器内存池中分配滤波器结构体内存
    fvarlimit_t *g_floatvarlimit = (fvarlimit_t*)lm_mallocmanager_alloc(taskname, sizeof(fvarlimit_t));
		
    // 设置限制器的上一次输出值为初始值
    g_floatvarlimit->last_output = initial_value;
    // 设置最大上升速率，取绝对值确保正值
    g_floatvarlimit->max_rise_rate = fabsf(max_rise_rate);
    // 设置最大下降速率，取绝对值确保正值
    g_floatvarlimit->max_fall_rate = fabsf(max_fall_rate);
    // 设置任务周期时间
    g_floatvarlimit->task_period = tskperiod;
    // 标记限制器已初始化完成
    g_floatvarlimit->initialized = true;
    
    // 返回初始化完成的滤波器指针
    return g_floatvarlimit;
}

/*******************************************************************************
 函数名称： lm_floatvarlimit_process
 功能描述： 处理浮点变量限制器的输入信号，根据上升下降速率限制输出变化    
 输入参数：   
   - fvarlimit: 指向浮点变量限制器结构体的指针
   - input: 输入信号值，需要进行变化率限制的原始输入
 输出参数：    无  
 返 回 值：    
   - float: 经过变化率限制处理后的输出信号值
 其它说明：    该函数会根据设定的最大上升下降速率，限制输出信号的变化幅度，
              确保输出平滑过渡，避免突变			   
 			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/11/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/  
float lm_floatvarlimit_process(fvarlimit_t *fvarlimit, float input)
{
    // 检查限制器指针是否有效，无效则直接返回输入值
    if (fvarlimit == NULL) {
        return input;
    }
    
    // 初始化检查，如果限制器未初始化，则用当前输入值初始化并返回
    if (!fvarlimit->initialized) {
        fvarlimit->last_output = input;
        fvarlimit->initialized = true;
        return input;
    }
    
    // 计算允许的最大上升变化量（上升速率 × 周期时间）
    float max_change_up = fvarlimit->max_rise_rate * fvarlimit->task_period;
    // 计算允许的最大下降变化量（下降速率 × 周期时间）
    float max_change_down = fvarlimit->max_fall_rate * fvarlimit->task_period;
    
    float output;  // 定义输出变量
    
    // 应用变化率限制逻辑
    if (input > fvarlimit->last_output + max_change_up) {
        // 输入超过最大上升限制，输出限制在上次输出加最大上升变化量
        output = fvarlimit->last_output + max_change_up;
    } else if (input < fvarlimit->last_output - max_change_down) {
        // 输入超过最大下降限制，输出限制在上次输出减最大下降变化量
        output = fvarlimit->last_output - max_change_down;
    } else {
        // 输入在允许变化范围内，直接输出输入值
        output = input;
    }
    
    // 保存当前输出值，作为下一次处理的上次输出
    fvarlimit->last_output = output;
    
    // 返回经过限制处理后的输出值
    return output;
}





