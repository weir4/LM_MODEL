/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_pushrod_control.h
 * 文件标识： 
 * 内容摘要： 电推杆控制
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年10月05日
 *
 *******************************************************************************/



#ifndef LM_PUSHROD_CONTROL_H
#define LM_PUSHROD_CONTROL_H

/* Includes ------------------------------------------------------------------*/
#include "sys.h"
#include <stdbool.h>

/* Exported types ------------------------------------------------------------*/

/**
  * @brief 电机运行状态枚举
  */
typedef enum {
    MOTOR_STOPPED = 0,           // 完全停止
    MOTOR_STOPPING,              // 正在停止（缓慢减速）
    MOTOR_STARTING_FORWARD,      // 正转启动（缓慢加速）
    MOTOR_RUNNING_FORWARD,       // 正转运行
    MOTOR_STARTING_REVERSE,      // 反转启动（缓慢加速）
    MOTOR_RUNNING_REVERSE,       // 反转运行
    MOTOR_FAULT                  // 故障状态
} MotorState;

/**
  * @brief PWM输出状态枚举
  */
typedef enum {
    MOTOR_PWM_Forward = 0,
    MOTOR_PWM_Reverse,
    MOTOR_PWM_Stop,
} Motor_pwm_state;

/**
  * @brief 电机控制信号结构体
  */
typedef struct {
    bool forward_cmd;           // 正转命令
    bool reverse_cmd;           // 反转命令
    bool forward_limit;         // 正转限位（true表示触发限位）
    bool reverse_limit;         // 反转限位（true表示触发限位）
} MotorSignals;

/**
  * @brief 电机配置参数结构体
  */
typedef struct {
    uint32_t acceleration_time;  // 加速时间 (ms)
    uint32_t deceleration_time;  // 减速时间 (ms)
    uint16_t max_speed;          // 最大速度 (PWM值)
    uint16_t min_speed;          // 最小启动速度 (PWM值)
    uint32_t state_change_delay; // 状态切换延时 (ms，防抖)
} MotorConfig;

/**
  * @brief 电机状态机结构体
  */
typedef struct {
    MotorState current_state;
    MotorState previous_state;
    MotorSignals signals;
    MotorConfig config;
    
    // 内部状态变量
    uint32_t state_entry_time;
    uint32_t pwm_value;
    uint32_t target_pwm;
    uint8_t emergency_stop;
    Motor_pwm_state pwmstate;
    
    // 硬件控制回调函数指针
    void (*set_pwm)(uint16_t pwm);
    void (*set_direction)(Motor_pwm_state forward);
    void (*brake)(bool enable);
} MotorStateMachine;

/* Exported functions --------------------------------------------------------*/

void lm_motor_init(void);
void lm_motor_update_signals(void);
void lm_motor_run_state_machine(uint32_t current_time);
void lm_motor_emergency_stop(MotorStateMachine* motor);
void lm_motor_clear_fault(MotorStateMachine* motor);
MotorState lm_motor_get_current_state(MotorStateMachine* motor);
uint16_t lm_motor_get_current_pwm(MotorStateMachine* motor);
void lm_set_direction(Motor_pwm_state pwmstates);
void lm_set_pwm(uint16_t pwm_value);
void lm_set_brake(bool enable);

#endif /* LM_PUSHROD_CONTROL_H */
