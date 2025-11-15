/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_pushrod_control.c
 * 文件标识： 
 * 内容摘要： 电推杆控制
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年10月05日
 *
 *******************************************************************************/


/*******************************************************************************
 * 直流电机状态机 - 状态转移表
 * 
 * 状态定义:
 * - STOPPED:        完全停止状态
 * - STOPPING:       正在停止（缓慢减速）
 * - STARTING_FORWARD: 正转启动（缓慢加速）
 * - RUNNING_FORWARD: 正转运行
 * - STARTING_REVERSE: 反转启动（缓慢加速） 
 * - RUNNING_REVERSE: 反转运行
 * - FAULT:          故障状态
 * 
 * 状态转移表:
 * 
 * 当前状态          条件                          下一状态          说明
 * ----------------------------------------------------------------------------------------------
 * STOPPED          防抖延时结束 &                    STARTING_FORWARD  开始正转缓启动
 *                 正转命令=1 & 正转限位=0
 * 
 * STOPPED          防抖延时结束 &                    STARTING_REVERSE  开始反转缓启动
 *                 反转命令=1 & 反转限位=0
 * 
 * STOPPED          其他情况                         STOPPED          保持停止状态
 * 
 * STARTING_FORWARD 正转限位=1                       STOPPING         限位触发，开始缓停止
 * 
 * STARTING_FORWARD 正转命令=0                       STOPPING         命令取消，开始缓停止
 * 
 * STARTING_FORWARD 加速时间完成                     RUNNING_FORWARD  加速完成，进入正转运行
 * 
 * STARTING_FORWARD 其他情况                         STARTING_FORWARD 继续加速过程
 * 
 * RUNNING_FORWARD  正转限位=1                       STOPPING         限位触发，开始缓停止
 * 
 * RUNNING_FORWARD  正转命令=0                       STOPPING         命令取消，开始缓停止
 * 
 * RUNNING_FORWARD  其他情况                         RUNNING_FORWARD  保持正转运行
 * 
 * STARTING_REVERSE 反转限位=1                       STOPPING         限位触发，开始缓停止
 * 
 * STARTING_REVERSE 反转命令=0                       STOPPING         命令取消，开始缓停止
 * 
 * STARTING_REVERSE 加速时间完成                     RUNNING_REVERSE  加速完成，进入反转运行
 * 
 * STARTING_REVERSE 其他情况                         STARTING_REVERSE 继续加速过程
 * 
 * RUNNING_REVERSE  反转限位=1                       STOPPING         限位触发，开始缓停止
 * 
 * RUNNING_REVERSE  反转命令=0                       STOPPING         命令取消，开始缓停止
 * 
 * RUNNING_REVERSE  其他情况                         RUNNING_REVERSE  保持反转运行
 * 
 * STOPPING         PWM值 ≤ 最小速度                STOPPED          减速完成，进入完全停止
 * 
 * STOPPING         其他情况                         STOPPING         继续减速过程
 * 
 * 任意状态(除FAULT) 正转命令=1 & 反转命令=1           FAULT            命令冲突，进入故障状态
 * 
 * 任意状态(除FAULT) 紧急停止=1                       STOPPED          紧急停止，立即刹车
 * 
 * FAULT            调用清除故障函数                  STOPPED          故障清除，返回停止状态
 * 
 * FAULT            其他情况                         FAULT            保持故障状态
 *************************************************************************************************/

#include "lm_pushrod_control.h"
#include "lm_pushrod_signal.h"
#include "lm_timer.h"






/* Includes ------------------------------------------------------------------*/
#include "lm_pushrod_control.h"

/* Private constants ---------------------------------------------------------*/

/**
  * @brief 电机默认配置参数
  */
static const MotorConfig DEFAULT_CONFIG = {
    .acceleration_time = 600,    // 1秒加速
    .deceleration_time = 600,    // 1秒减速
    .max_speed = 950,            // 最大PWM值
    .min_speed = 15,             // 最小启动PWM值
    .state_change_delay = 20     // 40ms状态切换延时
};

