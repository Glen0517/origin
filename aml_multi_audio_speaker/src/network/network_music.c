#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "curl/curl.h"
#include "log/aml_log.h"

AML_LOG_DEFINE(network);
#define AML_LOG_DEFAULT AML_LOG_GET_CAT(network)

struct memory {
    char *response;
    size_t size;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct memory *mem = (struct memory *)userp;

    char *ptr = realloc(mem->response, mem->size + realsize + 1);
    if(ptr == NULL) {
        AML_LOGE("Failed to allocate memory");
        return 0;
    }

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;

    return realsize;
}

int network_music_init(void) {
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if(res != CURLE_OK) {
        AML_LOGE("Failed to initialize curl: %s", curl_easy_strerror(res));
        return -1;
    }
    AML_LOGI("Network music service initialized successfully");
    return 0;
}

void network_music_deinit(void) {
    curl_global_cleanup();
    AML_LOGI("Network music service deinitialized");
}

int network_music_get_metadata(const char *url, char **metadata) {
    CURL *curl = curl_easy_init();
    if(!curl) {
        AML_LOGE("Failed to initialize curl");
        return -1;
    }

    struct memory chunk = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        AML_LOGE("Failed to get metadata: %s", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(chunk.response);
        return -1;
    }

    *metadata = chunk.response;
    curl_easy_cleanup(curl);
    AML_LOGI("Got metadata from URL: %s", url);
    return 0;
}

int network_music_stream(const char *url, void (*data_callback)(const char *, size_t)) {
    CURL *curl = curl_easy_init();
    if(!curl) {
        AML_LOGE("Failed to initialize curl");
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, data_callback);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        AML_LOGE("Failed to stream music: %s", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return -1;
    }

    curl_easy_cleanup(curl);
    AML_LOGI("Music streaming completed");
    return 0;
}

int network_music_search(const char *query, char **results) {
    CURL *curl = curl_easy_init();
    if(!curl) {
        AML_LOGE("Failed to initialize curl");
        return -1;
    }

    char url[512];
    snprintf(url, sizeof(url), "https://api.music.example.com/search?q=%s", query);

    struct memory chunk = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        AML_LOGE("Failed to search music: %s", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(chunk.response);
        return -1;
    }

    *results = chunk.response;
    curl_easy_cleanup(curl);
    AML_LOGI("Searched for music: %s", query);
    return 0;
}