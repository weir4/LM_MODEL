/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_pid.c
 * 文件标识： 
 * 内容摘要： pid 控制模块
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年10月22日
 *
 *******************************************************************************/



#include "lm_pid.h"

/*******************************************************************************
 函数名称：lm_pid_Init
 功能描述：初始化PID控制器的静态内存池，为后续PID控制器实例分配内存空间    
 输入参数：lm_function_name - 功能描述字符串，用于标识内存池用途
          memory_pools - 指向静态内存池结构体的指针，用于存储内存池管理信息   
 输出参数：memory_pools - 初始化后的内存池管理结构体    
 返 回 值：无    
 其它说明：该函数负责计算内存对齐、设置内存池参数，并调用底层内存池初始化函数
           如果内存不足或初始化失败，函数将直接返回不执行后续操作			   
 			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/11/11     V1.00          Qiguo_Cui          创建
 *******************************************************************************/  
void lm_pid_Init(char *lm_function_name, static_memory_pool_t *memory_pools)
{
    // 定义静态内存池数组，大小为PID控制器所需的总内存大小
    static uint8_t memory_pool[TOTAL_MEMORY_SIZE_PID];

    // 对内存池地址进行向上对齐处理，获取对齐后的起始地址，确保内存访问效率
    uint8_t* aligned_addr = (uint8_t*)ALIGN_UP(memory_pool, MEMORY_ALIGNMENT_PID);
    
    // 计算对齐所需的填充字节数，即原始地址与对齐后地址的差值
    size_t padding = aligned_addr - memory_pool;
    
    // 计算对齐后可用的实际内存大小，从总大小中减去填充字节
    size_t usable_size = TOTAL_MEMORY_SIZE_PID - padding;
    
    // 检查对齐后是否有足够的空间容纳所有PID任务所需的内存块
    if (usable_size <  SIGNALTASKSIZE_PID * TASKCOUNT_PID) {
        // 可用空间不足，直接返回初始化失败，不执行后续操作
        return;
    }

    // 设置内存池的基础地址（原始未对齐地址），用于后续内存管理
    memory_pools->base_address = memory_pool;  
    
    // 设置内存池的对齐后地址，实际分配内存时将从此地址开始
    memory_pools->aligned_address = aligned_addr;
    
    // 设置内存池的总分配大小，包括对齐填充部分
    memory_pools->total_size = TOTAL_MEMORY_SIZE_PID;
    
    // 设置内存池实际可用大小，不包括对齐填充部分
    memory_pools->usable_size = usable_size;
    
    // 设置每个内存块的大小，即单个PID控制器任务所需的内存大小
    memory_pools->block_size = SIGNALTASKSIZE_PID;
    
    // 设置内存池中块的总数量，即支持的最大PID控制器实例数
    memory_pools->block_count = TASKCOUNT_PID;
    
    // 初始化已使用块计数为0，表示当前没有任何PID控制器实例
    memory_pools->used_blocks = 0;
    
    // 初始化空闲块计数为总任务数，表示所有PID控制器实例槽位都可用
    memory_pools->free_blocks = TASKCOUNT_PID;
    
    // 设置下一个所有者ID起始值，用于唯一标识每个PID控制器实例
    memory_pools->next_owner_id = 1;
    
    // 记录内存对齐要求，用于后续内存分配时的对齐检查
    memory_pools->alignment = MEMORY_ALIGNMENT_PID;
    
    // 记录对齐填充字节数，用于内存使用统计和调试
    memory_pools->alignment_padding = padding;
    
    // 设置内存池的功能描述信息，便于调试和日志记录
    memory_pools->memory_descrition = lm_function_name;
    
    // 调用静态内存池初始化函数，进行底层内存池的初始化设置
    if(false == lm_static_memorypool_init(memory_pools, TASKCOUNT_PID))
    {
        // 内存池初始化失败，直接返回，不执行后续操作
        return;
    }
}



