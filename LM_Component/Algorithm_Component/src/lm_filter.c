/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_filter.c
 * 文件标识： 
 * 内容摘要： 滤波算法实现
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者：    Qiguo_Cui                   
 * 完成日期： 2025年09月01日
 *
 *******************************************************************************/

#include "lm_filter.h"

/*******************************************************************************
函数名称：lm_filterBW_Init
功能描述：初始化滤波器静态内存池，配置内存对齐和块大小参数    
输入参数：lm_function_name - 滤波器功能描述字符串指针
         taskcount - 需要创建的滤波器任务数量
         signaltask_size - 单个滤波器任务所需内存大小
         memory_alignment - 内存对齐要求
         memory_pools - 指向静态内存池结构体的指针   
输出参数：初始化后的静态内存池结构体    
返回值：无    
其它说明：该函数计算总内存需求，进行地址对齐处理，并初始化内存池管理结构			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/21     V1.00          Qiguo_Cui          创建
*******************************************************************************/  
void lm_filterBW_Init(char *lm_function_name,static_memory_pool_t *memory_pools)
{
    // 计算总内存需求：所有任务内存 + 对齐填充空间
    static uint8_t memory_pool[TOTAL_MEMORY_SIZE_FILTER];

    // 对内存池地址进行向上对齐处理，获取对齐后的起始地址
	 uint8_t* aligned_addr = (uint8_t*)ALIGN_UP(memory_pool, MEMORY_ALIGNMENT_FILTER);
    // 计算对齐所需的填充字节数
    size_t padding = aligned_addr - memory_pool;
    // 计算对齐后可用的实际内存大小
    size_t usable_size = TOTAL_MEMORY_SIZE_FILTER - padding;
    
    // 检查对齐后是否有足够的空间容纳所有任务
    if (usable_size <  SIGNALTASKSIZE_FILTER * TASKCOUNT_FILTER) {
        // 可用空间不足，直接返回初始化失败
        return;
    }

    // 设置内存池的基础地址（原始未对齐地址）
	  memory_pools->base_address = memory_pool;  
    // 设置内存池的对齐后地址
    memory_pools->aligned_address = aligned_addr;
    // 设置内存池的总分配大小
	  memory_pools->total_size = TOTAL_MEMORY_SIZE_FILTER;
    // 设置内存池实际可用大小
    memory_pools->usable_size = usable_size;
    // 设置每个内存块的大小（单个滤波器任务大小）
    memory_pools->block_size = SIGNALTASKSIZE_FILTER;
    // 设置内存池中块的总数量
    memory_pools->block_count = TASKCOUNT_FILTER;
    // 初始化已使用块计数为0
    memory_pools->used_blocks = 0;
    // 初始化空闲块计数为总任务数
    memory_pools->free_blocks = TASKCOUNT_FILTER;
    // 设置下一个所有者ID起始值
    memory_pools->next_owner_id = 1;
    // 记录内存对齐要求
    memory_pools->alignment = MEMORY_ALIGNMENT_FILTER;
    // 记录对齐填充字节数
    memory_pools->alignment_padding = padding;
    // 设置内存池的功能描述信息
	  memory_pools->memory_descrition = lm_function_name;
    
    // 调用静态内存池初始化函数，检查初始化结果
	if(false == lm_static_memorypool_init(memory_pools,TASKCOUNT_FILTER))
	{
        // 内存池初始化失败，直接返回
		return;
	}
}

/*******************************************************************************
函数名称：lm_filterBW_register
功能描述：注册并初始化一个新的巴特沃斯滤波器实例    
输入参数：taskname - 滤波器任务名称字符串指针
         order - 滤波器阶数（0-3阶）
         cutiffF - 滤波器截止频率（Hz）
         tskperiod - 任务周期时间（毫秒）   
输出参数：初始化完成的滤波器结构体指针    
返回值：成功返回滤波器指针，失败返回NULL    
其它说明：该函数从静态内存池分配滤波器内存，并设置滤波器基本参数			   
			   
修改日期      版本号          修改人            修改人            修改内容
-----------------------------------------------------------------------------
2025/10/21     V1.00          Qiguo_Cui          创建
*******************************************************************************/  
Filter_t *lm_filterBW_register(char * taskname, int order, float cutiffF, float tskperiod)
{
    // 检查任务名称参数是否有效
	if(taskname == NULL)
	{
        // 任务名称为空，返回空指针
		return NULL;
	}
    
    // 从滤波器内存池中分配滤波器结构体内存
    Filter_t *g_Filter  = (Filter_t*)lm_mallocmanager_alloc(taskname, sizeof(Filter_t));
    // 设置滤波器的阶数参数s
	  g_Filter->Order   =  order;
    // 设置滤波器的截止频率参数
	  g_Filter->CutOffF =  cutiffF;
    // 设置滤波器的采样时间（将毫秒转换为秒）
	  g_Filter->Ts      =  tskperiod/1000.0;
    
    // 返回初始化完成的滤波器指针
    return g_Filter;
}

