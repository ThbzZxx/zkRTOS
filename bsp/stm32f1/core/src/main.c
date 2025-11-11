#include "zk_rtos.h"

extern void task_test_main(void);
extern void board_init(void);

int main(void)
{
	board_init();
	zk_kernel_init();

	zk_start_scheduler();

	while (1)
	{
	}
}
