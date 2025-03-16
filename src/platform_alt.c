#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"
#include "os.h"
#include <string.h>

#if defined(MBEDTLS_PLATFORM_MEMORY)

static void *platform_calloc(size_t n, size_t size)
{
    void *ptr = os_malloc(n * size);
    if (ptr != NULL) {
        memset(ptr, 0, n * size);
    }
    return ptr;
}

static void platform_free(void *ptr)
{
    os_free(ptr);
}

int mbedtls_platform_set_calloc_free(void *(*calloc_func)(size_t, size_t),
                                    void (*free_func)(void *))
{
    mbedtls_calloc = calloc_func;
    mbedtls_free = free_func;
    return 0;
}

#endif /* MBEDTLS_PLATFORM_MEMORY */

#if defined(MBEDTLS_ENTROPY_HARDWARE_ALT)

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
    // 使用硬件随机数生成器
    uint32_t random;
    size_t available = 0;
    
    while (available < len) {
        random = hw_get_random();
        size_t chunk = (len - available) < 4 ? (len - available) : 4;
        memcpy(output + available, &random, chunk);
        available += chunk;
    }
    
    *olen = len;
    return 0;
}

#endif /* MBEDTLS_ENTROPY_HARDWARE_ALT */

// 时间获取函数
time_t mbedtls_time(time_t *timer)
{
    time_t now = os_get_time();
    if (timer != NULL) {
        *timer = now;
    }
    return now;
} 