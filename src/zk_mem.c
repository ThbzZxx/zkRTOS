/**
 * @file    zk_mem.c
 * @brief   Memory manager function
 */

#include "zk_internal.h"
#ifdef ZK_USING_HOOK
#include "zk_hook.h"
#endif

const zk_uint32 MEM_BLOCK_ALIGNMENT = (sizeof(mem_block_t) + (zk_uint32) (ZK_BYTE_ALIGNMENT - 1)) &
									  (~((zk_uint32) (ZK_BYTE_ALIGNMENT - 1)));
zk_uint8 g_heap[CONFIG_TOTAL_MEM_SIZE];

mem_manager_t g_mem_manager;

/**
 * @brief   Initialize memory manager
 */
void mem_init(void)
{
	mem_block_t *initial_free_block = ZK_NULL;

	g_mem_manager.base_address =
		zk_addr_align((zk_uint32) g_heap, ZK_BYTE_ALIGNMENT, ZK_BYTE_ALIGNMENT_MASK);
	g_mem_manager.total_size =
		CONFIG_TOTAL_MEM_SIZE - (g_mem_manager.base_address - (zk_uint32) g_heap);
	g_mem_manager.available_size = g_mem_manager.total_size;
	g_mem_manager.is_initialized = ZK_TRUE;

	/* 初始化统计信息 */
	g_mem_manager.peak_used_size = 0;
	g_mem_manager.alloc_count = 0;
	g_mem_manager.free_count = 0;
	g_mem_manager.alloc_fail_count = 0;
	g_mem_manager.free_block_count = 1;
	g_mem_manager.used_block_count = 0;

	zk_list_init(&g_mem_manager.free_list);
	zk_list_init(&g_mem_manager.used_list);

	initial_free_block = (mem_block_t *) g_mem_manager.base_address;
	initial_free_block->size = g_mem_manager.available_size;

	zk_list_add_after(&initial_free_block->list_node, &(g_mem_manager.free_list));
}

/**
 * @brief   Insert free block to free list by address
 * @param   block_to_insert Free block to insert
 */
static void mem_free_list_insert_by_addr(mem_block_t *block_to_insert)
{
	ZK_ASSERT_NULL_POINTER(block_to_insert);

	zk_list_node_t *current_node;
	mem_block_t *iterator;

	ZK_LIST_FOR_EACH_NODE(current_node, &g_mem_manager.free_list)
	{
		iterator = (mem_block_t *) current_node;
		if (iterator > block_to_insert)
		{
			break;
		}
	}

	zk_list_add_before(&block_to_insert->list_node, current_node);
}

/**
 * @brief   Allocate memory
 * @param   request_size Size of memory to allocate
 * @return  Pointer to allocated memory block
 */
void *mem_alloc(zk_uint32 request_size)
{
	ZK_ASSERT(g_mem_manager.is_initialized);

	/* Handle zero-size allocation: return NULL instead of asserting */
	if (request_size == 0)
	{
		return ZK_NULL;
	}

	ZK_ASSERT(request_size <= (ZK_UINT32_MAX - MEM_BLOCK_ALIGNMENT));

	zk_uint32 aligned_size;
	if (zk_add_overflow(request_size, MEM_BLOCK_ALIGNMENT, &aligned_size))
	{
		return ZK_NULL;
	}

	zk_uint32 final_size = zk_addr_align(request_size + MEM_BLOCK_ALIGNMENT, ZK_BYTE_ALIGNMENT,
										 ZK_BYTE_ALIGNMENT_MASK);

	/* Ensure final_size is at least MEM_BLOCK_MIN_SIZE */
	if (final_size < MEM_BLOCK_MIN_SIZE)
	{
		final_size = MEM_BLOCK_MIN_SIZE;
	}

	void *allocated_ptr = ZK_NULL;
	zk_list_node_t *current_node;
	mem_block_t *new_free_block = ZK_NULL;
	mem_block_t *allocated_block = ZK_NULL;

	ZK_ENTER_CRITICAL();

	if (final_size > g_mem_manager.available_size)
	{
		g_mem_manager.alloc_fail_count++;
#ifdef ZK_USING_HOOK
		zk_hook_call_malloc_failed(request_size);
#endif
		ZK_EXIT_CRITICAL();
		return ZK_NULL;
	}

	ZK_LIST_FOR_EACH_NODE(current_node, &g_mem_manager.free_list)
	{
		allocated_block = (mem_block_t *) current_node;
		if (allocated_block->size >= final_size)
		{
			zk_list_delete(&allocated_block->list_node);
			zk_list_add_after(&allocated_block->list_node, &g_mem_manager.used_list);

			if (allocated_block->size - final_size >= MEM_BLOCK_MIN_SIZE)
			{
				new_free_block = (mem_block_t *) ((zk_uint8 *) allocated_block + final_size);
				new_free_block->size = allocated_block->size - final_size;
				allocated_block->size = final_size;
				mem_free_list_insert_by_addr(new_free_block);
			}
			else
			{
				g_mem_manager.free_block_count--;
			}

			g_mem_manager.available_size -= final_size;
			g_mem_manager.used_block_count++;

			zk_uint32 current_used = g_mem_manager.total_size - g_mem_manager.available_size;
			if (current_used > g_mem_manager.peak_used_size)
			{
				g_mem_manager.peak_used_size = current_used;
			}

			g_mem_manager.alloc_count++;
			break;
		}
	}

	if (allocated_block == ZK_NULL)
	{
		g_mem_manager.alloc_fail_count++;
#ifdef ZK_USING_HOOK
		zk_hook_call_malloc_failed(request_size);
#endif
	}

	ZK_EXIT_CRITICAL();

	if (allocated_block != ZK_NULL)
	{
		allocated_ptr = (void *) ((zk_uint8 *) allocated_block + MEM_BLOCK_ALIGNMENT);
	}

	return allocated_ptr;
}

