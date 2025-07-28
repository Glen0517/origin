#include "dft.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <spa/utils/string.h>
#include "../system_log/system_log.h"

static SystemLogService *dft_logger = NULL;

static void dft_add_test_result(DftService *service, DftComponent component, const char *test_name, DftTestStatus status, uint32_t execution_time, const char *failure_details)
{
    DftTestResult *result = calloc(1, sizeof(DftTestResult));
    if (!result) return;

    result->component = component;
    result->test_name = strdup(test_name);
    result->status = status;
    result->execution_time_ms = execution_time;
    result->timestamp = time(NULL);
    if (failure_details)
        result->failure_details = strdup(failure_details);

    pthread_mutex_lock(&service->mutex);
    spa_list_append(&service->test_results, result);
    service->test_count++;
    if (status == DFT_TEST_STATUS_PASSED)
        service->passed_count++;
    else if (status == DFT_TEST_STATUS_FAILED)
        service->failed_count++;
    pthread_mutex_unlock(&service->mutex);
}

static void* dft_test_thread(void *data)
{
    DftService *service = (DftService*)data;
    DftComponent component_filter = *(DftComponent*)service->user_data;
    bool all_passed = true;

    // Audio component tests
    if (component_filter == DFT_COMPONENT_AUDIO || component_filter == (DFT_COMPONENT_AUDIO - 1)) {
        uint64_t start_time = clock();
        bool audio_test_passed = true;
        char failure_details[256] = {0};

        // Test audio routing
        if (audio_test_passed) {
            // Simulated test: Would actual call audio routing API
            if (rand() % 100 > 5) { // 95% pass rate for simulation
                dft_add_test_result(service, DFT_COMPONENT_AUDIO, "Audio Routing Test", DFT_TEST_STATUS_PASSED, (clock() - start_time) * 1000 / CLOCKS_PER_SEC, NULL);
            } else {
                audio_test_passed = false;
                snprintf(failure_details, sizeof(failure_details), "Routing path not found");
                dft_add_test_result(service, DFT_COMPONENT_AUDIO, "Audio Routing Test", DFT_TEST_STATUS_FAILED, (clock() - start_time) * 1000 / CLOCKS_PER_SEC, failure_details);
            }
        }

        all_passed &= audio_test_passed;
    }

    // Network component tests
    if (component_filter == DFT_COMPONENT_NETWORK || component_filter == (DFT_COMPONENT_AUDIO - 1)) {
        uint64_t start_time = clock();
        bool network_test_passed = true;
        char failure_details[256] = {0};

        // Test network connectivity
        if (network_test_passed) {
            // Simulated test
            if (rand() % 100 > 10) { // 90% pass rate
                dft_add_test_result(service, DFT_COMPONENT_NETWORK, "Network Connectivity Test", DFT_TEST_STATUS_PASSED, (clock() - start_time) * 1000 / CLOCKS_PER_SEC, NULL);
            } else {
                network_test_passed = false;
                snprintf(failure_details, sizeof(failure_details), "No active network interfaces");
                dft_add_test_result(service, DFT_COMPONENT_NETWORK, "Network Connectivity Test", DFT_TEST_STATUS_FAILED, (clock() - start_time) * 1000 / CLOCKS_PER_SEC, failure_details);
            }
        }

        all_passed &= network_test_passed;
    }

    // Mark self-test as complete and trigger callback
    pthread_mutex_lock(&service->mutex);
    service->self_test_running = false;
    pthread_mutex_unlock(&service->mutex);

    if (service->test_complete_callback) {
        service->test_complete_callback(service->user_data, all_passed);
    }

    return NULL;
}

DftService* dft_create(struct pw_loop *loop)
{
    DftService *service = calloc(1, sizeof(DftService));
    if (!service) return NULL;

    service->loop = loop;
    spa_list_init(&service->test_results);
    pthread_mutex_init(&service->mutex, NULL);

    // Initialize logger
    dft_logger = system_log_create(loop, "/var/log/dft_service.log", LOG_LEVEL_INFO, true, true);
    system_log_start(dft_logger);
    system_log_log_message(dft_logger, LOG_LEVEL_INFO, "DFT service initialized");

    return service;
}

void dft_destroy(DftService *service)
{
    if (!service) return;

    // Stop any running tests
    dft_stop(service);

    // Free test results
    DftTestResult *result, *tmp;
    spa_list_for_each_safe(result, tmp, &service->test_results) {
        free((void*)result->test_name);
        free((void*)result->failure_details);
        spa_list_remove(result);
        free(result);
    }

    pthread_mutex_destroy(&service->mutex);
    system_log_stop(dft_logger);
    system_log_destroy(dft_logger);
    free(service);
}

int dft_start(DftService *service)
{
    if (!service) return -1;

    system_log_log_message(dft_logger, LOG_LEVEL_INFO, "DFT service started");
    return 0;
}

