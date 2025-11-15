/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_kalman_one_dim.c
 * 文件标识： 
 * 内容摘要： 油箱信号处理
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者：    Qiguo_Cui                   
 * 完成日期： 2025年09月01日
 *
 *******************************************************************************/


/*******************************************************************************
状态变量：油位（level）
状态方程：假设油位变化是线性的，并且有过程噪声
测量方程：我们通过传感器直接测量油位，但有测量噪声

状态方程：x(k) = A * x(k-1) + B * u(k) + w(k) 其中，我们假设A=1，B=0（没有控制量），w(k)是过程噪声，服从正态分布N(0, Q)
测量方程：z(k) = H * x(k) + v(k) 其中，H=1，v(k)是测量噪声，服从正态分布N(0, R)
卡尔曼滤波的五个核心公式：
预测状态：x_pred = A * x_prev
预测误差协方差：P_pred = A * P_prev * A' + Q
卡尔曼增益：K = P_pred * H' * (H * P_pred * H' + R)^(-1)
更新状态：x_update = x_pred + K * (z - H * x_pred)
更新误差协方差：P_update = (I - K * H) * P_pred
*******************************************************************************/


#include "lm_kalman_one_dim.h"


void lm_kalman_onedim_Init(char *lm_function_name, static_memory_pool_t *memory_pools)
{
    // 定义静态内存池数组，大小为PID控制器所需的总内存大小
    static uint8_t memory_pool[TOTAL_MEMORY_SIZE_KALMAN_ONEDIM];

    // 对内存池地址进行向上对齐处理，获取对齐后的起始地址，确保内存访问效率
    uint8_t* aligned_addr = (uint8_t*)ALIGN_UP(memory_pool, MEMORY_ALIGNMENT_KALMAN_ONEDIM);
    
    // 计算对齐所需的填充字节数，即原始地址与对齐后地址的差值
    size_t padding = aligned_addr - memory_pool;
    
    // 计算对齐后可用的实际内存大小，从总大小中减去填充字节
    size_t usable_size = TOTAL_MEMORY_SIZE_KALMAN_ONEDIM - padding;
    
    // 检查对齐后是否有足够的空间容纳所有PID任务所需的内存块
    if (usable_size <  SIGNALTASKSIZE_KALMAN_ONEDIM * TASKCOUNT_KALMAN_ONEDIM) {
        // 可用空间不足，直接返回初始化失败，不执行后续操作
        return;
    }

    // 设置内存池的基础地址（原始未对齐地址），用于后续内存管理
    memory_pools->base_address = memory_pool;  
    
    // 设置内存池的对齐后地址，实际分配内存时将从此地址开始
    memory_pools->aligned_address = aligned_addr;
    
    // 设置内存池的总分配大小，包括对齐填充部分
    memory_pools->total_size = TOTAL_MEMORY_SIZE_KALMAN_ONEDIM;
    
    // 设置内存池实际可用大小，不包括对齐填充部分
    memory_pools->usable_size = usable_size;
    
    // 设置每个内存块的大小，即单个PID控制器任务所需的内存大小
    memory_pools->block_size = SIGNALTASKSIZE_KALMAN_ONEDIM;
    
    // 设置内存池中块的总数量，即支持的最大PID控制器实例数
    memory_pools->block_count = TASKCOUNT_KALMAN_ONEDIM;
    
    // 初始化已使用块计数为0，表示当前没有任何PID控制器实例
    memory_pools->used_blocks = 0;
    
    // 初始化空闲块计数为总任务数，表示所有PID控制器实例槽位都可用
    memory_pools->free_blocks = TASKCOUNT_KALMAN_ONEDIM;
    
    // 设置下一个所有者ID起始值，用于唯一标识每个PID控制器实例
    memory_pools->next_owner_id = 1;
    
    // 记录内存对齐要求，用于后续内存分配时的对齐检查
    memory_pools->alignment = MEMORY_ALIGNMENT_KALMAN_ONEDIM;
    
    // 记录对齐填充字节数，用于内存使用统计和调试
    memory_pools->alignment_padding = padding;
    
    // 设置内存池的功能描述信息，便于调试和日志记录
    memory_pools->memory_descrition = lm_function_name;
    
    // 调用静态内存池初始化函数，进行底层内存池的初始化设置
    if(false == lm_static_memorypool_init(memory_pools, TASKCOUNT_KALMAN_ONEDIM))
    {
        // 内存池初始化失败，直接返回，不执行后续操作
        return;
    }
}

FuelKalmanFilter_t *lm_kalman_onedim_register(char * taskname, float q, float r, float initial_value, float initial_p)
{
    // 检查任务名称参数是否有效，防止空指针访问
    if(taskname == NULL)
    {
        // 任务名称为空，返回空指针表示注册失败
        return NULL;
    }
    
		
		FuelKalmanFilter_t *fuelkalmanfilter = (FuelKalmanFilter_t*)lm_mallocmanager_alloc(taskname, sizeof(FuelKalmanFilter_t));
		
    fuelkalmanfilter->q = q;
    fuelkalmanfilter->r = r;
    fuelkalmanfilter->x = initial_value;
    fuelkalmanfilter->p = initial_p;
    fuelkalmanfilter->k = 0.0f;
		return fuelkalmanfilter;
}








float lm_kalman_onedim_process(FuelKalmanFilter_t* kf, float measurement) {
    // 预测步骤
    kf->p = kf->p + kf->q;
    
    // 更新步骤
    kf->k = kf->p / (kf->p + kf->r);           // 计算卡尔曼增益
    kf->x = kf->x + kf->k * (measurement - kf->x); // 更新状态估计
    kf->p = (1 - kf->k) * kf->p;               // 更新协方差估计
    
    return kf->x;
}


