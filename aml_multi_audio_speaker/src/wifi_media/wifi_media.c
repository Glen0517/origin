#include "wifi_media.h"
#include "logger.h"
#include "product_type.h"

#ifdef CONFIG_ENABLE_WIFI_MEDIA
int wifi_media_init(void)
{
    LOG_INFO("WIFI media module init success (CAST/NET PLAY) [HIGH END ONLY]");
    return 0;
}

void wifi_media_deinit(void)
{
    LOG_INFO("WIFI media module deinit success");
}
#else
int wifi_media_init(void) { return 0; }
void wifi_media_deinit(void) {}
#endif