int dft_stop(DftService *service)
{
    if (!service) return -1;

    pthread_mutex_lock(&service->mutex);
    bool was_running = service->self_test_running || service->diagnostics_running;
    pthread_mutex_unlock(&service->mutex);

    if (was_running) {
        // In a real implementation, we would cancel running threads
        system_log_log_message(dft_logger, LOG_LEVEL_WARN, "DFT service stopped with operations in progress");
    } else {
        system_log_log_message(dft_logger, LOG_LEVEL_INFO, "DFT service stopped");
    }

    return 0;
}

int dft_run_self_tests(DftService *service, DftComponent component_filter)
{
    if (!service) return -1;

    pthread_mutex_lock(&service->mutex);
    if (service->self_test_running) {
        pthread_mutex_unlock(&service->mutex);
        return -2; // Test already running
    }

    // Reset test counters
    service->test_count = 0;
    service->passed_count = 0;
    service->failed_count = 0;
    service->self_test_running = true;
    service->user_data = &component_filter;
    pthread_mutex_unlock(&service->mutex);

    system_log_log_message(dft_logger, LOG_LEVEL_INFO, "Starting DFT self-tests for component: %d", component_filter);

    pthread_t test_thread;
    if (pthread_create(&test_thread, NULL, dft_test_thread, service) != 0) {
        pthread_mutex_lock(&service->mutex);
        service->self_test_running = false;
        pthread_mutex_unlock(&service->mutex);
        system_log_log_message(dft_logger, LOG_LEVEL_ERROR, "Failed to create test thread");
        return -3;
    }

    // Detach the thread so we don't need to join it
    pthread_detach(test_thread);

    return 0;
}

int dft_collect_diagnostics(DftService *service, char *buffer, size_t buffer_size)
{
    if (!service || !buffer || buffer_size == 0) return -1;

    pthread_mutex_lock(&service->mutex);
    service->diagnostics_running = true;
    pthread_mutex_unlock(&service->mutex);

    // Get current time
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // Collect system information (simplified)
    snprintf(buffer, buffer_size, "=== DFT Diagnostics ===\nTimestamp: %s\n\nTest Summary:\nTotal Tests: %u\nPassed: %u\nFailed: %u\nSkipped: %u\n\nLast Test Results:\n",
             time_str, service->test_count, service->passed_count, service->failed_count,
             service->test_count - service->passed_count - service->failed_count);

    // Add last few test results
    size_t remaining = buffer_size - strlen(buffer);
    DftTestResult *result;
    int count = 0;
    spa_list_for_each_reverse(result, &service->test_results) {
        if (count >= 5) break; // Only show last 5 results

        char result_str[256];
        const char *status_str;
        switch (result->status) {
            case DFT_TEST_STATUS_PASSED: status_str = "PASSED";
 break;
            case DFT_TEST_STATUS_FAILED: status_str = "FAILED";
 break;
            case DFT_TEST_STATUS_SKIPPED: status_str = "SKIPPED";
 break;
            case DFT_TEST_STATUS_RUNNING: status_str = "RUNNING";
 break;
            default: status_str = "NOT RUN";
        }

        snprintf(result_str, sizeof(result_str), "[%s] %s: %s (%.2fms)\n",
                 status_str, result->test_name, 
                 (result->component == DFT_COMPONENT_AUDIO ? "Audio" : 
                  result->component == DFT_COMPONENT_NETWORK ? "Network" : 
                  result->component == DFT_COMPONENT_STORAGE ? "Storage" : 
                  result->component == DFT_COMPONENT_HARDWARE ? "Hardware" : "Codecs"),
                 (float)result->execution_time_ms / 1000);

        if (strlen(buffer) + strlen(result_str) < buffer_size) {
            strcat(buffer, result_str);
        } else {
            break;
        }

        count++;
    }

    pthread_mutex_lock(&service->mutex);
    service->diagnostics_running = false;
    pthread_mutex_unlock(&service->mutex);

    return strlen(buffer);
}

const DftTestResult* dft_get_test_results(DftService *service, uint32_t *count)
{
    if (!service || !count) return NULL;

    pthread_mutex_lock(&service->mutex);
    *count = service->test_count;

    // In a real implementation, we would return a copy or a safe reference
    // For simplicity, we return the first result (this is just a demo)
    DftTestResult *first_result = spa_list_first(&service->test_results);
    pthread_mutex_unlock(&service->mutex);

    return first_result;
}

void dft_register_test_complete_callback(DftService *service, void (*callback)(void *user_data, bool all_passed), void *user_data)
{
    if (service) {
        pthread_mutex_lock(&service->mutex);
        service->test_complete_callback = callback;
        service->user_data = user_data;
        pthread_mutex_unlock(&service->mutex);
    }
}