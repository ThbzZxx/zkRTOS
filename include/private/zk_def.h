/**
 * @file    zk_def.h
 * @brief   ZK-RTOS type definition
 */

#ifndef ZK_DEF_H
#define ZK_DEF_H

#include "zk_config.h"

/* Forward declaration of zk_printf for assert macros */
void zk_printf(const char *fmt, ...);

/* ==================== Basic type definition ==================== */
#ifndef ZK_SIZE_BYTE
#define ZK_SIZE_BYTE 1
#define ZK_SIZE_KB (1024 * ZK_SIZE_BYTE)
#define ZK_SIZE_MB (1024 * ZK_SIZE_KB)
#endif

/*============================================================================
 *                  System configuration
 *============================================================================*/

/* Memory configuration */
#define CONFIG_TOTAL_MEM_SIZE (10 * ZK_SIZE_KB) // Heap memory: 10KB
#define ZK_BYTE_ALIGNMENT 8						// Memory alignment: 8 bytes

/* Task system configuration */
#define ZK_PRIORITY_NUM 32				 // Number of priority levels: 32
#define CONFIG_TASK_NAME_LEN 10			 // Maximum task name length
#define TIMER_TASK_PRIORITY 0			 // Timer task priority (highest)
#define TIMER_TASK_STACK_SIZE 1024		 // Timer task stack size
#define SCHEDULE_TIME_SLICE_INIT_VALUE 5 // Time slice: 5 ticks
#define CONFIG_TICK_COUNT_INIT_VALUE 0	 // Tick initial value: 0

/* Semaphore configuration */
#define SEM_COUNT_MAX 0xFFFE // Maximum semaphore count value

/* Interrupt priority configuration */
#define ZK_KERNEL_INTERRUPT_PRIORITY 255	  // Kernel interrupt priority (lowest)
#define ZK_MAX_SYSCALL_INTERRUPT_PRIORITY 191 // Maximum syscall priority
#define ZK_SYSTICK_CLOCK_HZ ZK_CPU_CLOCK_HZ	  // SysTick clock source

/* Derived values (calculated automatically by system) */
#define ZK_MIN_PRIORITY (ZK_PRIORITY_NUM - 1)
#define ZK_HIGHEST_PRIORITY 0
#define ZK_BYTE_ALIGNMENT_MASK (ZK_BYTE_ALIGNMENT - 1)
#define IDLE_TASK_PRIO ZK_MIN_PRIORITY

/* Compile-time parameter validation */
#if (ZK_PRIORITY_NUM != 8) && (ZK_PRIORITY_NUM != 16) && (ZK_PRIORITY_NUM != 32) &&                \
	(ZK_PRIORITY_NUM != 64)
#error "ZK_PRIORITY_NUM must be 8, 16, 32, or 64"
#endif

#if (ZK_BYTE_ALIGNMENT != 4) && (ZK_BYTE_ALIGNMENT != 8)
#error "ZK_BYTE_ALIGNMENT must be 4 or 8"
#endif

#if (CONFIG_TASK_NAME_LEN < 4) || (CONFIG_TASK_NAME_LEN > 32)
#error "CONFIG_TASK_NAME_LEN must be between 4 and 32"
#endif

/* ==================== Basic type definitions ==================== */
typedef unsigned char zk_uint8;
typedef unsigned short zk_uint16;
typedef unsigned int zk_uint32;
typedef signed char zk_int8;
typedef signed short zk_int16;
typedef signed int zk_int32;

typedef enum
{
	ZK_FALSE = 0,
	ZK_TRUE = 1
} zk_bool;

/* ==================== Constant definitions ==================== */
#define ZK_NULL ((void *) 0)
#define ZK_UINT32_MAX (0xFFFFFFFFUL)

/* General constants */
#define ZK_STRING_TERMINATOR '\0'
#define ZK_TIMEOUT_INFINITE 0xFFFFFFFFUL
#define ZK_TIMEOUT_NONE 0