/* Private variables ---------------------------------------------------------*/

/**
  * @brief 电机状态机实例
  */
MotorStateMachine motor_handle;

/**
  * @brief 电机状态机指针
  */
static MotorStateMachine* motor = &motor_handle;

/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
函数名称：lm_calculate_ramp_pwm
功能描述：计算电机斜坡PWM值，实现平滑加速和减速控制    
输入参数：motor - 电机状态机指针，包含当前状态和配置参数
         current_time - 当前系统时间戳，用于计算时间间隔   
输出参数：无    
返 回 值：uint16_t - 计算得到的斜坡PWM值    
其它说明：该函数根据电机当前状态（启动、停止）计算线性斜坡PWM值
         实现电机的平滑加速和减速，避免急启急停造成的机械冲击			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/18     V1.00          Qiguo_Cui          创建
*******************************************************************************/
static uint16_t lm_calculate_ramp_pwm(MotorStateMachine* motor, uint32_t current_time) {
    /* 计算从状态开始到当前的时间间隔 */
    uint32_t elapsed_time = current_time - motor->state_entry_time;  // 计算当前状态持续时间（毫秒）
    
    /* 定义斜坡计算相关变量 */
    uint32_t ramp_time;                                              // 斜坡总时间（加速或减速时间）
    uint16_t start_pwm;                                              // 斜坡起始PWM值
    uint16_t end_pwm;                                                // 斜坡结束PWM值
    
    /* 根据电机当前状态确定斜坡参数 */
    switch (motor->current_state) {
        /* 正转启动状态处理 */
        case MOTOR_STARTING_FORWARD:                                 // 电机处于正转启动状态
        /* 反转启动状态处理 */    
        case MOTOR_STARTING_REVERSE:                                 // 电机处于反转启动状态
            ramp_time = motor->config.acceleration_time;             // 使用配置的加速时间作为斜坡时间
            start_pwm = motor->config.min_speed;                     // 从最小启动速度开始加速
            end_pwm = motor->config.max_speed;                       // 加速到最大运行速度
            break;                                                   // 跳出switch语句
            
        /* 停止状态处理 */    
        case MOTOR_STOPPING:                                         // 电机处于停止减速状态
            ramp_time = motor->config.deceleration_time;             // 使用配置的减速时间作为斜坡时间
            /* 根据先前运行状态确定起始PWM值 */
            start_pwm = motor->previous_state == MOTOR_RUNNING_FORWARD || 
                        motor->previous_state == MOTOR_RUNNING_REVERSE ? 
                        motor->config.max_speed : motor->pwm_value;  // 如果先前在运行则从最大速度开始减速，否则从当前速度开始
            end_pwm = motor->config.min_speed;                       // 减速到最小速度
            break;                                                   // 跳出switch语句
            
        /* 其他状态处理 */
        default:
            return motor->target_pwm;                                // 非斜坡状态直接返回目标PWM值
    }
    
    /* 检查斜坡时间是否已经完成 */
    if (elapsed_time >= ramp_time) {                                 // 如果持续时间超过斜坡总时间
        return end_pwm;                                              // 返回斜坡结束的目标PWM值
    }
    
    /* 线性插值计算当前斜坡PWM值 */
    int32_t pwm_range = end_pwm - start_pwm;                         // 计算PWM变化范围（结束值-起始值）
    return start_pwm + (pwm_range * elapsed_time) / ramp_time;       // 根据时间比例计算当前PWM值并返回
}

/* Exported functions --------------------------------------------------------*/

