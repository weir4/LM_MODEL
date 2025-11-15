/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_pid.h
 * 文件标识： 
 * 内容摘要： pid 控制模块
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年10月22日
 *
 *******************************************************************************/


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef LM_PID_H
#define LM_PID_H

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <math.h>
#include "lm_staticmanager.h"
#include "lm_mallocmanager.h"

/* Exported constants --------------------------------------------------------*/
/* PID控制器任务内存块大小定义 */
#define SIGNALTASKSIZE_PID 52          /* 单个PID控制器实例占用的内存大小（字节） */

/* PID控制器最大实例数量定义 */  
#define TASKCOUNT_PID      10          /* 系统支持的最大PID控制器实例数量 */

/* PID控制器内存对齐要求定义 */
#define MEMORY_ALIGNMENT_PID 4         /* 内存对齐字节数，确保内存访问效率 */

/* PID控制器总内存大小定义 */
#define TOTAL_MEMORY_SIZE_PID     (SIGNALTASKSIZE_PID*TASKCOUNT_PID + MEMORY_ALIGNMENT_PID)
                                      /* 计算PID控制器模块所需的总内存大小，包含对齐填充 */

/* Exported types ------------------------------------------------------------*/
typedef struct  __attribute__((aligned(4))) 
{
    /* PID基本控制参数 */
    float Kp;           /* 比例系数：决定系统对当前误差的反应强度，值越大响应越快但可能超调 */
    float Ki;           /* 积分系数：用于消除稳态误差，值越大消除速度越快但可能引起振荡 */  
    float Kd;           /* 微分系数：预测误差变化趋势，提高系统稳定性，抑制超调 */
    
    /* 控制器运行状态变量 */
    float error[3];     /* 误差队列：存储当前和历史的误差值
                         * [0] - 当前误差e(k)：本次采样周期的误差值
                         * [1] - 上一次误差e(k-1)：前一个采样周期的误差值  
                         * [2] - 上上次误差e(k-2)：前两个采样周期的误差值 */
    float output_prev;  /* 上一次输出值：记录前一个控制周期的输出，用于增量式计算 */
    
    /* 输出限幅保护参数 */
    float output_min;   /* 输出最小值：限制控制器输出下限，保护执行机构反向不过载 */
    float output_max;   /* 输出最大值：限制控制器输出上限，保护执行机构正向不过载 */
    
    /* 抗积分饱和和性能优化参数 */
    float error_threshold;  /* 积分分离阈值：当误差绝对值小于此值时启用积分作用，避免大误差时积分导致超调 */
    float delta_max;        /* 最大输出增量限制：限制单步输出变化的最大值，提高系统平稳性 */
    float integral_limit;   /* 积分作用限制：限制积分项的最大值，防止积分饱和现象 */
    
    /* 系统运行参数 */
    float dt;           /* 采样时间：控制算法的执行周期，单位秒，必须大于零 */
} PID_t;

/* Exported functions --------------------------------------------------------*/
void lm_pid_Init(char *lm_function_name, static_memory_pool_t *memory_pools);

PID_t *lm_pid_register(char * taskname, float Kp, float Ki, float Kd, float output_min, float output_max, float error_threshold, float delta_max, float dt);
float lm_pid_process(PID_t *pid, float target, float measurement);

#endif /* LM_PID_H */

