/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_kalman_one_dim.h
 * 文件标识： 
 * 内容摘要： 油箱信号处理
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者：    Qiguo_Cui                   
 * 完成日期： 2025年09月01日
 *
 *******************************************************************************/

#ifndef LM_KALMAN_ONE_DIM_H
#define LM_KALMAN_ONE_DIM_H

#include <stdio.h>
#include <math.h>
#include "lm_staticmanager.h"
#include "lm_mallocmanager.h"


/* Exported constants --------------------------------------------------------*/
/* PID控制器任务内存块大小定义 */
#define SIGNALTASKSIZE_KALMAN_ONEDIM   20          /* 单个PID控制器实例占用的内存大小（字节） */

/* PID控制器最大实例数量定义 */  
#define TASKCOUNT_KALMAN_ONEDIM        5          /* 系统支持的最大PID控制器实例数量 */

/* PID控制器内存对齐要求定义 */
#define MEMORY_ALIGNMENT_KALMAN_ONEDIM 4         /* 内存对齐字节数，确保内存访问效率 */

/* PID控制器总内存大小定义 */
#define TOTAL_MEMORY_SIZE_KALMAN_ONEDIM     (SIGNALTASKSIZE_KALMAN_ONEDIM*TASKCOUNT_KALMAN_ONEDIM + MEMORY_ALIGNMENT_KALMAN_ONEDIM)
                                      /* 计算PID控制器模块所需的总内存大小，包含对齐填充 */



typedef struct {
    float q;        // 过程噪声协方差
    float r;        // 测量噪声协方差
    float x;        // 系统状态值（估计的油位）
    float p;        // 状态估计协方差
    float k;        // 卡尔曼增益
} FuelKalmanFilter_t;


void lm_kalman_onedim_Init(char *lm_function_name, static_memory_pool_t *memory_pools);
FuelKalmanFilter_t *lm_kalman_onedim_register(char * taskname,  float q, float r, float initial_value, float initial_p);
float lm_kalman_onedim_process(FuelKalmanFilter_t* kf, float measurement);


#endif 