/*******************************************************************************
 函数名称：lm_pid_register
 功能描述：注册并初始化一个PID控制器实例，从内存池分配内存并设置初始参数    
 输入参数：taskname - 任务名称字符串，用于标识PID控制器实例
          Kp - 比例系数，决定系统对当前误差的反应强度
          Ki - 积分系数，用于消除稳态误差，值越大消除速度越快但可能引起振荡
          Kd - 微分系数，预测误差变化趋势，提高系统稳定性
          output_min - 输出最小值限制，保护执行机构不过载
          output_max - 输出最大值限制，保护执行机构不过载
          error_threshold - 积分分离阈值，当误差绝对值小于此值时启用积分作用
          delta_max - 单步输出增量最大值限制，防止输出突变
          dt - 采样时间周期，单位秒，必须大于零   
 输出参数：无    
 返 回 值：PID_t* - 成功返回指向PID控制器实例的指针，失败返回NULL    
 其它说明：该函数会从预分配的内存池中申请内存，并初始化所有PID参数和状态变量
           如果任务名称为空或内存分配失败，函数将返回NULL指针			   
 			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/11/11     V1.00          Qiguo_Cui          创建
 *******************************************************************************/  
PID_t *lm_pid_register(char * taskname, float Kp, float Ki, float Kd, 
                      float output_min, float output_max, 
                      float error_threshold, float delta_max, float dt)
{
    // 检查任务名称参数是否有效，防止空指针访问
    if(taskname == NULL)
    {
        // 任务名称为空，返回空指针表示注册失败
        return NULL;
    }
    
    // 从PID内存池中分配PID控制器结构体内存，使用任务名称作为标识
    PID_t *pid= (PID_t*)lm_mallocmanager_alloc(taskname, sizeof(PID_t));
    // 设置PID控制器的比例系数，影响系统响应速度
    pid->Kp = Kp;
    
    // 设置PID控制器的积分系数，影响消除稳态误差的能力
    pid->Ki = Ki;
    
    // 设置PID控制器的微分系数，影响系统的阻尼特性
    pid->Kd = Kd;
    
    // 初始化当前误差值，设置为零表示初始无误差
    pid->error[0] = 0.0f;
    
    // 初始化上一次误差值，设置为零表示初始无历史误差
    pid->error[1] = 0.0f;
    
    // 初始化上上次误差值，设置为零表示初始无历史误差
    pid->error[2] = 0.0f;
    
    // 初始化上一次输出值，设置为零表示初始输出为零
    pid->output_prev = 0.0f;
    
    // 设置输出最小值限制，防止执行机构反向过载
    pid->output_min = output_min;
    
    // 设置输出最大值限制，防止执行机构正向过载
    pid->output_max = output_max;
    
    // 设置积分分离阈值，当误差绝对值小于此值时启用积分作用
    pid->error_threshold = error_threshold;
    
    // 设置单步输出增量最大值，限制输出变化的剧烈程度
    pid->delta_max = delta_max;
    
    // 设置积分项限幅值，限制积分作用的强度，避免积分饱和
    // 计算为输出范围的50%，在输出限幅和积分作用间取得平衡
    pid->integral_limit = (output_max - output_min) * 0.5f;
    
    // 设置采样时间周期，单位秒，必须大于零以确保控制算法正确执行
    pid->dt = dt;
    
    // 返回初始化完成的PID控制器实例指针
    return pid;
}



/*******************************************************************************
 函数名称：lm_pid_process
 功能描述：执行增量式PID控制算法计算，根据目标值和测量值计算控制输出    
 输入参数：pid - 指向PID控制器实例的指针，包含控制器参数和状态
          target - 目标设定值，系统期望达到的状态值
          measurement - 实际测量值，系统当前的实际状态值   
 输出参数：pid - 更新PID控制器内部状态（误差队列和上一次输出值）    
 返 回 值：float - 计算得到的控制输出值，用于驱动执行机构    
 其它说明：该函数实现增量式PID算法，具有积分分离功能，但微分项计算存在理论错误
           当前代码中输出限幅和增量限幅被注释掉，实际使用时需要根据情况启用			   
 			   
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/11/11     V1.00          Qiguo_Cui          创建
 *******************************************************************************/  
