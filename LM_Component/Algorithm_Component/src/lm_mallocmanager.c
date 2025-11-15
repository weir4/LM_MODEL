/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_mollocmanager.c
 * 文件标识： 
 * 内容摘要： 动态空间管理
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年10月23日
 *
 *******************************************************************************/




#include "lm_mallocmanager.h"
#include <string.h>
#include <stdio.h>


static_mem_t  memory;
static_mem_t *mem = &memory;

/*******************************************************************************
函数名称：lm_mallocmanager_init
功能描述：初始化8KB静态内存管理器，设置初始内存池和空闲链表    
输入参数：无   
输出参数：无    
返 回 值：无    
其它说明：此函数必须在调用其他内存管理函数前执行，用于初始化内存管理器的
          所有内部状态，包括创建初始的空闲内存块和设置管理数据结构			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/25     V1.00          Qiguo_Cui          创建
*******************************************************************************/
void lm_mallocmanager_init(void) {
    /* 检查内存管理器指针是否有效 */
    if (!mem) return;
    
    /* 清空整个8KB内存池，初始化为0 */
    memset(mem->pool, 0, MEMORY_SIZE);
    
    /* 初始化第一个空闲块，占据整个内存池 */
    mem_block_header_t* first_block = (mem_block_header_t*)mem->pool;  /* 将内存池起始地址转换为块头指针 */
    first_block->next = NULL;                                          /* 设置下一个块指针为空 */
    first_block->size = MEMORY_SIZE - sizeof(mem_block_header_t);      /* 计算可用数据区大小 */
    first_block->status = MEM_MALLOC_FREE;                             /* 标记为空闲状态 */
    first_block->id = 0;                                               /* 空闲块ID设为0 */
    strcpy(first_block->name, "INITIAL_FREE");                         /* 设置初始空闲块名称 */
    lm_update_header_checksum(first_block);                            /* 计算并设置块头校验和 */
    
    /* 初始化内存管理器全局状态 */
    mem->free_list = first_block;                                      /* 空闲链表指向初始块 */
    mem->used_list = NULL;                                             /* 已使用链表初始为空 */
    mem->next_id = 1;                                                  /* 下一个分配ID从1开始 */
    mem->used_size = 0;                                                /* 已使用内存大小初始为0 */
    mem->free_size = first_block->size;                                /* 空闲内存大小为初始块大小 */
    mem->overhead_size = sizeof(mem_block_header_t);                   /* 管理开销为第一个块头大小 */
    mem->initialized = true;                                           /* 标记管理器已初始化 */
}

