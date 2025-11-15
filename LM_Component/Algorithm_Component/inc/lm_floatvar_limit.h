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

#ifndef LM_FLOATVAR_LIMIT_H
#define LM_FLOATVAR_LIMIT_H



/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LM_FLOATVARLIMIT_H
#define __LM_FLOATVARLIMIT_H

/* Includes ------------------------------------------------------------------*/
#include "lm_staticmanager.h"
#include "lm_mallocmanager.h"
#include <math.h>

/* Exported types ------------------------------------------------------------*/

/**
  * @brief 浮点变量限制器结构体定义
  * @note 该结构体用于存储变量限制器的状态和参数，采用4字节对齐
  */
typedef struct __attribute__((aligned(4))) B{
    float last_output;      // 上一周期输出值
    float max_rise_rate;    // 最大上升率（单位/ms）
    float max_fall_rate;    // 最大下降率（单位/ms）
    float task_period;      // 任务周期（ms）
    bool initialized;       // 初始化标志
}fvarlimit_t;

/* Exported constants --------------------------------------------------------*/

/**
  * @brief 信号任务大小限制定义
  * @note 每个浮点变量限制器任务占用的内存大小（字节）
  */
#define SIGNALTASKSIZE_LIMIT 20

/**
  * @brief 任务数量限制定义  
  * @note 系统中最多可同时存在的浮点变量限制器任务数量
  */
#define TASKCOUNT_LIMIT      10

/**
  * @brief 内存对齐限制定义
  * @note 内存地址对齐要求，确保内存访问效率
  */
#define MEMORY_ALIGNMENT_LIMIT 4

/**
  * @brief 总内存大小限制定义
  * @note 静态内存池的总大小，计算公式：任务大小×任务数量+对齐填充
  */
#define TOTAL_MEMORY_SIZE_LIMIT     (SIGNALTASKSIZE_LIMIT*TASKCOUNT_LIMIT + MEMORY_ALIGNMENT_LIMIT)

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  初始化浮点变量限制模块的静态内存池
  * @param  lm_function_name: 函数名称描述字符串，用于标识内存池功能
  * @param  memory_pools: 指向静态内存池结构体的指针
  * @retval 无
  */
void lm_floatvarlimit_Init(char *lm_function_name, static_memory_pool_t *memory_pools);

/**
  * @brief  注册并初始化一个浮点变量限制器任务
  * @param  taskname: 任务名称字符串，用于标识和分配内存
  * @param  initial_value: 初始输出值，限制器的起始输出值
  * @param  tskperiod: 任务周期时间，用于计算每个周期的最大变化量
  * @param  max_rise_rate: 最大上升速率，单位时间内允许的最大正向变化率
  * @param  max_fall_rate: 最大下降速率，单位时间内允许的最大负向变化率
  * @retval fvarlimit_t*: 成功时返回限制器指针，失败时返回NULL
  */
fvarlimit_t *lm_floatvarlimit_register(char *taskname, float initial_value, float tskperiod, float max_rise_rate, float max_fall_rate);

/**
  * @brief  处理浮点变量限制器的输入信号
  * @param  fvarlimit: 指向浮点变量限制器结构体的指针
  * @param  input: 输入信号值，需要进行变化率限制的原始输入
  * @retval float: 经过变化率限制处理后的输出信号值
  */
float lm_floatvarlimit_process(fvarlimit_t *fvarlimit, float input);

#endif /* __LM_FLOATVARLIMIT_H */

/* Private types -------------------------------------------------------------*/
/* 私有类型定义区域 - 当前模块无私有类型 */

/* Private variables ---------------------------------------------------------*/
/* 私有变量定义区域 - 当前模块无私有变量 */

/* Private constants ---------------------------------------------------------*/
/* 私有常量定义区域 - 当前模块无私有常量 */

/* Private macros ------------------------------------------------------------*/
/* 私有宏定义区域 - 当前模块无私有宏 */

/* Private functions ---------------------------------------------------------*/
/* 私有函数声明区域 - 当前模块无私有函数 */


#endif
