/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_common_function.h
 * 文件标识： 
 * 内容摘要： 常用处理函数
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年10月22日
 *
 *******************************************************************************/
#ifndef LM_COMMON_FUNCTION_H
#define LM_COMMON_FUNCTION_H

int lm_bit_divide(int byte_data, int bit_loc);                 
int lm_byte_divide_high_16(int byte_comb);
int lm_byte_divide_low_16(int byte_comb);
 
int lm_byte_combine(int low_byte, int high_byte);
double lm_period_normalize(double value, double period);
double lm_period_normalize_symmetric(double value, double period);
double lm_angle_normalize_0_to_2pi(double angle_rad);
double lm_angle_normalize_neg_pi_to_pi(double angle_rad);

float lm_linear_function(float y1, float y2, float x1, float x2, float x);
double lm_linear_interpolation(double x0, double y0, double x1, double y1, double x);   
double lm_clamp(double value, double min_value,double max_value);        

#endif
