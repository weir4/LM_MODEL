#ifndef LM_485_H
#define LM_485_H

/* Includes ------------------------------------------------------------------*/
#include "sys.h"

/* Exported types ------------------------------------------------------------*/
// 无导出类型



#define GET_BITS(value, start, count) (((value) >> (start)) & ((1U << (count)) - 1))

typedef union {
    struct {
        uint8_t bit0 : 1;  // 第0位 (LSB - 最低有效位)
        uint8_t bit1 : 1;  // 第1位
        uint8_t bit2 : 1;  // 第2位
        uint8_t bit3 : 1;  // 第3位
        uint8_t bit4 : 1;  // 第4位
        uint8_t bit5 : 1;  // 第5位
        uint8_t bit6 : 1;  // 第6位
        uint8_t bit7 : 1;  // 第7位 (MSB - 最高有效位)
    } bits;
    uint8_t value;         // 完整的字节值
} ByteBits_t;

//extern RS485_Status status;

#define RX_BUFFER_SIZE       40          // 接收缓冲区字节数据
#define TX_BUFFER_SIZE       8           // 固定本机地址码

// 超时定义
#define RS485_TX_TIMEOUT      300     // 发送超时(ms)
#define RS485_TX_TIMECIRC     100     // 接收超时(ms)

typedef enum {
	 RS485_ADDRESS        =  0x10,       // 本机地址
	 RS485_READ_FUNC_CODE =  0x03,       // 读取功能码
	 RS485_StartAddress   =  0x0A00,     // 起始地址
	 RS485_AddressNumber  =  0x000D,     // 寄存器数量
}RS485_Send;


typedef enum {
    RS485_OK = 0x00,                 // 操作成功 
    // 通信错误
    RS485_ERR_TX_FAILED = 0x10,      // 发送失败
    RS485_ERR_RX_TIMEOUT = 0x11,     // 接收超时
    RS485_ERR_BUSY = 0x12,           // 模块忙
    
    // 帧格式错误
    RS485_ERR_FRAME_TOO_SHORT = 0x20, // 帧长度过短
    RS485_ERR_FRAME_TOO_LONG = 0x21,  // 帧长度过长
    RS485_ERR_INVALID_LENGTH = 0x22,  // 无效数据长度
    
    // 校验错误
    RS485_ERR_CRC_MISMATCH = 0x30,   // CRC校验失败
    RS485_ERR_ADDR_MISMATCH = 0x31,  // 地址不匹配
    RS485_ERR_FUNC_MISMATCH = 0x32,  // 功能码不匹配
    
    // 硬件错误
    RS485_ERR_UART_ERROR = 0x40,     // UART错误
    RS485_ERR_DMA_ERROR = 0x41,      // DMA错误
} LM_RS485_Status_t;


//extern LM_RS485_Status_t status;

typedef struct {
    uint8_t slave_addr;      // 从机地址
    uint8_t func_code;       // 功能码
    uint8_t byte_count;      // 数据字节数
    uint8_t *data;           // 数据指针
    uint16_t data_length;    // 数据长度
    uint16_t crc;            // CRC值
    LM_RS485_Status_t status;     // 解析状态
} LM_RS485_Responsestatus_t;



typedef struct{
	uint16_t  I_MAX_Discharge;
	uint16_t  V_MAX_Recharge;
	uint16_t  I_MAX_Recharge;
	uint16_t  I_MAX_Feedback;
  uint16_t  V_TOTAL_BatteryPack;
  uint16_t  Fault_Code_0x0A08;
  uint16_t  Fault_Code_0x0A09;
	uint16_t  Fault_Code_0x0A0A;
	uint16_t  Fault_Code_0x0A0B;
	uint16_t  Battery_Charging_Status;
}Battery_Param_t;


void lm_rs485_init(u32 bound);
void lm_rs485_tx_set(u8 en);
void lm_rs485_chipselec(void);
void lm_rs485_chipSetLevel(GPIO_PinState state);

static uint16_t lm_rs485_buildreadrequestframe(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count);
uint8_t lm_rs485_isresponseready(void);
LM_RS485_Status_t lm_rs485_sendreadrequest(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count);
LM_RS485_Status_t lm_rs485_parseresponse(LM_RS485_Responsestatus_t *response);
static LM_RS485_Status_t lm_rs485_verifyframecrc(uint8_t *data, uint16_t length);
void lm_rs485_clearresponseflag(void);
void lm_rs485_dataparsing(LM_RS485_Responsestatus_t *data);
void lm_rs485_DealData(void);
void lm_rs485_DataPara(uint8_t *data,uint8_t lenth);

uint16_t merge_bytes_big_endian(uint8_t high_byte, uint8_t low_byte);
#endif

