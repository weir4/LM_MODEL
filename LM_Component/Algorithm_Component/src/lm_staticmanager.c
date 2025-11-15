/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： lm_staticmanager.c
 * 文件标识： 
 * 内容摘要： 静态空间管理
 * 其它说明： 无
 * 当前版本： v1.0.0
 * 作    者：    Qiguo_Cui                   
 * 完成日期： 2025年09月01日
 *
 *******************************************************************************/
/*******************************************************************************
0x20000000 ┌─────────────┐ ← 起始地址 (低地址)
           │   .data段   │ 已初始化全局/静态变量
           ├─────────────┤
           │   .bss段    │ 未初始化全局/静态变量
           ├─────────────┤
           │   堆(heap)  │ ↑ 从低向高增长
           │     ...     │
           ├─────────────┤
           │    栈(stack)│ ↓ 从高向低增长
0x2000xxxx └─────────────┘ ← 结束地址 (高地址)
********************************************************************************/

#include "lm_staticmanager.h"


/*******************************************************************************
函数名称：lm_static_memorypool_init
功能描述：初始化静态内存池，设置所有内存块的基本属性并进行地址对齐验证    
输入参数：memory_pool_prot - 指向静态内存池结构体的指针   
输出参数：初始化后的静态内存池    
返回值：true - 初始化成功，false - 初始化失败    
其它说明：该函数会遍历所有内存块，设置每个块的地址、大小和状态，并验证地址对齐性			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/24     V1.00          Qiguo_Cui          创建
*******************************************************************************/  
bool lm_static_memorypool_init(static_memory_pool_t *memory_pool_prot,uint8_t block_count)
{
		int memory_block_count = 0;
		if(block_count > BLOCK_COUNT)
		{
			  memory_block_count = BLOCK_COUNT;
		}
		else
		{
			  memory_block_count = block_count;
		}
    // 遍历所有内存块，进行初始化设置
    for (uint32_t i = 0; i < memory_block_count; i++) {
        // 计算当前内存块的起始地址，确保地址连续
        memory_pool_prot->blocks[i].address = memory_pool_prot->aligned_address + (i * memory_pool_prot->block_size);
        // 设置内存块的标称大小
        memory_pool_prot->blocks[i].size = memory_pool_prot->block_size;
        // 设置内存块的实际可用大小
        memory_pool_prot->blocks[i].actual_size = memory_pool_prot->block_size;
        // 初始化内存块状态为空闲状态
        memory_pool_prot->blocks[i].state = MEM_STATIC_FREE;
        // 设置内存块的唯一标识符
        memory_pool_prot->blocks[i].block_id = i;
        // 初始化内存块的所有者ID为0（无所有者）
        memory_pool_prot->blocks[i].owner_id = 0;
        // 清空内存块的描述信息
        memset(memory_pool_prot->blocks[i].description, 0, 
               sizeof(memory_pool_prot->blocks[i].description));
        
        // 验证每个内存块的地址是否满足内存对齐要求
        if (!IS_ALIGNED(memory_pool_prot->blocks[i].address, 4)) {
            // 如果地址未对齐，返回初始化失败
            return false;
        }
    }

    // 所有内存块初始化成功，返回true
    return true;
}

/*******************************************************************************
函数名称：lm_static_memory_allocate
功能描述：从静态内存池中分配指定大小的内存块    
输入参数：description - 内存块描述信息字符串指针
         memory_pool_prot - 指向静态内存池结构体的指针   
输出参数：分配的内存块地址    
返回值：成功返回分配的内存地址，失败返回NULL    
其它说明：该函数会查找空闲内存块，验证地址对齐性，并更新内存块状态和统计信息			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/24     V1.00          Qiguo_Cui          创建
*******************************************************************************/  
void *lm_static_memory_allocate(const char* description, static_memory_pool_t *memory_pool_prot)
{
    // 在内存池中查找空闲的内存块索引
    int block_index = lm_find_free_block(memory_pool_prot);
    // 如果没有找到空闲内存块，返回NULL表示分配失败
    if (block_index == -1) {
        return NULL;
    }
    
    // 获取指向目标内存块的指针
    memory_block_t * block = &memory_pool_prot->blocks[block_index];
    
		block->address = (uint8_t*)block;
    // 更新内存块状态为已使用状态
    block->state = MEM_STATIC_USED;
    // 分配新的所有者ID并递增下一个可用ID
    block->owner_id = memory_pool_prot->next_owner_id++;
    
    // 处理内存块的描述信息
    if (description) {
        // 如果提供了描述信息，复制到内存块中（安全复制，防止溢出）
        strncpy(block->description, description, 
                sizeof(block->description) - 1);
    } else {
        // 如果未提供描述信息，设置为默认描述
        strcpy(block->description, "unnamed");
    }
    
    // 更新内存池的统计信息：增加已使用块计数，减少空闲块计数
    memory_pool_prot->used_blocks++;
    memory_pool_prot->free_blocks--;

    // 返回分配的内存块地址给调用者
    return block->address;
}