/*******************************************************************************
函数名称：lm_mallocmanager_alloc
功能描述：从8KB静态内存池中分配指定大小的内存块    
输入参数：name - 内存块名称标识，用于调试和跟踪
          size - 请求分配的内存大小（字节）   
输出参数：无    
返 回 值：成功返回指向分配内存的指针，失败返回NULL    
其它说明：此函数实现首次适应算法，在空闲链表中搜索第一个足够大的块进行分配，
          支持内存块分割以提高空间利用率			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/25     V1.00          Qiguo_Cui          创建
*******************************************************************************/
void* lm_mallocmanager_alloc(const char* name, size_t size) {
    /* 参数有效性检查：管理器、初始化状态、大小范围 */
    if (!mem || !mem->initialized || size == 0 || size > mem->free_size) {
        return NULL;  /* 参数无效或内存不足时返回空指针 */
    }
    
    /* 计算对齐后的实际需求大小和总内存需求 */                                                        
      size_t aligned_size = lm_align_size(size);                         /* 按4字节对齐调整大小 */
//    size_t total_needed = aligned_size + sizeof(mem_block_header_t);   /* 总需求=数据大小+块头大小 */
    
    /* 在空闲链表中查找合适的块 */
    mem_block_header_t* prev = NULL;                                   /* 前驱节点指针 */
    mem_block_header_t* current = mem->free_list;                      /* 当前节点指针，从头开始搜索 */
    
    /* 遍历空闲链表寻找合适的内存块 */
    while (current) {
        /* 验证当前块头完整性，防止内存损坏 */
        if (!lm_validate_header(current)) {
            return NULL; /* 内存块损坏，终止分配 */
        }
        
        /* 检查当前块是否空闲且大小足够 */
        if (current->status == MEM_MALLOC_FREE && current->size >= aligned_size) {
            break; /* 找到合适的空闲块，退出搜索循环 */
        }
        
        /* 移动到链表下一个节点 */
        prev = current;
        current = current->next;
    }
    
    /* 检查是否找到合适的空闲块 */
    if (!current) {
        return NULL; /* 未找到合适块，分配失败 */
    }
    
    /* 计算分割决策：剩余空间是否足够创建新空闲块 */
    size_t remaining = current->size - aligned_size;                   /* 计算分配后剩余空间 */
    bool should_split = (remaining >= (sizeof(mem_block_header_t) + MEM_ALIGNMENT));  /* 判断是否需要分割 */
    
    /* 如果需要分割当前块 */
    if (should_split) {
        /* 创建新的空闲块在分配块之后 */
        mem_block_header_t* new_free = (mem_block_header_t*)((uint8_t*)current + 
                                    sizeof(mem_block_header_t) + aligned_size);  /* 计算新块位置 */
			
        new_free->next = current->next;                                /* 新块继承原块的链表关系 */
        new_free->size = remaining - sizeof(mem_block_header_t);       /* 设置新块数据区大小 */
        new_free->status = MEM_MALLOC_FREE;                            /* 标记新块为空闲状态 */
        new_free->id = 0;                                              /* 空闲块ID设为0 */
        strcpy(new_free->name, "FREE_BLOCK");                          /* 设置空闲块名称 */
        lm_update_header_checksum(new_free);                           /* 更新新块校验和 */
        
        /* 更新当前分配块信息 */
        current->size = aligned_size;                                  /* 调整当前块大小为请求大小 */
        current->next = new_free;                                      /* 当前块指向新创建的空闲块 */
        
        /* 更新内存管理器开销统计 */
        mem->overhead_size += sizeof(mem_block_header_t);              /* 增加一个新块头的管理开销 */
    }
    
    /* 从空闲链表移除当前分配块 */
    if (prev) {
        prev->next = current->next;    /* 中间节点：前驱指向后继 */
    } else {
        mem->free_list = current->next; /* 头节点：更新链表头指针 */
    }
    
    /* 将分配块添加到已使用链表头部 */
    current->next = mem->used_list;    /* 当前块指向原使用链表头 */
    mem->used_list = current;          /* 更新使用链表头为当前块 */  
    
    /* 更新分配块的元数据信息 */
    current->status = MEM_MALLOC_USED;                                 /* 标记块为已使用状态 */
    current->id = mem->next_id++;                                      /* 分配唯一ID并递增计数器 */
    if (name) {
        /* 复制用户提供的名称，确保字符串安全 */
        strncpy(current->name, name, MAX_NAME_LEN - 1);
        current->name[MAX_NAME_LEN - 1] = '\0';                        /* 强制添加字符串结束符 */
    } else {
        strcpy(current->name, "UNNAMED");                              /* 使用默认名称 */
    }
    lm_update_header_checksum(current);                                /* 更新分配块的校验和 */
    
    /* 更新内存使用统计信息 */
    mem->used_size += current->size;                                   /* 增加已使用内存统计 */
    mem->free_size -= current->size;                                   /* 减少空闲内存统计 */
    if (should_split) {
        mem->free_size -= sizeof(mem_block_header_t); /* 如果分割了，额外扣除新块头的空间开销 */
    }
    
    /* 返回分配的内存数据区指针（跳过块头） */
    return lm_get_data_ptr(current);
}