/*******************************************************************************
函数名称：lm_motor_init
功能描述：初始化电机状态机，设置默认参数和回调函数    
输入参数：无   
输出参数：无    
返 回 值：无    
其它说明：初始化后电机处于停止状态，刹车启用，PWM输出为0			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/18     V1.00          Qiguo_Cui          创建
*******************************************************************************/
void lm_motor_init() {
    motor->current_state = MOTOR_STOPPED;                            // 设置初始状态为停止
    motor->previous_state = MOTOR_STOPPED;                           // 设置先前状态为停止
    motor->config = DEFAULT_CONFIG;                                  // 加载默认配置
    
    // 初始化控制信号
    motor->signals.forward_cmd = false;                              // 正转命令初始化为false
    motor->signals.reverse_cmd = false;                              // 反转命令初始化为false
    motor->signals.forward_limit = false;                            // 正转限位初始化为false
    motor->signals.reverse_limit = false;                            // 反转限位初始化为false
    
    // 初始化内部状态变量
    motor->state_entry_time = 0;                                     // 状态进入时间清零
    motor->pwm_value = 0;                                            // PWM输出值清零
    motor->target_pwm = 0;                                           // 目标PWM值清零
    motor->emergency_stop = false;                                   // 紧急停止标志清零
    motor->pwmstate = MOTOR_PWM_Stop;                                // PWM状态设为停止
    
    // 设置硬件控制回调函数
    motor->set_pwm = lm_set_pwm;                                     // 设置PWM输出回调
    motor->set_direction = lm_set_direction;                         // 设置方向控制回调
    motor->brake = lm_set_brake;                                     // 设置刹车控制回调
    
    // 初始化硬件状态
    if (motor->brake) motor->brake(true);                            // 启用刹车
    if (motor->set_pwm) motor->set_pwm(0);                           // PWM输出设为0
}

/*******************************************************************************
函数名称：lm_motor_update_signals
功能描述：更新电机控制信号状态    
输入参数：无   
输出参数：无    
返 回 值：无    
其它说明：从外部信号数组读取命令和限位状态			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/18     V1.00          Qiguo_Cui          创建
*******************************************************************************/
void lm_motor_update_signals() {
    motor->signals.forward_cmd   =  PushActuatorCommandCurrentSignal[0];  // 更新正转命令信号
    motor->signals.reverse_cmd   =  PushActuatorCommandCurrentSignal[1];  // 更新反转命令信号
    //motor->signals.forward_limit =  PushActuatorCommandCurrentSignal[2];  // 更新正转限位信号
    //motor->signals.reverse_limit =  PushActuatorCommandCurrentSignal[3];  // 更新反转限位信号
}

/*******************************************************************************
函数名称：lm_motor_emergency_stop
功能描述：执行电机紧急停止操作    
输入参数：motor - 电机状态机指针   
输出参数：无    
返 回 值：无    
其它说明：立即停止电机，启用刹车，PWM输出为0			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/18     V1.00          Qiguo_Cui          创建
*******************************************************************************/
void lm_motor_emergency_stop(MotorStateMachine* motor) {
    motor->emergency_stop = 1;                                       // 设置紧急停止标志
    motor->target_pwm = 0;                                           // 目标PWM设为0
    
    if (motor->brake) {
        motor->brake(1);                                             // 启用刹车
    }
    if (motor->set_pwm) {
        motor->set_pwm(0);                                           // PWM输出设为0
    }
}

/*******************************************************************************
函数名称：lm_motor_clear_fault
功能描述：清除电机故障状态    
输入参数：motor - 电机状态机指针   
输出参数：无    
返 回 值：无    
其它说明：只有在故障状态时才能清除			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/18     V1.00          Qiguo_Cui          创建
*******************************************************************************/
void lm_motor_clear_fault(MotorStateMachine* motor) {
    if (motor->current_state == MOTOR_FAULT) {                       // 检查当前是否为故障状态
        motor->current_state = MOTOR_STOPPED;                        // 清除故障，设为停止状态
        motor->emergency_stop = 0;                                   // 清除紧急停止标志
    }
}