/**
 * @brief   Check memory list integrity
 * @param   head Head node of memory list
 */
static void mem_check_list_integrity(zk_list_node_t *head)
{
	ZK_ASSERT_NULL_POINTER(head);

	zk_list_node_t *node = head->next;
	while (node != head)
	{
		if (node->next == ZK_NULL || node->pre == ZK_NULL)
		{
			ZK_ASSERT(0);
		}
		node = node->next;
	}
}

/**
 * @brief   Merge free blocks in free list
 * @param   block_to_merge Free block to merge
 */
static void mem_merge_free_blocks(mem_block_t *block_to_merge)
{
	ZK_ASSERT_NULL_POINTER(block_to_merge);

	if (zk_list_is_empty(&g_mem_manager.free_list))
	{
		zk_list_add_after(&block_to_merge->list_node, &g_mem_manager.free_list);
		return;
	}

	zk_list_node_t *insert_pos = &g_mem_manager.free_list;
	mem_block_t *prev_block = ZK_NULL;
	mem_block_t *next_block = ZK_NULL;

	zk_list_node_t *current_node;
	ZK_LIST_FOR_EACH_NODE(current_node, &g_mem_manager.free_list)
	{
		if ((mem_block_t *) current_node > block_to_merge)
		{
			insert_pos = current_node;
			next_block = (mem_block_t *) current_node;
			break;
		}
	}

	if (insert_pos != &g_mem_manager.free_list && insert_pos->pre != &g_mem_manager.free_list)
	{
		prev_block = (mem_block_t *) insert_pos->pre;
	}

	if (next_block && (zk_uint8 *) block_to_merge + block_to_merge->size == (zk_uint8 *) next_block)
	{
		zk_list_add_before(&block_to_merge->list_node, &next_block->list_node);
		block_to_merge->size += next_block->size;
		zk_list_delete(&next_block->list_node);
	}

	if (prev_block && (zk_uint8 *) prev_block + prev_block->size == (zk_uint8 *) block_to_merge)
	{
		prev_block->size += block_to_merge->size;

		if (block_to_merge->list_node.next != ZK_NULL)
		{
			zk_list_delete(&block_to_merge->list_node);
		}
		return;
	}

	if (block_to_merge->list_node.next == ZK_NULL)
	{
		zk_list_add_before(&block_to_merge->list_node, insert_pos);
	}
}


/**
 * @brief   Free memory
 * @param   user_addr Pointer to memory block to free
 */