/* Task priority aliases (backward compatibility) */
#define ZK_MIN_TASK_PRIORITY ZK_MIN_PRIORITY
#define ZK_HIGHEST_TASK_PRIORITY ZK_HIGHEST_PRIORITY
#define MIN_TASK_PRIORITY ZK_MIN_PRIORITY
#define HIGHEST_TASK_PRIORITY ZK_HIGHEST_PRIORITY

/* Task stack protection values */
#define ZK_TASK_MAGIC_NUMBER 0xA5
#define ZK_TASK_STACK_BOUNDARY 0xA5A5A5A5
#define TASK_MAGIC_NUMBER ZK_TASK_MAGIC_NUMBER	   /* Backward compatibility */
#define TASK_STACK_BOUNDARY ZK_TASK_STACK_BOUNDARY /* Backward compatibility */

/* Bit operation constants */
#define ZK_BIT_MASK_0 0x01

/* ==================== List structure definition ==================== */
typedef struct zk_list_node
{
	struct zk_list_node *pre;
	struct zk_list_node *next;
} zk_list_node_t;

/* ==================== List macro definitions ==================== */
#define ZK_LIST_NODE_NULL ((zk_list_node_t *) ZK_NULL)
#define LIST_NODE_NULL ZK_LIST_NODE_NULL /* Backward compatibility */
#define ZK_OFFSET(type, member) ((zk_uint32) & ((type *) 0)->member)
#define ZK_GET_STRUCT(ptr, type, member) ((type *) ((zk_uint8 *) (ptr) - ZK_OFFSET(type, member)))
#define ZK_LIST_GET_OWNER(ptr, type, member) ZK_GET_STRUCT(ptr, type, member)
#define ZK_LIST_GET_FIRST_ENTRY(ptr, type, member) ZK_LIST_GET_OWNER((ptr)->next, type, member)
#define ZK_LIST_FOR_EACH_NODE(pos, head) for (pos = (head)->next; pos != (head); pos = pos->next)


/* ==================== Error code definitions ==================== */

typedef enum
{
	/* ===== Basic errors (0-3) ===== */
	ZK_SUCCESS = 0,		  /**< Operation successful */
	ZK_ERR_FAILED,		  /**< General failure */
	ZK_ERR_STATE,		  /**< State error (operation not allowed in current state) */
	ZK_ERR_NOT_SUPPORTED, /**< Feature not supported) */

	/* ===== Parameter errors (4-6) ===== */
	ZK_ERR_INVALID_PARAM,  /**< Invalid parameter */
	ZK_ERR_INVALID_HANDLE, /**< Invalid handle (NULL or destroyed) */
	ZK_ERR_OUT_OF_RANGE,   /**< Parameter value out of range */

	/* ===== Resource errors (7-8) ===== */
	ZK_ERR_NOT_ENOUGH_MEMORY,	 /**< Insufficient memory */
	ZK_ERR_RESOURCE_UNAVAILABLE, /**< Resource unavailable/exhausted */

	/* ===== Timeout error (9) ===== */
	ZK_ERR_TIMEOUT, /**< Wait or operation timeout */

	/* ===== Task errors (10-12) ===== */
	ZK_ERR_TASK_INVALID,		   /**< Task state or parameter invalid */
	ZK_ERR_TASK_NOT_FOUND,		   /**< Task not found */
	ZK_ERR_TASK_PRIORITY_CONFLICT, /**< Task priority conflict */

	/* ===== Synchronization errors (13-15) ===== */
	ZK_ERR_SYNC_INVALID,   /**< Synchronization object (mutex/sem) state invalid */
	ZK_ERR_SYNC_NOT_OWNER, /**< Non-owner attempted operation */
	ZK_ERR_SYNC_DEADLOCK,  /**< Deadlock detected */

	/* ===== Queue error (16) ===== */
	ZK_ERR_QUEUE_SIZE_MISMATCH, /**< Queue element size mismatch */

	/* ===== Memory and system errors (17-18) ===== */
	ZK_ERR_MEMORY_CORRUPTION, /**< Memory corruption */
	ZK_ERR_IN_INTERRUPT,	  /**< Disallowed operation in interrupt context */

	ZK_ERROR_COUNT /**< Total number of error codes */
} zk_error_code_t;