/*******************************************************************************
函数名称：lm_motor_run_state_machine
功能描述：运行电机状态机，根据输入信号更新状态和PWM输出    
输入参数：current_time - 当前时间戳   
输出参数：无    
返 回 值：无    
其它说明：电机控制的核心状态机函数			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/18     V1.00          Qiguo_Cui          创建
*******************************************************************************/
void lm_motor_run_state_machine(uint32_t current_time) {
    // 紧急停止优先处理
    if (motor->emergency_stop) {                                     // 检查紧急停止标志
        if (motor->current_state != MOTOR_STOPPED && motor->current_state != MOTOR_FAULT) {
            motor->previous_state = motor->current_state;            // 保存先前状态
            motor->current_state = MOTOR_STOPPED;                    // 切换到停止状态
            motor->state_entry_time = current_time;                  // 更新状态进入时间
            motor->pwm_value = 0;                                    // PWM值清零
            if (motor->set_pwm) motor->set_pwm(0);                   // 硬件PWM输出清零
            if (motor->brake) motor->brake(true);                    // 启用刹车
        }
        return;                                                      // 紧急停止时直接返回
    }
    
    // 检查命令冲突（正转和反转同时有效）
    if (motor->signals.forward_cmd && motor->signals.reverse_cmd) {  // 检查正反转命令是否冲突
        motor->current_state = MOTOR_FAULT;                          // 切换到故障状态
        motor->state_entry_time = current_time;                      // 更新状态进入时间
        motor->pwm_value = 0;                                        // PWM值清零
        if (motor->set_pwm) motor->set_pwm(0);                       // 硬件PWM输出清零
        if (motor->brake) motor->brake(true);                        // 启用刹车
        return;                                                      // 故障状态直接返回
    }
    
    // 状态处理
    MotorState next_state = motor->current_state;                    // 初始化下一状态为当前状态
    bool state_changed = false;                                      // 状态变化标志
     
    // 状态机主逻辑
    switch (motor->current_state) {
        case MOTOR_STOPPED:                                          // 停止状态
            if (current_time - motor->state_entry_time >= motor->config.state_change_delay) {
                if (motor->signals.forward_cmd && !motor->signals.forward_limit) {
                    next_state = MOTOR_STARTING_FORWARD;             // 条件满足，切换到正转启动
                    state_changed = true;                            // 设置状态变化标志
                } else if (motor->signals.reverse_cmd && !motor->signals.reverse_limit) {
                    next_state = MOTOR_STARTING_REVERSE;             // 条件满足，切换到反转启动
                    state_changed = true;                            // 设置状态变化标志
                }
            }
            break;
            
        case MOTOR_STARTING_FORWARD:                                 // 正转启动状态
            if (motor->signals.forward_limit) {                      // 正转限位触发
                next_state = MOTOR_STOPPING;                         // 切换到停止状态
                state_changed = true;                                // 设置状态变化标志
            } else if (!motor->signals.forward_cmd) {                // 正转命令取消
                next_state = MOTOR_STOPPING;                         // 切换到停止状态
                state_changed = true;                                // 设置状态变化标志
            } else {
                // 检查是否加速完成
                uint32_t elapsed = current_time - motor->state_entry_time;  // 计算加速时间
                if (elapsed >= motor->config.acceleration_time) {    // 检查加速时间是否达到
                    next_state = MOTOR_RUNNING_FORWARD;              // 切换到正转运行状态
                    state_changed = true;                            // 设置状态变化标志
                }
            }
            break;
            
        case MOTOR_RUNNING_FORWARD:                                  // 正转运行状态
            if (motor->signals.forward_limit || !motor->signals.forward_cmd) {
                next_state = MOTOR_STOPPING;                         // 限位触发或命令取消，切换到停止
                state_changed = true;                                // 设置状态变化标志
            }
            break;
            
        case MOTOR_STARTING_REVERSE:                                 // 反转启动状态
            if (motor->signals.reverse_limit) {                      // 反转限位触发
                next_state = MOTOR_STOPPING;                         // 切换到停止状态
                state_changed = true;                                // 设置状态变化标志
            } else if (!motor->signals.reverse_cmd) {                // 反转命令取消
                next_state = MOTOR_STOPPING;                         // 切换到停止状态
                state_changed = true;                                // 设置状态变化标志
            } else {
                // 检查是否加速完成
                uint32_t elapsed = current_time - motor->state_entry_time;  // 计算加速时间
                if (elapsed >= motor->config.acceleration_time) {    // 检查加速时间是否达到
                    next_state = MOTOR_RUNNING_REVERSE;              // 切换到反转运行状态
                    state_changed = true;                            // 设置状态变化标志
                }
            }
            break;
            
        case MOTOR_RUNNING_REVERSE:                                  // 反转运行状态
            if (motor->signals.reverse_limit || !motor->signals.reverse_cmd) {
                next_state = MOTOR_STOPPING;                         // 限位触发或命令取消，切换到停止
                state_changed = true;                                // 设置状态变化标志
            }
            break;
            
        case MOTOR_STOPPING:                                         // 停止中状态
            // 检查减速是否完成
            if (motor->pwm_value <= motor->config.min_speed) {       // 检查PWM是否降到最小速度
                next_state = MOTOR_STOPPED;                          // 切换到完全停止状态
                state_changed = true;                                // 设置状态变化标志
            }
            break;
            
        case MOTOR_FAULT:                                            // 故障状态
            // 故障状态需要外部清除，不自动转换
            break;
    }
    
    // 状态转换处理
    if (state_changed) {                                             // 检查状态是否发生变化
        motor->previous_state = motor->current_state;                // 保存先前状态
        motor->current_state = next_state;                           // 更新当前状态
        motor->state_entry_time = current_time;                      // 更新状态进入时间
        
        // 执行状态进入动作
        switch (next_state) {
            case MOTOR_STOPPED:                                      // 进入停止状态
                motor->target_pwm = 0;                               // 目标PWM设为0
                if (motor->brake) motor->brake(true);                // 启用刹车
                if (motor->set_direction) motor->set_direction(MOTOR_PWM_Stop);  // 设置方向为停止
                if (motor->set_pwm) motor->set_pwm(0);               // PWM输出设为0
                break;
                
            case MOTOR_STARTING_FORWARD:                             // 进入正转启动状态
                if (motor->set_direction) motor->set_direction(MOTOR_PWM_Forward);  // 设置正转方向
                if (motor->brake) motor->brake(false);               // 释放刹车
                motor->target_pwm = motor->config.max_speed;         // 目标PWM设为最大速度
                break;
                
            case MOTOR_RUNNING_FORWARD:                              // 进入正转运行状态
                motor->target_pwm = motor->config.max_speed;         // 目标PWM设为最大速度
                break; 
                
            case MOTOR_STARTING_REVERSE:                             // 进入反转启动状态
                if (motor->set_direction) motor->set_direction(MOTOR_PWM_Reverse);  // 设置反转方向
                if (motor->brake) motor->brake(false);               // 释放刹车
                motor->target_pwm = motor->config.max_speed;         // 目标PWM设为最大速度
                break;
                
            case MOTOR_RUNNING_REVERSE:                              // 进入反转运行状态
                motor->target_pwm = motor->config.max_speed;         // 目标PWM设为最大速度
                break;
                
            case MOTOR_STOPPING:                                     // 进入停止中状态
                motor->target_pwm = 0;                               // 目标PWM设为0
                break;
                
            default:
                break;
        }
    }
    
    // 更新PWM输出（缓启动/缓停止）
    if (motor->current_state == MOTOR_STARTING_FORWARD || 
        motor->current_state == MOTOR_STARTING_REVERSE ||
        motor->current_state == MOTOR_STOPPING) {                    // 检查是否处于斜坡状态
        
        motor->pwm_value = lm_calculate_ramp_pwm(motor, current_time);  // 计算斜坡PWM值
    } else if (motor->current_state == MOTOR_RUNNING_FORWARD || 
               motor->current_state == MOTOR_RUNNING_REVERSE) {      // 检查是否处于运行状态
        motor->pwm_value = motor->target_pwm;                        // 直接使用目标PWM值
    }
    
    // 检查PWM回调函数是否有效
    if(!motor->set_pwm) {
        return;                                                      // 回调函数无效，直接返回
    }
    
    // 根据状态设置硬件PWM输出
    if (motor->current_state == MOTOR_STARTING_FORWARD || motor->current_state == MOTOR_RUNNING_FORWARD) {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, motor->pwm_value);  // 正转通道输出PWM
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);                 // 反转通道输出0
    } else if(motor->current_state == MOTOR_STARTING_REVERSE || motor->current_state == MOTOR_RUNNING_REVERSE) {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);                 // 正转通道输出0
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, motor->pwm_value);  // 反转通道输出PWM
    } else if(motor->current_state == MOTOR_STOPPING && (motor->previous_state == MOTOR_STARTING_FORWARD ||  
             motor->previous_state == MOTOR_RUNNING_FORWARD)) {      // 正转方向减速
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, motor->pwm_value);  // 正转通道输出斜坡PWM
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);                 // 反转通道输出0
    } else if(motor->current_state == MOTOR_STOPPING && (motor->previous_state == MOTOR_STARTING_REVERSE || 
             motor->previous_state == MOTOR_RUNNING_REVERSE)) {      // 反转方向减速
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);                 // 正转通道输出0
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, motor->pwm_value);  // 反转通道输出斜坡PWM
    } else if(motor->current_state == MOTOR_STOPPED) {               // 完全停止状态
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);                 // 正转通道输出0
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);                 // 反转通道输出0
    }
		
		// 采样频率
		if(motor->pwm_value == 0)	
		{
			__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, 500); 
		}
		else
		{
			__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, motor->pwm_value);    
		}
}