void mem_free(void *user_addr)
{
	/* Handle NULL pointer: return silently instead of asserting */
	if (user_addr == ZK_NULL)
	{
		return;
	}

	ZK_ASSERT(g_mem_manager.is_initialized);

	mem_block_t *mem_block = (mem_block_t *) ((zk_uint8 *) user_addr - MEM_BLOCK_ALIGNMENT);

	ZK_ASSERT((zk_uint32) mem_block >= g_mem_manager.base_address);
	ZK_ASSERT((zk_uint32) mem_block < (g_mem_manager.base_address + g_mem_manager.total_size));

	ZK_ASSERT(mem_block->size >= MEM_BLOCK_MIN_SIZE);
	ZK_ASSERT(mem_block->size <= g_mem_manager.total_size);

	zk_list_node_t *list_pos = ZK_NULL;
	zk_uint8 block_found = ZK_FALSE;
	zk_uint32 free_block_count_before = 0;

	ZK_ENTER_CRITICAL();

	ZK_LIST_FOR_EACH_NODE(list_pos, &g_mem_manager.used_list)
	{
		if (list_pos == &(mem_block->list_node))
		{
			block_found = ZK_TRUE;
			break;
		}
	}

	ZK_ASSERT(block_found);

	zk_list_node_t *node;
	ZK_LIST_FOR_EACH_NODE(node, &g_mem_manager.free_list)
	{
		free_block_count_before++;
	}

	g_mem_manager.available_size += mem_block->size;
	g_mem_manager.used_block_count--;
	zk_list_delete(&(mem_block->list_node));

	mem_check_list_integrity(&g_mem_manager.free_list);
	mem_merge_free_blocks(mem_block);
	mem_check_list_integrity(&g_mem_manager.free_list);

	g_mem_manager.free_block_count = 0;
	ZK_LIST_FOR_EACH_NODE(node, &g_mem_manager.free_list)
	{
		g_mem_manager.free_block_count++;
	}

	g_mem_manager.free_count++;

	ZK_EXIT_CRITICAL();
}

/**
 * @brief   Validate free list integrity
 */
void mem_validate_free_list(void)
{
	zk_list_node_t *node;
	mem_block_t *prev = ZK_NULL;

	ZK_LIST_FOR_EACH_NODE(node, &g_mem_manager.free_list)
	{
		mem_block_t *current = (mem_block_t *) node;
		if (prev != ZK_NULL && (zk_uint8 *) prev + prev->size > (zk_uint8 *) current)
		{
			zk_printf("[MEM] Free list order violation!");
			ZK_ASSERT(0);
		}
		prev = current;
	}
}

/**
 * @brief   Print memory statistics
 */
void mem_print_stats(void)
{
	zk_uint32 free_blocks = 0;
	zk_uint32 used_blocks = 0;

	zk_list_node_t *node;
	ZK_LIST_FOR_EACH_NODE(node, &g_mem_manager.free_list)
	{
		free_blocks++;
	}
	ZK_LIST_FOR_EACH_NODE(node, &g_mem_manager.used_list)
	{
		used_blocks++;
	}
}

/**
 * @brief   Print free blocks in free list
 */
void mem_print_free_blocks(void)
{
	zk_list_node_t *node;
	ZK_LIST_FOR_EACH_NODE(node, &g_mem_manager.free_list)
	{
	}
}

/**
 * @brief   Get memory statistics
 * @param   total_size Total memory size (output parameter)
 * @param   used_size Current used size (output parameter)
 * @param   peak_used Peak used size (output parameter)
 * @param   free_blocks Number of free blocks (output parameter)
 * @param   alloc_count Total allocation count (output parameter)
 * @param   alloc_fail_count Allocation failure count (output parameter)
 * @note    P1: Memory statistics feature
 */
void mem_get_stats(zk_uint32 *total_size, zk_uint32 *used_size, zk_uint32 *peak_used,
				   zk_uint32 *free_blocks, zk_uint32 *alloc_count, zk_uint32 *alloc_fail_count)
{
	ZK_ENTER_CRITICAL();

	if (total_size)
		*total_size = g_mem_manager.total_size;
	if (used_size)
		*used_size = g_mem_manager.total_size - g_mem_manager.available_size;
	if (peak_used)
		*peak_used = g_mem_manager.peak_used_size;
	if (free_blocks)
		*free_blocks = g_mem_manager.free_block_count;
	if (alloc_count)
		*alloc_count = g_mem_manager.alloc_count;
	if (alloc_fail_count)
		*alloc_fail_count = g_mem_manager.alloc_fail_count;

	ZK_EXIT_CRITICAL();
}

/**
 * @brief   Calculate memory fragmentation rate
 * @return  Fragmentation rate (percentage, 0-100)
 * @note    P1: Fragmentation rate = (free block count - 1) / free block count * 100
 *          Ideal case is 1 large block (0% fragmentation), multiple small blocks indicate severe fragmentation
 */
zk_uint32 mem_get_fragmentation(void)
{
	zk_uint32 fragmentation = 0;

	ZK_ENTER_CRITICAL();

	if (g_mem_manager.free_block_count > 1)
	{
		fragmentation =
			((g_mem_manager.free_block_count - 1) * 100) / g_mem_manager.free_block_count;
	}

	ZK_EXIT_CRITICAL();

	return fragmentation;
}
