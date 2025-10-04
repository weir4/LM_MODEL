/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_16crc.h
 * 文件标识： 
 * 内容摘要： crc校验
 * 其它说明： 无
 * 当前版本： FreeRTOS V9.0.0
 * 作    者：    Qiguo_Cui                   
 * 完成日期： 2025年09月01日
 *
 *******************************************************************************/


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef LM_16CRC_H
#define LM_16CRC_H

/* Includes ------------------------------------------------------------------*/
#include "sys.h"
#include <stdint.h>
#include <stddef.h>

/* Exported types ------------------------------------------------------------*/
// 无导出类型

/* Exported constants --------------------------------------------------------*/
// 无导出常量

/* Exported macro ------------------------------------------------------------*/
// 无导出宏

/* Exported functions --------------------------------------------------------*/
uint8_t lm_crc16_reversebyte(uint8_t byte);
uint16_t lm_crc16_reverse16(uint16_t value);
void lm_crc16_generate_modbus_table(void);
uint16_t lm_crc16_modbus(const uint8_t *data, size_t length);
void lm_get_crcbytes(uint16_t crc, uint8_t *high_byte, uint8_t *low_byte);

#endif /* LM_16CRC_H */