/*******************************************************************************
函数名称：lm_motor_get_current_state
功能描述：获取电机当前状态    
输入参数：motor - 电机状态机指针   
输出参数：无    
返 回 值：MotorState - 电机当前状态枚举值    
其它说明：			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/18     V1.00          Qiguo_Cui          创建
*******************************************************************************/
MotorState lm_motor_get_current_state(MotorStateMachine* motor) {
    return motor->current_state;                                     // 返回当前状态
}

/*******************************************************************************
函数名称：lm_motor_get_current_pwm
功能描述：获取电机当前PWM输出值    
输入参数：motor - 电机状态机指针   
输出参数：无    
返 回 值：uint16_t - 当前PWM输出值    
其它说明：			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/18     V1.00          Qiguo_Cui          创建
*******************************************************************************/
uint16_t lm_motor_get_current_pwm(MotorStateMachine* motor) {
    return motor->pwm_value;                                         // 返回当前PWM值
}

/*******************************************************************************
函数名称：lm_set_direction
功能描述：设置电机转向方向    
输入参数：pwmstates - 转向方向枚举值   
输出参数：无    
返 回 值：无    
其它说明：			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/18     V1.00          Qiguo_Cui          创建
*******************************************************************************/
void lm_set_direction(Motor_pwm_state pwmstates) {
    motor->pwmstate = pwmstates;                                     // 更新PWM状态
}

/*******************************************************************************
函数名称：lm_set_brake
功能描述：设置电机制动状态    
输入参数：enable - 制动使能标志（true启用制动，false释放制动）   
输出参数：无    
返 回 值：无    
其它说明：当前版本为空实现，需要根据具体硬件完善			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/18     V1.00          Qiguo_Cui          创建
*******************************************************************************/
void lm_set_brake(bool enable) {
    if (enable) {
        ;                                                           // 启用制动（待实现）
    } else {
        ;                                                           // 释放制动（待实现）
    }
}
/*******************************************************************************
函数名称：lm_set_pwm
功能描述：设置电机pwm数值  
输入参数: pwm数值处理  
输出参数：无    
返 回 值：无    
其它说明：当前版本为空实现，需要根据具体硬件完善			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/18     V1.00          Qiguo_Cui          创建
*******************************************************************************/
void lm_set_pwm(uint16_t pwm_value)
{
		;
}





