/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_mollocmanager.h
 * 文件标识： 
 * 内容摘要： 动态空间管理
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者：    Qiguo_Cui                   
 * 完成日期： 2025年10月23日
 *
 *******************************************************************************/

#ifndef LM_MALLOCMANAGER_H
#define LM_MALLOCMANAGER_H

/* Define to prevent recursive inclusion -------------------------------------*/

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Exported constants --------------------------------------------------------*/
#define MEMORY_SIZE (4 * 1024)       /* 8KB静态存储空间 */
#define MAX_NAME_LEN 32              /* 内存块名称最大长度 */
#define MEM_ALIGNMENT 4              /* 内存对齐 */

/* Exported types ------------------------------------------------------------*/


typedef enum {
    MEM_MALLOC_FREE = 0,    /* 未使用 */
    MEM_MALLOC_USED = 1,    /* 已使用 */
    MEM_MALLOC_LOCKED = 2   /* 锁定（不可释放） */
} mem_malloc_status_t;


/* 内存块头部（嵌入在内存池中） */
typedef struct mem_block_header {
    struct mem_block_header* next;   /* 下一个内存块 */
    size_t size;                     /* 数据区大小 */
    mem_malloc_status_t status;      /* 内存块状态 */
    uint32_t id;                     /* 内存块ID */
    char name[MAX_NAME_LEN];         /* 内存块名称 */
    uint32_t checksum;               /* 校验和 */
} mem_block_header_t;

/* 内存管理器 */
typedef struct {
    uint8_t pool[MEMORY_SIZE];       /* 8KB静态内存池 */
    mem_block_header_t* free_list;   /* 空闲链表 */
    mem_block_header_t* used_list;   /* 已使用链表 */
    uint32_t next_id;                /* 下一个内存块ID */
    size_t used_size;                /* 已使用内存大小 */
    size_t free_size;                /* 空闲内存大小 */
    size_t overhead_size;            /* 管理开销大小 */
    bool initialized;                /* 初始化标志 */
} static_mem_t;

/* Exported functions --------------------------------------------------------*/
static inline size_t lm_align_size(size_t size);
static inline void* lm_get_data_ptr(mem_block_header_t* header);
static inline mem_block_header_t* lm_get_header_ptr(void* data);
static void lm_update_header_checksum(mem_block_header_t* header);
static bool lm_validate_header(mem_block_header_t* header);
static bool lm_is_valid_pointer(static_mem_t* mem, void* ptr);

void lm_mallocmanager_init(void);
void* lm_mallocmanager_alloc(const char* name, size_t size);
int lm_mallocmanager_free(void* ptr);
/* Private types -------------------------------------------------------------*/
/* 以下类型仅在模块内部使用，不对外暴露 */

/* Private variables ---------------------------------------------------------*/
/* 以下变量仅在模块内部使用，不对外暴露 */

/* Private constants ---------------------------------------------------------*/
/* 以下常量仅在模块内部使用，不对外暴露 */

/* Private macros ------------------------------------------------------------*/
/* 以下宏仅在模块内部使用，不对外暴露 */

/* Private functions ---------------------------------------------------------*/
/* 以下函数仅在模块内部使用，不对外暴露 */

#endif /* LM_MALLOCMANAGER_H */
