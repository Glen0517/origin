#ifndef DFT_H
#define DFT_H

#include <stdint.h>
#include <stdbool.h>
#include <pipewire/pipewire.h>
#include <spa/utils/list.h>

typedef enum {
    DFT_TEST_STATUS_NOT_RUN,
    DFT_TEST_STATUS_RUNNING,
    DFT_TEST_STATUS_PASSED,
    DFT_TEST_STATUS_FAILED,
    DFT_TEST_STATUS_SKIPPED
} DftTestStatus;

typedef enum {
    DFT_COMPONENT_AUDIO,
    DFT_COMPONENT_NETWORK,
    DFT_COMPONENT_STORAGE,
    DFT_COMPONENT_HARDWARE,
    DFT_COMPONENT_CODECS
} DftComponent;

typedef struct {
    DftComponent component;
    const char *test_name;
    DftTestStatus status;
    uint32_t execution_time_ms;
    const char *failure_details;
    uint64_t timestamp;
} DftTestResult;

typedef struct {
    struct pw_core *core;
    struct pw_loop *loop;
    bool self_test_running;
    bool diagnostics_running;
    struct spa_list test_results;
    uint32_t test_count;
    uint32_t passed_count;
    uint32_t failed_count;
    pthread_mutex_t mutex;
    void (*test_complete_callback)(void *user_data, bool all_passed);
    void *user_data;
} DftService;

DftService* dft_create(struct pw_loop *loop);
void dft_destroy(DftService *service);
int dft_start(DftService *service);
int dft_stop(DftService *service);
int dft_run_self_tests(DftService *service, DftComponent component_filter);
int dft_collect_diagnostics(DftService *service, char *buffer, size_t buffer_size);
const DftTestResult* dft_get_test_results(DftService *service, uint32_t *count);
void dft_register_test_complete_callback(DftService *service, void (*callback)(void *user_data, bool all_passed), void *user_data);

#endif // DFT_H