/* ==================== Task-related structures ==================== */
typedef void (*task_function_t)(void *private_data);

typedef enum task_state
{
	TASK_READY = 0,
	TASK_DELAY,
	TASK_SUSPEND,
	TASK_ENDLESS_BLOCKED,
	TASK_TIMEOUT_BLOCKED,
	TASK_UNKNOWN
} task_state_t;


typedef struct task_control_block
{
	void *stack;
	zk_list_node_t state_node;
	zk_uint8 priority;
	zk_uint8 base_priority;
	zk_uint8 *task_name[CONFIG_TASK_NAME_LEN];
	task_state_t state;
	zk_list_node_t event_sleep_list; // Event wait queue node
	zk_uint8 event_timeout_wakeup;
	zk_uint32 wake_up_time;

	/* P1: Stack overflow detection */
	void *stack_base;	  /* Stack base address (for overflow detection) */
	zk_uint32 stack_size; /* Stack size (bytes) */

	/* P1: Task runtime statistics */
	zk_uint32 run_time_ticks;	   /* Task cumulative runtime (tick) */
	zk_uint32 last_switch_in_time; /* Last switch-in timestamp (for delta calculation) */

#ifdef ZK_USING_MUTEX
	/* P1: Priority inheritance chain propagation */
	struct mutex *holding_mutex; /* Currently held mutex (for chain propagation) */
#endif
} task_control_block_t;


typedef struct task_init_parameter
{
	task_function_t task_entry;
	zk_uint8 priority;
	zk_uint8 name[CONFIG_TASK_NAME_LEN];
	zk_uint32 stack_size;
	void *private_data;
} task_init_parameter_t;

/* ==================== Timer structures ==================== */
#ifdef ZK_USING_TIMER
/* Timer callback function type definition */
typedef void (*timer_handler_t)(void *param);

typedef enum timer_mode
{
	TIMER_ONESHOT = 0, /* One-shot timer, stops automatically after triggering */
	TIMER_AUTO_RELOAD  /* Auto-reload timer, restarts counting after triggering */
} timer_mode_t;

typedef enum timer_status
{
	TIMER_STOP = 0, /* Timer stopped */
	TIMER_RUNNING	/* Timer running */
} timer_status_t;

typedef struct timer
{
	zk_list_node_t list;	 /* List node, used to add timer to manager */
	timer_status_t status;	 /* Timer running state */
	timer_mode_t mode;		 /* Timer mode */
	zk_uint32 interval;		 /* Timer interval time */
	timer_handler_t handler; /* Timer callback function */
	zk_uint32 wake_up_time;	 /* Next wake-up time */
	zk_uint8 is_used;		 /* Timer resource usage status */
	void *param;			 /* Parameter passed to callback function */
} timer_t;

typedef struct timer_manager
{
	zk_list_node_t
		timers_list; /* Timer list, sorted by timeout in ascending order (head is nearest timeout) */
} timer_manager_t;
#endif

/* ==================== Semaphore structures ==================== */
#ifdef ZK_USING_SEMAPHORE
typedef enum sem_status
{
	SEM_UNUSED = 0,
	SEM_USED
} sem_status_t;

typedef struct semaphore
{
	zk_list_node_t wait_list; // Task list waiting for this semaphore
	zk_uint32 count;		  // Semaphore count value
	zk_uint8 is_used;		  // Whether semaphore is in use
} semaphore_t;
#endif

/* ==================== Mutex structures ==================== */
#ifdef ZK_USING_MUTEX
typedef enum mutex_status
{
	MUTEX_UNUSED = 0,
	MUTEX_USED
} mutex_status_t;

