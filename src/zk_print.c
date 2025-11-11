/**
 * @file    zk_print.c
 * @brief   ZK-RTOS lightweight print output module
 */

#include "zk_def.h"
#include <stdarg.h>

/**
 * @brief Weak symbol putc hook (users can override to implement UART/SWO output)
 * @param c Character to output
 */
__attribute__((weak)) void zk_putc(char c)
{
	//用户应该在 bsp 层重写此函数
	(void) c;
}

/**
 * @brief Lightweight vsnprintf implementation (supports only %d %u %x %s %c)
 * @param buffer Output buffer
 * @param size Buffer size (including null terminator)
 * @param fmt Format string
 * @param args Variable argument list
 * @return Number of characters written (excluding null terminator)
 */
static int zk_vsnprintf(char *buffer, zk_uint32 size, const char *fmt, va_list args)
{
	char *ptr = buffer;
	const char *end = buffer + size - 1;

	while (*fmt && ptr < end)
	{
		if (*fmt == '%')
		{
			fmt++;
			if (*fmt == 'd') /* 有符号十进制 */
			{
				int value = va_arg(args, int);
				char temp[12];
				int len = 0;

				if (value < 0)
				{
					if (ptr < end)
						*ptr++ = '-';
					value = -value;
				}
				do
				{
					temp[len++] = (char) ('0' + (value % 10));
					value /= 10;
				} while (value && len < (int) sizeof(temp));

				while (len-- && ptr < end)
					*ptr++ = temp[len];
			}
			else if (*fmt == 'u') /* 无符号十进制 */
			{
				unsigned int value = va_arg(args, unsigned int);
				char temp[12];
				int len = 0;

				do
				{
					temp[len++] = (char) ('0' + (value % 10));
					value /= 10;
				} while (value && len < (int) sizeof(temp));

				while (len-- && ptr < end)
					*ptr++ = temp[len];
			}
			else if (*fmt == 'x') /* 十六进制（小写） */
			{
				unsigned int value = va_arg(args, unsigned int);
				char temp[10];
				int len = 0;

				do
				{
					int digit = value % 16;
					temp[len++] = (char) (digit < 10 ? ('0' + digit) : ('a' + digit - 10));
					value /= 16;
				} while (value && len < (int) sizeof(temp));

				while (len-- && ptr < end)
					*ptr++ = temp[len];
			}
			else if (*fmt == 's') /* 字符串 */
			{
				const char *str = va_arg(args, const char *);
				if (str == ZK_NULL)
					str = "(null)";
				while (*str && ptr < end)
					*ptr++ = *str++;
			}
			else if (*fmt == 'c') /* 字符 */
			{
				char c = (char) va_arg(args, int);
				if (ptr < end)
					*ptr++ = c;
			}
			else /* 未识别的格式符，原样输出 */
			{
				if (ptr < end)
					*ptr++ = '%';
				if (ptr < end)
					*ptr++ = *fmt;
			}
		}
		else
		{
			if (ptr < end)
				*ptr++ = *fmt;
		}
		fmt++;
	}

	*ptr = '\0';
	return (int) (ptr - buffer);
}

/**
 * @brief Lightweight printf implementation
 * @param fmt Format string
 * @note Supports format specifiers: %d (signed) %u (unsigned) %x (hexadecimal) %s (string) %c (character)
 */
void zk_printf(const char *fmt, ...)
{
	char buffer[ZK_PRINTF_BUF_SIZE];
	va_list args;
	int length;

	va_start(args, fmt);
	length = zk_vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	for (int i = 0; i < length; i++)
		zk_putc(buffer[i]);
}
