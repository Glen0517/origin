#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <stdint.h>
#include <stdbool.h>
#include <pipewire/pipewire.h>
#include <spa/utils/list.h>

#define OTA_UPDATE_MAX_VERSION_LENGTH 64
#define OTA_UPDATE_MAX_URL_LENGTH 256
#define OTA_UPDATE_MAX_DESCRIPTION_LENGTH 512

typedef enum {
    OTA_STATUS_IDLE,
    OTA_STATUS_CHECKING,
    OTA_STATUS_UPDATE_AVAILABLE,
    OTA_STATUS_DOWNLOADING,
    OTA_STATUS_DOWNLOADED,
    OTA_STATUS_INSTALLING,
    OTA_STATUS_INSTALLED,
    OTA_STATUS_FAILED,
    OTA_STATUS_CANCELED,
    OTA_STATUS_ROLLING_BACK
} OtaUpdateStatus;

typedef struct {
    char version[OTA_UPDATE_MAX_VERSION_LENGTH];
    char download_url[OTA_UPDATE_MAX_URL_LENGTH];
    uint64_t file_size;
    char description[OTA_UPDATE_MAX_DESCRIPTION_LENGTH];
    uint32_t checksum;
    bool critical_update;
} OtaUpdateInfo;

typedef struct {
    struct pw_loop *loop;
    OtaUpdateStatus status;
    OtaUpdateInfo current_update;
    char current_version[OTA_UPDATE_MAX_VERSION_LENGTH];
    int download_progress;
    int install_progress;
    pthread_mutex_t mutex;
    pthread_t update_thread;
    bool update_in_progress;
    bool rollback_enabled;
    void (*status_changed_callback)(OtaUpdateStatus status, int progress, void *user_data);
    void (*update_available_callback)(const OtaUpdateInfo *update_info, void *user_data);
    void *user_data;
} OtaUpdateService;

OtaUpdateService* ota_update_create(struct pw_loop *loop, const char *current_version, bool enable_rollback);
void ota_update_destroy(OtaUpdateService *service);
int ota_update_start(OtaUpdateService *service);
int ota_update_stop(OtaUpdateService *service);
int ota_update_check_for_updates(OtaUpdateService *service, const char *update_server_url);
int ota_update_download_update(OtaUpdateService *service);
int ota_update_install_update(OtaUpdateService *service);
int ota_update_cancel_update(OtaUpdateService *service);
int ota_update_rollback(OtaUpdateService *service);
OtaUpdateStatus ota_update_get_status(OtaUpdateService *service);
const OtaUpdateInfo* ota_update_get_current_update(OtaUpdateService *service);
void ota_update_register_status_callback(OtaUpdateService *service, void (*callback)(OtaUpdateStatus status, int progress, void *user_data), void *user_data);
void ota_update_register_available_callback(OtaUpdateService *service, void (*callback)(const OtaUpdateInfo *update_info, void *user_data), void *user_data);

#endif // OTA_UPDATE_H