typedef struct mutex
{
	zk_list_node_t sleep_list;	 // Task list blocked on this mutex
	task_control_block_t *owner; // Task currently holding the mutex
	zk_uint32 owner_hold_count;	 // Hold count (supports recursive locking)
	zk_uint8 owner_priority;	 // Owner priority (for priority inheritance)
	zk_uint8 is_used;			 // Whether mutex is in use

	/* P1: Chain priority inheritance */
	struct mutex *next_mutex; // Next mutex that owner is waiting for (for chain propagation)
} mutex_t;

#define MUTEX_HANDLE_TO_POINTER(handle) (&g_mutex_pool[handle])
#define CHECK_MUTEX_HANDLE_VALID(handle)                                                           \
	if (handle >= MUTEX_MAX_NUM)                                                                   \
	return ZK_ERR_INVALID_HANDLE

#define CHECK_MUTEX_CREATED(handle)                                                                \
	if (g_mutex_pool[handle].is_used == MUTEX_UNUSED)                                              \
	return ZK_ERR_STATE

#endif

/* ==================== Message queue structures ==================== */
#ifdef ZK_USING_QUEUE
typedef enum queue_state
{
	QUEUE_UNUSED = 0,
	QUEUE_USED,
} queue_state_t;

typedef struct queue
{
	void *data_buffer;				  // Queue data buffer pointer
	zk_list_node_t reader_sleep_list; // Read-blocked task list
	zk_list_node_t writer_sleep_list; // Write-blocked task list
	zk_uint32 read_pos;				  // Current read position index
	zk_uint32 write_pos;			  // Current write position index
	zk_uint32 element_single_size;	  // Size of single element (bytes)
	zk_uint32 element_num;			  // Number of elements queue can store
	zk_uint8 is_used;				  // Queue usage status flag
} queue_t;

#define QUEUE_HANDLE_TO_POINTER(handle) (&g_queue_pool[handle])
#define QUEUE_INDEX_TO_BUFFERADDR(queue_p, index)                                                  \
	(((zk_uint8 *) queue_p->data_buffer) + (queue_p->element_single_size * index))

#define QUEUE_CHECK_HANDLE_VALID(handle)                                                           \
	if (handle > QUEUE_MAX_NUM)                                                                    \
	return ZK_ERR_INVALID_HANDLE

#define QUEUE_CHECK_HANDLE_CREATED(handle)                                                         \
	if (g_queue_pool[handle].is_used == QUEUE_UNUSED)                                              \
	return ZK_ERR_STATE

#endif

/* ==================== Memory management structures ==================== */
typedef struct mem_manager
{
	zk_list_node_t free_list;
	zk_list_node_t used_list;
	zk_uint32 base_address;
	zk_uint32 total_size;
	zk_uint32 available_size;
	zk_bool is_initialized;

	/* P1 statistics information */
	zk_uint32 peak_used_size;	/* Peak usage (bytes) */
	zk_uint32 alloc_count;		/* Total allocation count */
	zk_uint32 free_count;		/* Total free count */
	zk_uint32 alloc_fail_count; /* Allocation failure count */
	zk_uint32 free_block_count; /* Current free block count */
	zk_uint32 used_block_count; /* Current used block count */
} mem_manager_t;

typedef struct mem_block
{
	zk_list_node_t list_node;
	zk_uint32 size;
} mem_block_t;

/* Global variable declarations (extern), actual definition in zk_mem.c */
extern const zk_uint32 MEM_BLOCK_ALIGNMENT;
extern zk_uint8 g_heap[CONFIG_TOTAL_MEM_SIZE];

#define MEM_BLOCK_MIN_SIZE (MEM_BLOCK_ALIGNMENT << 1)

/* ==================== Scheduler structures ==================== */
typedef struct task_scheduler
{
	zk_list_node_t ready_list[ZK_PRIORITY_NUM]; // Ready queue array
	zk_list_node_t delay_list;					// Delay queue
	zk_list_node_t suspend_list;				// Suspend queue
	zk_list_node_t block_timeout_list;			// Block timeout queue

	zk_uint32 priority_active;			 // Priority active bitmap
	zk_uint32 scheduler_suspend_nesting; // Scheduler suspend nesting count
	zk_uint32 re_schedule_pending;		 // Reschedule request flag
} task_scheduler_t;