/*******************************************************************************
函数名称：lm_find_free_block
功能描述：在静态内存池中查找第一个可用的空闲内存块    
输入参数：无   
输出参数：无    
返回值：成功返回空闲内存块索引，失败返回-1    
其它说明：该函数遍历整个内存池，返回第一个状态为MEM_FREE的内存块索引			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/24     V1.00          Qiguo_Cui          创建
*******************************************************************************/  
uint8_t test_find_free_block  = 0;
static int lm_find_free_block(static_memory_pool_t *memory_pool_prot) {
    // 遍历内存池中的所有内存块
	  static_memory_pool_t g_memory_pool = *memory_pool_prot;
    for (int i = 0; i < BLOCK_COUNT; i++) {
        // 检查当前内存块是否处于空闲状态
        if (g_memory_pool.blocks[i].state == MEM_STATIC_FREE) {
            // 找到空闲内存块，返回其索引
					  test_find_free_block = i;
            return test_find_free_block;
        }
    }
    // 遍历完成未找到空闲内存块，返回-1表示查找失败
    return -1;
}

/*******************************************************************************
函数名称：lm_static_memory_free
功能描述：释放之前分配的静态内存块，将其状态重置为空闲    
输入参数：address - 要释放的内存块地址指针
         memory_pool_prot - 指向静态内存池结构体的指针   
输出参数：无    
返回值：true - 释放成功，false - 释放失败    
其它说明：该函数会验证地址有效性，查找对应内存块，并重置其状态和统计信息			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/24     V1.00          Qiguo_Cui          创建
*******************************************************************************/  
bool lm_static_memory_free(void* address, static_memory_pool_t *memory_pool_prot) {
    // 检查传入的地址是否为NULL，如果是则直接返回失败
    if (address == NULL) {
        return false;
    }
    
    // 根据内存地址查找对应的内存块索引
    int block_index = find_block_by_address(address,memory_pool_prot);
    // 如果未找到对应的内存块，返回释放失败
    if (block_index == -1) {
        return false;
    }

    // 将传入的内存池指针赋值给局部变量，便于后续操作
    static_memory_pool_t g_memory_pool = *memory_pool_prot;
    // 获取指向目标内存块的指针
    memory_block_t* block = &g_memory_pool.blocks[block_index];
    
    // 验证内存块当前状态是否为已使用状态
    if (block->state != MEM_STATIC_USED) {
        // 如果内存块不是已使用状态，返回释放失败
        return false;
    }

    // 将内存块状态重置为空闲状态
    block->state = MEM_STATIC_FREE;
    // 清除内存块的所有者ID
    block->owner_id = 0;
    // 清空内存块的描述信息
    memset(block->description, 0, sizeof(block->description));
    
    // 更新内存池的统计信息：减少已使用块计数，增加空闲块计数
    g_memory_pool.used_blocks--;
    g_memory_pool.free_blocks++;
    
    // 内存块释放成功，返回true
    return true;
}

/*******************************************************************************
函数名称：find_block_by_address
功能描述：根据内存地址在内存池中查找对应的内存块索引    
输入参数：address - 要查找的内存地址指针
         memory_pool_prot - 指向静态内存池结构体的指针   
输出参数：无    
返回值：成功返回内存块索引，失败返回-1    
其它说明：该函数会验证地址是否在内存池有效范围内，并计算对应的块索引			   
			   
修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/10/24     V1.00          Qiguo_Cui          创建
*******************************************************************************/  
static int find_block_by_address(void* address, static_memory_pool_t *memory_pool_prot) {
    // 将传入的内存池指针赋值给局部变量，便于后续操作
    static_memory_pool_t g_memory_pool = *memory_pool_prot;
    // 将目标地址转换为字节指针类型，便于地址计算
    uint8_t* target_addr = (uint8_t*)address;
    // 获取内存池的起始地址
    uint8_t* pool_start = g_memory_pool.aligned_address;
    // 计算内存池的结束地址（起始地址+可用大小）
    uint8_t* pool_end = pool_start + g_memory_pool.usable_size;
    
    // 检查目标地址是否在内存池的有效范围内
    if (target_addr < pool_start || target_addr >= pool_end) {
        // 地址不在内存池范围内，返回-1表示查找失败
        return -1;
    }
    
    // 计算目标地址相对于内存池起始地址的偏移量
    uint32_t offset = target_addr - pool_start;
    // 根据偏移量和块大小计算对应的内存块索引
    uint32_t block_index = offset / g_memory_pool.block_size;
    
    // 验证计算出的索引是否在有效范围内
    if (block_index < BLOCK_COUNT) {
        // 索引有效，返回对应的内存块索引
        return block_index;
    }
    
    // 索引超出有效范围，返回-1表示查找失败
    return -1;
}
