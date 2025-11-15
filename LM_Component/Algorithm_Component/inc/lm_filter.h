/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_filter.h
 * 文件标识： 
 * 内容摘要： 数字滤波器
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年10月22日
 *
 *******************************************************************************/


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef LM_FILTER_H
#define LM_FILTER_H
#include "lm_staticmanager.h"
#include "lm_mallocmanager.h"
/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/

#define SIGNALTASKSIZE_FILTER 32
#define TASKCOUNT_FILTER      10
#define MEMORY_ALIGNMENT_FILTER 4
#define TOTAL_MEMORY_SIZE_FILTER     (SIGNALTASKSIZE_FILTER*TASKCOUNT_FILTER + MEMORY_ALIGNMENT_FILTER)




typedef struct __attribute__((aligned(4))) A
{
    float   Xk;         /*!< 当前时刻输入信号值 */
    float   Yk[4];      /*!< 历史输出信号值数组，存储前几个时刻的输出 */
    int     Order;      /*!< 滤波器阶数，取值范围0-3 */
    int     state;      /*!< 滤波器运行状态标识 */
    float   CutOffF;    /*!< 滤波器截止频率，单位Hz */
    float   Ts;         /*!< 采样周期，单位秒 */
    float   A1;         /*!< 前向通道系数，对应当前输入 */
    float   Bk[3];      /*!< 反馈通道系数数组，对应历史输出 */
} Filter_t;

/* Exported constants --------------------------------------------------------*/
/* 无导出常量 */

/* Exported macro ------------------------------------------------------------*/
/* 无导出宏 */

/* Exported functions --------------------------------------------------------*/

void lm_filterBW_Init(char *lm_function_name,static_memory_pool_t *memory_pools);

Filter_t *lm_filterBW_register(char *taskname, int order, 
                              float cutiffF, float tskperiod);
float lm_filterBW_process(Filter_t *filter, float input);

/* Private types -------------------------------------------------------------*/
/* 无私有类型 */

/* Private variables ---------------------------------------------------------*/
/* 无私有变量 */

/* Private constants ---------------------------------------------------------*/
/* 无私有常量 */

/* Private macros ------------------------------------------------------------*/
/* 无私有宏 */

/* Private functions ---------------------------------------------------------*/
/* 无私有函数 */

#endif /* LM_FILTER_H */