// Block sort type enumeration
typedef enum block_sort_type
{
	BLOCK_SORT_FIFO = 0, // First-in-first-out sorting
	BLOCK_SORT_PRIO		 // Priority sorting
} block_sort_type_t;

// Block type enumeration
typedef enum block_type
{
	BLOCK_TYPE_ENDLESS = 0, // Endless blocking (no timeout)
	BLOCK_TYPE_TIMEOUT		// Timeout blocking
} block_type_t;

typedef enum scheduler_state_list
{
	READY_LIST = 0,		 // Ready task list
	DELAY_LIST,			 // Delay task list
	SUSPEND_LIST,		 // Suspend task list
	BLOCKED_TIMEOUT_LIST // Timeout blocked task list
} scheduler_state_list_t;

typedef enum schedule_pending
{
	SCHEDULE_PENDING_NONE = 0,
	SCHEDULE_PENDING_PENDING = 1
} schedule_pending_t;

typedef enum event_timeout_wakeup
{
	EVENT_NO_TIMEOUT = 0, // Event triggered, not timeout wakeup
	EVENT_WAIT_TIMEOUT	  // Event wait timeout wakeup
} event_timeout_wakeup_t;


/* ==================== Utility macros and functions ==================== */

#define ZK_TIME_MAX ZK_UINT32_MAX
#define ZK_TSK_DLY_MAX (ZK_TIME_MAX / 2)

/**
 * @brief Time comparison macros (overflow-safe)
 * @note These macros handle uint32 time overflow correctly using signed arithmetic
 *       They work correctly when time wraps around from 0xFFFFFFFF to 0x00000000
 *       Limitation: Maximum time difference must be < 2^31 (enforced by ZK_TSK_DLY_MAX)
 *
 * @param now    Current time value
 * @param target Target time value to compare against
 *
 * Examples:
 *   - zk_time_is_reached(0x00000002, 0xFFFFFFFE) → TRUE  (time has wrapped around)
 *   - zk_time_is_reached(0xFFFFFFFE, 0x00000002) → FALSE (target is in future)
 */
#define zk_time_is_reached(now, target) (((long) (now) - (long) (target)) >= 0)
#define zk_time_is_before(now, target) (((long) (now) - (long) (target)) < 0)
#define zk_time_is_after(now, target) (((long) (now) - (long) (target)) > 0)
#define zk_time_not_reached(now, target) (((long) (now) - (long) (target)) < 0)


static inline zk_uint32 zk_addr_align(zk_uint32 addr, zk_uint32 align, zk_uint32 mask)
{
	if (addr & mask)
	{
		addr &= ~mask;
		addr += align;
	}
	return addr;
}

static inline int zk_memcpy(void *dest, const void *src, zk_uint32 size)
{
	if (dest == ZK_NULL || src == ZK_NULL)
	{
		return 0;
	}

	zk_uint8 *dest_addr = (zk_uint8 *) dest;
	const zk_uint8 *src_addr = (const zk_uint8 *) src;

	while (size-- > 0)
	{
		*(dest_addr++) = *(src_addr++);
	}
	return 1;
}

static inline void zk_memset(void *addr, zk_uint8 data, zk_uint32 size)
{
	zk_uint8 *base_addr = (zk_uint8 *) addr;
	while (size-- > 0)
	{
		*base_addr++ = data;
	}
}


static inline void zk_memclear(void *addr, zk_uint32 size)
{
	zk_memset(addr, 0, size);
}


static inline zk_uint8 zk_add_overflow(zk_uint32 a, zk_uint32 b, zk_uint32 *res)
{
	*res = a + b;
	return (*res < a) || (*res < b);
}

/* ==================== Assert macros ==================== */
#define ZK_PRINTK(format, ...) zk_printf(format, ##__VA_ARGS__)
#define ZK_PRINTK_LN(format, ...) zk_printf(format "\n", ##__VA_ARGS__)


#ifdef DEBUG_ASSERT