float lm_pid_process(PID_t *pid, float target, float measurement) {
    // 检查采样时间是否有效，防止除零错误和无效计算
    if (pid->dt <= 0.0f)
		{
        // 采样时间无效，返回上一次输出值保持系统稳定
        return pid->output_prev;
    }
    // 更新误差队列：将上一次误差e(k-1)移动到上上次误差e(k-2)位置
    pid->error[2] = pid->error[1];
    
    // 更新误差队列：将当前误差e(k)移动到上一次误差e(k-1)位置
    pid->error[1] = pid->error[0];
    
    // 计算当前误差：目标设定值减去实际测量值，得到当前控制偏差
    pid->error[0] = target - measurement;
    
    // 计算比例项增量：比例系数乘以当前误差与上一次误差的差值
    // 反映误差的变化趋势，提供快速响应能力
    float delta_p = pid->Kp * (pid->error[0] - pid->error[1]);
    
    // 初始化积分项增量为零，默认不进行积分作用
    float delta_i = 0.0f;
    
    // 检查当前误差绝对值是否小于积分分离阈值，决定是否启用积分作用
    if (fabsf(pid->error[0]) < pid->error_threshold) {
        // 误差较小时启用积分作用：积分系数乘以当前误差乘以采样时间
        // 用于消除稳态误差，提高控制精度
        delta_i = pid->Ki * pid->error[0] * pid->dt;
        
        // 检查积分项增量是否超过正方向限幅值，防止积分作用过强
        if (delta_i > pid->integral_limit) {
            // 积分项超过正限幅，将其限制在最大值，避免积分饱和
            delta_i = pid->integral_limit;
        } 
        // 检查积分项增量是否超过负方向限幅值
        else if (delta_i < -pid->integral_limit) {
            // 积分项超过负限幅，将其限制在最小值，避免积分饱和
            delta_i = -pid->integral_limit;
        }
    }
    // 误差较大时，保持积分项增量为零，实现积分分离，避免过大超调
    
    // 计算微分项增量：微分系数乘以误差的二阶差分除以采样时间
    // 注：此处存在理论错误，正确的微分项不应除以dt
    // 当前实现会导致微分作用过强，可能引起系统振荡
    float delta_d = pid->Kd * (pid->error[0] - 2.0f * pid->error[1] + pid->error[2]) / pid->dt;
    
    // 计算总输出增量：比例项、积分项和微分项增量的代数和
    float delta_output = delta_p + delta_i + delta_d;
    
    // 注：增量限幅代码被注释掉，实际使用时需要根据系统特性启用
    // 增量限幅可以防止单步输出变化过大，提高系统平稳性
    if (delta_output > pid->delta_max) {
        delta_output = pid->delta_max;
    } else if (delta_output < - pid->delta_max) {
        delta_output = - pid->delta_max;
    }
    
    // 计算新的输出值：上一次输出值加上本次输出增量
    float new_output = pid->output_prev + delta_output;
    
    // 注：输出值限幅代码被注释掉，实际使用时必须启用
    // 输出限幅是抗积分饱和的关键措施，保护执行机构在安全范围内工作
    if (new_output > pid->output_max) {
        new_output = pid->output_max;
    } else if (new_output < pid->output_min) {
        new_output = pid->output_min;
    }
    
    // 更新控制器状态：将本次输出值保存为上一次输出值，供下次计算使用
    pid->output_prev = new_output;
    
    // 返回计算得到的控制输出值，用于驱动实际执行机构
    return new_output;
}