/*******************************************************************************
函数名称：lm_mallocmanager_free
功能描述：释放之前分配的内存块，将其返回给空闲链表并尝试合并相邻空闲块    
输入参数：ptr - 要释放的内存块指针   
输出参数：无    
返 回 值：成功返回0，失败返回错误码（负数）    
其它说明：此函数会验证指针有效性、内存块状态，并执行内存块合并以减少外部碎片，
          支持错误码返回便于调试和错误处理			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/25     V1.00          Qiguo_Cui          创建
*******************************************************************************/
int lm_mallocmanager_free(void* ptr) {
    /* 基本参数有效性检查 */
    if (!mem || !mem->initialized || !ptr) {
        return -1;  /* 管理器未初始化或空指针 */
    }
    
    /* 检查指针是否在有效的内存池范围内 */
    if (!lm_is_valid_pointer(mem, ptr)) {
        return -2;  /* 指针越界错误 */
    }
    
    /* 通过数据指针获取对应的内存块头部 */
    mem_block_header_t* header = lm_get_header_ptr(ptr);
    
    /* 验证内存块头部完整性 */
    if (!lm_validate_header(header)) {
        return -3; /* 内存块损坏，校验和验证失败 */
    }
    
    /* 检查内存块状态是否允许释放 */
    if (header->status != MEM_MALLOC_USED && header->status != MEM_MALLOC_LOCKED) {
        return -4; /* 不是已使用状态，无法释放 */
    }
    
    /* 检查内存块是否被锁定 */
    if (header->status == MEM_MALLOC_LOCKED) {
        return -5; /* 内存块被锁定，禁止释放 */
    }
    
    /* 从已使用链表中查找并移除目标块 */
    mem_block_header_t* prev_used = NULL;                              /* 已使用链表前驱指针 */
    mem_block_header_t* curr_used = mem->used_list;                    /* 当前搜索指针 */
    
    /* 遍历已使用链表寻找目标块 */
    while (curr_used && curr_used != header) {
        prev_used = curr_used;                                         /* 记录前驱节点 */
        curr_used = curr_used->next;                                   /* 移动到下一个节点 */
    }
    
    /* 检查是否在已使用链表中找到目标块 */
    if (!curr_used) {
        return -6; /* 未在已使用链表中找到指定块 */
    }
    
    /* 从已使用链表中移除目标块 */
    if (prev_used) {
        prev_used->next = header->next;    /* 中间节点：前驱指向后继 */
    } else {
        mem->used_list = header->next;     /* 头节点：更新链表头指针 */
    }
    
    /* 更新内存块状态为空闲 */
    header->status = MEM_MALLOC_FREE;                                 /* 标记块为空闲状态 */
    strcpy(header->name, "FREE_BLOCK");                              /* 设置空闲块标准名称 */
    lm_update_header_checksum(header);                               /* 更新块头校验和 */
    
    /* 将释放的块按地址顺序插入空闲链表 */
    mem_block_header_t* prev_free = NULL;                              /* 空闲链表前驱指针 */
    mem_block_header_t* curr_free = mem->free_list;                    /* 当前搜索指针 */
    
    /* 在空闲链表中找到合适的插入位置（按内存地址升序） */
    while (curr_free && curr_free < header) {
        prev_free = curr_free;                                         /* 记录前驱节点 */
        curr_free = curr_free->next;                                   /* 移动到下一个节点 */
    }
    
    /* 插入目标块到空闲链表 */
    header->next = curr_free;                                          /* 目标块指向当前找到的位置 */
    if (prev_free) {
        prev_free->next = header;          /* 中间插入：前驱指向目标块 */
    } else {
        mem->free_list = header;           /* 头部插入：更新链表头指针 */
    }
    
    /* 合并相邻的空闲块以减少外部碎片 */
    mem_block_header_t* temp = mem->free_list;                         /* 遍历指针从链表头开始 */
    while (temp && temp->next) {
        /* 验证相邻块的完整性 */
        if (!lm_validate_header(temp) || !lm_validate_header(temp->next)) {
            break; /* 发现内存损坏，终止合并操作 */
        }
        
        mem_block_header_t* next_block = temp->next;                   /* 获取下一个相邻块 */
        
        /* 检查当前块与下一个块是否在内存中相邻 */
        if ((uint8_t*)temp + sizeof(mem_block_header_t) + temp->size == (uint8_t*)next_block) {
            /* 合并相邻的空闲块 */
            temp->size += sizeof(mem_block_header_t) + next_block->size;  /* 合并块大小 */
            temp->next = next_block->next;                             /* 跳过被合并的块 */
            
            /* 更新管理开销统计 */
            mem->overhead_size -= sizeof(mem_block_header_t);          /* 减少一个块头的开销 */
            
            /* 清空被合并块的头信息（可选，便于调试） */
            memset(next_block, 0, sizeof(mem_block_header_t));
        } else {
            temp = temp->next;                                         /* 移动到下一个块继续检查 */
        }
    }
    
    /* 更新内存使用统计信息 */
    mem->used_size -= header->size;                                    /* 减少已使用内存统计 */
    mem->free_size += header->size;                                    /* 增加空闲内存统计 */
    
    /* 返回成功状态 */
    return 0;
}

/*******************************************************************************
函数名称：lm_align_size
功能描述：计算按指定对齐要求调整后的内存大小    
输入参数：size - 原始内存大小（字节）   
输出参数：无    
返 回 值：对齐后的内存大小    
其它说明：使用4字节对齐，确保内存访问效率和硬件兼容性			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/25     V1.00          Qiguo_Cui          创建
*******************************************************************************/
static inline size_t lm_align_size(size_t size) {
    /* 计算公式：(size + (alignment - 1)) & ~(alignment - 1) */
    /* 将大小向上对齐到4字节边界，提高内存访问效率 */
    return (size + (MEM_ALIGNMENT - 1)) & ~(MEM_ALIGNMENT - 1);
}