#define ZK_ASSERT(expr)                                                                            \
	do                                                                                             \
	{                                                                                              \
		if (!(expr))                                                                               \
		{                                                                                          \
			ZK_PRINTK_LN("ASSERT: %s at %s:%d", #expr, __FILE__, __LINE__);                        \
			while (1)                                                                              \
				;                                                                                  \
		}                                                                                          \
	} while (0)

#define ZK_ASSERT_PARAM(expr) ZK_ASSERT(expr)

#define ZK_ASSERT_NULL_POINTER(ptr) ZK_ASSERT((ptr) != ZK_NULL)

#define ZK_ASSERT_SCHEDULER_RUNNING() ZK_ASSERT(!is_scheduler_suspending())

#else /* !DEBUG_ASSERT */
#define ZK_ASSERT(expr) ((void) 0)
#define ZK_ASSERT_PARAM(expr) ((void) 0)
#define ZK_ASSERT_NULL_POINTER(ptr) ((void) 0)
#define ZK_ASSERT_SCHEDULER_RUNNING() ((void) 0)
#endif

/* ==================== List inline functions ==================== */

static inline void zk_list_init(zk_list_node_t *list)
{
	list->pre = list;
	list->next = list;
}

/**
 * zk_list_add_after - Add new node after existing node
 */
static inline void __ListAdd(zk_list_node_t *addList, zk_list_node_t *prev, zk_list_node_t *next)
{
	addList->next = next;
	addList->pre = prev;
	prev->next = addList;
	next->pre = addList;
}

static inline void zk_list_add_after(zk_list_node_t *new, zk_list_node_t *old)
{
	new->next = old->next;
	new->pre = old;
	old->next->pre = new;
	old->next = new;
}

static inline void zk_list_add_before(zk_list_node_t *new, zk_list_node_t *old)
{
	new->pre = old->pre;
	new->next = old;
	old->pre->next = new;
	old->pre = new;
}

/**
 *  zk_list_delete - Delete node and reconnect its previous and next nodes
 */

static inline void zk_list_delete(zk_list_node_t *node)
{
	node->pre->next = node->next;
	node->next->pre = node->pre;
	node->pre = LIST_NODE_NULL;
	node->next = LIST_NODE_NULL;
}

/**
 *  zk_list_move_before - Move an existing node before another node
 */
static inline void zk_list_move_before(zk_list_node_t *old, zk_list_node_t *head)
{
	zk_list_delete(old);
	zk_list_add_before(old, head);
}


/**
 * ListMoveAfter - Move an existing node after another node
 */
static inline void zk_list_move_after(zk_list_node_t *old, zk_list_node_t *head)
{
	zk_list_delete(old);
	zk_list_add_after(old, head);
}

/**
 * zk_list_is_first - Check if list node is the first node in the list headed by head
 */
static inline int zk_list_is_first(zk_list_node_t *list, zk_list_node_t *head)
{
	return (list->pre == head);
}

static inline int zk_list_is_empty(zk_list_node_t *list)
{
	return (list->next == list);
}

/**
 * zk_list_is_last - Check if list node is the last node in the list headed by head
 */

static inline int zk_list_is_last(zk_list_node_t *list, zk_list_node_t *head)
{
	return (list->next == head);
}

/**
 *  GetListLast - Get the last node of the list headed by head
 */

static inline zk_list_node_t *zk_list_get_last(zk_list_node_t *head)
{
	return head->pre;
}

static inline zk_list_node_t *zk_list_get_first(zk_list_node_t *head)
{
	return head->next;
}

static inline void zk_list_move_to_tail(zk_list_node_t *list, zk_list_node_t *head)
{
	zk_list_delete(list);
	zk_list_add_before(list, head);
}

/* ==================== Critical section forward declaration ==================== */
#ifndef ZK_ENTER_CRITICAL
#define ZK_ENTER_CRITICAL() zk_cpu_enter_critical()
#define ZK_EXIT_CRITICAL() zk_cpu_exit_critical()
#endif

#endif /* ZK_DEF_H */
