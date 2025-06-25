#include "posix_stub.h"
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <esp_log.h>

static const char *TAG = "posix_stub";

// Define CPU affinity types and macros for ESP32-S3 compatibility
#define CPU_SETSIZE 2  // ESP32-S3 has 2 cores

typedef struct {
    unsigned long bits;
} cpu_set_t;

// CPU set manipulation macros
#define CPU_ZERO(cpuset) do { (cpuset)->bits = 0; } while (0)
#define CPU_SET(cpu, cpuset) do { (cpuset)->bits |= (1UL << (cpu)); } while (0)
#define CPU_CLR(cpu, cpuset) do { (cpuset)->bits &= ~(1UL << (cpu)); } while (0)
#define CPU_ISSET(cpu, cpuset) (((cpuset)->bits & (1UL << (cpu))) != 0)

// Stub implementation of sysconf
// OpenH264 calls this to get number of processors
long sysconf(int name) {
    ESP_LOGD(TAG, "sysconf called with name=%d", name);
    
    switch (name) {
        case _SC_NPROCESSORS_ONLN:
        case _SC_NPROCESSORS_CONF:
            // Return number of cores on ESP32-S3 (2 cores)
            return 2;
        case _SC_PAGESIZE:
            return 4096;  // 4KB page size
        case _SC_CLK_TCK:
            return 100;   // 100 Hz tick rate
        default:
            ESP_LOGW(TAG, "Unsupported sysconf parameter: %d", name);
            errno = EINVAL;
            return -1;
    }
}

// Stub implementation of pthread_setaffinity_np
// OpenH264 calls this to set thread CPU affinity
int pthread_setaffinity_np(pthread_t thread, size_t cpusetsize, const cpu_set_t *cpuset) {
    ESP_LOGD(TAG, "pthread_setaffinity_np called (stub implementation)");
    
    // On ESP32-S3, we can't really set CPU affinity for pthreads the same way
    // as Linux, so we just return success
    (void)thread;
    (void)cpusetsize;
    (void)cpuset;
    
    return 0;  // Success
}

// Stub implementation of pthread_getaffinity_np
int pthread_getaffinity_np(pthread_t thread, size_t cpusetsize, cpu_set_t *cpuset) {
    ESP_LOGD(TAG, "pthread_getaffinity_np called (stub implementation)");
    
    (void)thread;
    (void)cpusetsize;
    
    if (cpuset && cpusetsize >= sizeof(cpu_set_t)) {
        // Set both cores as available
        CPU_ZERO(cpuset);
        CPU_SET(0, cpuset);
        CPU_SET(1, cpuset);
        return 0;
    }
    
    errno = EINVAL;
    return -1;
}

// Stub for sched_setaffinity if needed
int sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask) {
    ESP_LOGD(TAG, "sched_setaffinity called (stub implementation)");
    
    (void)pid;
    (void)cpusetsize;
    (void)mask;
    
    return 0;  // Success
}

// Stub for sched_getaffinity if needed
int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask) {
    ESP_LOGD(TAG, "sched_getaffinity called (stub implementation)");
    
    (void)pid;
    (void)cpusetsize;
    
    if (mask && cpusetsize >= sizeof(cpu_set_t)) {
        CPU_ZERO(mask);
        CPU_SET(0, mask);
        CPU_SET(1, mask);
        return 0;
    }
    
    errno = EINVAL;
    return -1;
}

void posix_stub_init(void) {
    ESP_LOGI(TAG, "POSIX stub functions initialized");
} 