/*******************************************************************************
函数名称：lm_get_data_ptr
功能描述：从内存块头部指针计算对应的数据区指针    
输入参数：header - 内存块头部指针   
输出参数：无    
返 回 值：数据区起始指针    
其它说明：数据区位于块头之后，此函数用于跳过块头访问实际数据			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/25     V1.00          Qiguo_Cui          创建
*******************************************************************************/
static inline void* lm_get_data_ptr(mem_block_header_t* header) {
    /* 计算数据区指针：头部地址 + 头部大小 */
    /* 使用uint8_t指针运算确保字节级精度 */
    return (void*)((uint8_t*)header + sizeof(mem_block_header_t));
}

/*******************************************************************************
函数名称：lm_get_header_ptr
功能描述：从数据区指针反推对应的内存块头部指针    
输入参数：data - 数据区指针   
输出参数：无    
返 回 值：内存块头部指针    
其它说明：通过数据指针反向定位块头，用于内存管理和释放操作			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/25     V1.00          Qiguo_Cui          创建
*******************************************************************************/
static inline mem_block_header_t* lm_get_header_ptr(void* data) {
    /* 计算头部指针：数据区地址 - 头部大小 */
    /* 使用uint8_t指针运算确保字节级精度 */
    return (mem_block_header_t*)((uint8_t*)data - sizeof(mem_block_header_t));
}

/*******************************************************************************
函数名称：lm_is_valid_pointer
功能描述：验证指针是否在8KB内存池的有效范围内    
输入参数：mem - 内存管理器指针
          ptr - 待验证的指针   
输出参数：无    
返 回 值：true-指针有效，false-指针无效    
其它说明：此函数用于防止越界访问，确保内存操作的安全性			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/25     V1.00          Qiguo_Cui          创建
*******************************************************************************/
static bool lm_is_valid_pointer(static_mem_t* mem, void* ptr) {
    /* 检查指针是否为NULL */
    if (!ptr) return false;
    
    /* 计算内存池的起始和结束地址 */
    uint8_t* pool_start = mem->pool;                                   /* 内存池起始地址 */
    uint8_t* pool_end = pool_start + MEMORY_SIZE;                      /* 内存池结束地址 */
    uint8_t* test_ptr = (uint8_t*)ptr;                                 /* 待测试指针转换为字节指针 */
    
    /* 检查指针是否在内存池地址范围内 */
    return (test_ptr >= pool_start && test_ptr < pool_end);
}

/*******************************************************************************
函数名称：lm_calc_checksum
功能描述：计算内存块头部的校验和，用于数据完整性验证    
输入参数：header - 内存块头部指针   
输出参数：无    
返 回 值：32位校验和值    
其它说明：使用滚动哈希算法，计算除checksum字段外所有字节的校验和，
          用于检测内存块头部数据是否被意外修改			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/25     V1.00          Qiguo_Cui          创建
*******************************************************************************/
static uint32_t lm_calc_checksum(mem_block_header_t* header) {
    uint32_t sum = 0;                                                  /* 初始化校验和 */
    uint8_t* bytes = (uint8_t*)header;                                 /* 将头部转换为字节数组 */
    
    /* 遍历头部所有字节（除checksum字段外）计算校验和 */
    for (size_t i = 0; i < sizeof(mem_block_header_t) - sizeof(uint32_t); i++) {
        /* 使用移位和异或操作的滚动哈希算法 */
        /* 算法：sum = (sum << 3) + (sum >> 5) + bytes[i] */
        sum = (sum << 3) + (sum >> 5) + bytes[i];                      /* 更新校验和 */
    }
    
    /* 返回最终计算的校验和值 */
    return sum;
}

/*******************************************************************************
函数名称：lm_validate_header
功能描述：验证内存块头部的数据完整性    
输入参数：header - 内存块头部指针   
输出参数：无    
返 回 值：true-头部数据完整，false-头部数据损坏    
其它说明：通过比较存储的校验和与重新计算的校验和来验证数据完整性			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/25     V1.00          Qiguo_Cui          创建
*******************************************************************************/
static bool lm_validate_header(mem_block_header_t* header) {
    /* 比较存储的校验和与重新计算的校验和是否一致 */
    return header->checksum == lm_calc_checksum(header);
}

/*******************************************************************************
函数名称：lm_update_header_checksum
功能描述：更新内存块头部的校验和字段    
输入参数：header - 内存块头部指针   
输出参数：无    
返 回 值：无    
其它说明：在修改内存块头部数据后必须调用此函数更新校验和，
          以确保后续完整性验证能够通过			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/25     V1.00          Qiguo_Cui          创建
*******************************************************************************/
static void lm_update_header_checksum(mem_block_header_t* header) {
    /* 重新计算并更新头部的校验和字段 */
    header->checksum = lm_calc_checksum(header);
}