/*******************************************************************************
函数名称：lm_filterBW_process
功能描述：执行巴特沃斯滤波器处理，根据阶数计算滤波器系数并处理输入信号    
输入参数：filter - 指向滤波器结构体的指针
         input - 输入信号值   
输出参数：无    
返回值：滤波后的输出信号值    
其它说明：该函数支持0-3阶巴特沃斯滤波器，自动计算相应阶数的差分方程系数			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/21     V1.00          Qiguo_Cui          创建
*******************************************************************************/  
float lm_filterBW_process(Filter_t * filter, float input)
{
    // 定义输出变量
	float Output;
    // 定义循环计数器
	int i;
    // 定义滤波器计算中间变量：分母、一阶、二阶、三阶项
	float Den, OneOrder, TwoOrder, ThreeOrder;
    
    // 将当前输入值存入滤波器结构体
	(*filter).Xk = input;
	
    // 根据滤波器阶数选择不同的系数计算路径
	switch ((*filter).Order)
	{
			case 0:  // 0阶滤波器（直通）
                // 设置0阶滤波器系数为1（直通）
				(*filter).A1 = 1.0;
                // 更新滤波器状态计数器
				(*filter).state = (*filter).state + 1;
				break;
                
			case 1:  // 1阶巴特沃斯滤波器：1/(S+1)
                // 计算一阶项的归一化频率
				OneOrder = 6.2832 * (*filter).Ts * (*filter).CutOffF;
                // 计算差分方程分母
				Den = 1 + OneOrder;
                // 计算当前输入项系数
				(*filter).A1 = OneOrder / Den;
                // 计算历史输出项系数
				(*filter).Bk[0] = 1 / Den;
                // 更新滤波器状态计数器
				(*filter).state = (*filter).state + 1;
				break;
                
			case 2:  // 2阶巴特沃斯滤波器：1/(S*S+1.4142S+1)
                // 计算一阶项的归一化频率
				OneOrder = 6.2832 * (*filter).Ts * (*filter).CutOffF;
                // 计算二阶项的归一化频率
				TwoOrder = OneOrder * OneOrder;
                // 计算差分方程分母（巴特沃斯多项式）
				Den = 1 + 1.4142 * OneOrder + TwoOrder;
                // 计算当前输入项系数
				(*filter).A1 = TwoOrder / Den;
                // 计算第一个历史输出项系数
				(*filter).Bk[0] = (2 + 1.4142 * OneOrder) / Den;
                // 计算第二个历史输出项系数
				(*filter).Bk[1] = -1 / Den;
                // 更新滤波器状态计数器
				(*filter).state = (*filter).state + 1;
				break;
                
			case 3:  // 3阶巴特沃斯滤波器：1/(S*S*S+2S*S+2S+1)
                // 计算一阶项的归一化频率
				OneOrder = 6.2832 * (*filter).Ts * (*filter).CutOffF;
                // 计算二阶项的归一化频率
				TwoOrder = OneOrder * OneOrder;
                // 计算三阶项的归一化频率
				ThreeOrder = TwoOrder * OneOrder;
                // 计算差分方程分母（巴特沃斯多项式）
				Den = 1 + 2.0 * OneOrder + 2.0 * TwoOrder + ThreeOrder;
                // 计算当前输入项系数
				(*filter).A1 = ThreeOrder / Den;
                // 计算第一个历史输出项系数
				(*filter).Bk[0] = (3 + 2 * 2.0 * OneOrder + 2.0 * TwoOrder) / Den;
                // 计算第二个历史输出项系数
				(*filter).Bk[1] = -(3 + 2.0 * OneOrder) / Den;
                // 计算第三个历史输出项系数
				(*filter).Bk[2] = 1 / Den;
                // 更新滤波器状态计数器
				(*filter).state = (*filter).state + 1;
				break;
                
			default:  // 无效阶数处理
                // 将阶数重置为0阶（直通）
				(*filter).Order = 0;
                // 设置系数为1（直通）
				(*filter).A1 = 1.0;
                // 更新滤波器状态计数器
				(*filter).state = (*filter).state + 1;
				break;	
	}

    // 计算当前输入项的贡献
	Output = (*filter).A1 * (*filter).Xk;
    
    // 循环处理所有历史输出项的贡献
	for (i = (*filter).Order; i > 0; i--)
	{
        // 累加历史输出项的贡献
		Output += ((*filter).Bk[i-1] * (*filter).Yk[i]);
        // 更新历史输出状态：将较新的状态移动到较旧的位置
		(*filter).Yk[i] = (*filter).Yk[i-1];
	}
    
    // 将当前输出保存为最新的历史状态
	(*filter).Yk[1] = Output;
    
    // 返回滤波后的输出值
	return Output;
}

