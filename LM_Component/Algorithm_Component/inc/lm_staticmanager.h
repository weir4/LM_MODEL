/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_staticmanager.h
 * 文件标识： 
 * 内容摘要： 静态空间管理
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者：    Qiguo_Cui                   
 * 完成日期： 2025年09月01日
 *
 *******************************************************************************/


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef LM_STATICMANAGER_H
#define LM_STATICMANAGER_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Exported constants --------------------------------------------------------*/

#define  BLOCK_COUNT 10

typedef enum {
    MEM_STATIC_FREE = 0,       /*!< 内存块空闲，可供分配 */
    MEM_STATIC_USED = 1,       /*!< 内存块已使用，已被分配 */
    MEM_STATIC_RESERVED = 2    /*!< 内存块保留，不可分配 */
} block_static_state_t;


/* Exported macro ------------------------------------------------------------*/

#define ALIGN_UP(addr, align) (((uintptr_t)(addr) + (align) - 1) & ~((align) - 1))

#define ALIGN_DOWN(addr, align) ((uintptr_t)(addr) & ~((align) - 1))

#define IS_ALIGNED(addr, align) (((uintptr_t)(addr) & ((align) - 1)) == 0)

/* Exported types ------------------------------------------------------------*/

typedef struct memory_block {
    uint8_t* address;          /*!< 内存块起始地址（已对齐） */
    size_t size;               /*!< 内存块标称大小 */
    size_t actual_size;        /*!< 内存块实际大小（包含对齐填充） */
    block_static_state_t state;       /*!< 内存块当前状态 */
    uint32_t block_id;         /*!< 内存块唯一标识符 */
    uint32_t owner_id;         /*!< 当前使用者标识符 */
    char description[32];      /*!< 内存块使用描述信息 */
} memory_block_t;

typedef struct static_memory_pool {
    uint8_t* base_address;            /*!< 内存池原始基地址（未对齐） */
    uint8_t* aligned_address;         /*!< 对齐后的内存池起始地址 */
    size_t   total_size;              /*!< 内存池总分配大小 */
    size_t   usable_size;             /*!< 内存池实际可用大小（对齐后） */
    size_t   block_size;              /*!< 每个内存块的大小 */
    uint32_t block_count;             /*!< 内存池中块的总数量 */
    
    memory_block_t blocks[BLOCK_COUNT]; /*!< 内存块描述符数组 */
    uint32_t used_blocks;               /*!< 当前已使用的块数 */
    uint32_t free_blocks;               /*!< 当前空闲的块数 */
    uint32_t next_owner_id;             /*!< 下一个分配的使用者ID */
    size_t   alignment;                 /*!< 内存对齐要求字节数 */
    size_t   alignment_padding;         /*!< 对齐填充的字节数 */
    char*    memory_descrition;         /*!< 内存池功能描述字符串 */
} static_memory_pool_t;

/* Exported functions --------------------------------------------------------*/


bool lm_static_memorypool_init(static_memory_pool_t *memory_pool_prot,uint8_t block_count);
void *lm_static_memory_allocate(const char* description, static_memory_pool_t *memory_pool_prot);
bool lm_static_memory_free(void* address, static_memory_pool_t *memory_pool_prot);

/* Private types -------------------------------------------------------------*/
/* 无私有类型 */

/* Private variables ---------------------------------------------------------*/
/* 无私有变量 */

/* Private constants ---------------------------------------------------------*/
/* 无私有常量 */

/* Private macros ------------------------------------------------------------*/
/* 无私有宏 */

/* Private functions ---------------------------------------------------------*/
static int lm_find_free_block(static_memory_pool_t *memory_pool_prot);
static int find_block_by_address(void* address, static_memory_pool_t *memory_pool_prot);

#endif /* LM_STATICMANAGER_H */
