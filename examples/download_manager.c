#include "mcurl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_DOWNLOADS 10
#define BUFFER_SIZE 8192

// 下载项结构
typedef struct {
    char *url;
    char *filename;
    size_t size;
    size_t downloaded;
    bool completed;
    bool failed;
    char error[256];
    pthread_t thread;
    FILE *file;
} download_item_t;

// 下载管理器结构
typedef struct {
    download_item_t downloads[MAX_DOWNLOADS];
    int download_count;
    pthread_mutex_t lock;
} download_manager_t;

// 写入回调
static size_t write_callback(char *ptr,
                           size_t size,
                           size_t nmemb,
                           void *userdata)
{
    download_item_t *item = (download_item_t *)userdata;
    size_t bytes = size * nmemb;
    
    if (fwrite(ptr, 1, bytes, item->file) != bytes) {
        return 0;
    }
    
    item->downloaded += bytes;
    return bytes;
}

// 进度回调
static int progress_callback(void *userdata,
                           double dltotal,
                           double dlnow,
                           double ultotal,
                           double ulnow)
{
    download_item_t *item = (download_item_t *)userdata;
    item->size = (size_t)dltotal;
    return 0;
}

// 下载线程函数
static void *download_thread(void *arg)
{
    download_item_t *item = (download_item_t *)arg;
    
    // 创建CURL句柄
    mcurl_t *curl = mcurl_init();
    if (!curl) {
        item->failed = true;
        strcpy(item->error, "Failed to initialize CURL");
        return NULL;
    }
    
    // 打开文件
    item->file = fopen(item->filename, "wb");
    if (!item->file) {
        item->failed = true;
        strcpy(item->error, "Failed to open file");
        mcurl_cleanup(curl);
        return NULL;
    }
    
    // 设置选项
    mcurl_setopt_pointer(curl, "WRITE_CALLBACK", write_callback);
    mcurl_setopt_pointer(curl, "WRITE_DATA", item);
    mcurl_setopt_pointer(curl, "PROGRESS_CALLBACK", progress_callback);
    mcurl_setopt_pointer(curl, "PROGRESS_DATA", item);
    mcurl_setopt_long(curl, "FOLLOW_LOCATION", 1);
    mcurl_setopt_string(curl, "USER_AGENT",
                       "Mozilla/5.0 MCurl Download Manager");
    
    // 执行请求
    mcurl_response_t response;
    int ret = mcurl_perform(curl, item->url, &response);
    
    if (ret != MCURL_OK) {
        item->failed = true;
        strncpy(item->error, response.error, sizeof(item->error) - 1);
    } else {
        item->completed = true;
    }
    
    // 清理
    fclose(item->file);
    mcurl_free_response(&response);
    mcurl_cleanup(curl);
    
    return NULL;
}

// 创建下载管理器
static download_manager_t *create_manager(void)
{
    download_manager_t *manager = calloc(1, sizeof(download_manager_t));
    if (!manager) return NULL;
    
    pthread_mutex_init(&manager->lock, NULL);
    return manager;
}

// 添加下载
static int add_download(download_manager_t *manager,
                       const char *url,
                       const char *filename)
{
    pthread_mutex_lock(&manager->lock);
    
    if (manager->download_count >= MAX_DOWNLOADS) {
        pthread_mutex_unlock(&manager->lock);
        return -1;
    }
    
    download_item_t *item = &manager->downloads[manager->download_count];
    item->url = strdup(url);
    item->filename = strdup(filename);
    
    // 创建下载线程
    if (pthread_create(&item->thread, NULL, download_thread, item) != 0) {
        free(item->url);
        free(item->filename);
        pthread_mutex_unlock(&manager->lock);
        return -1;
    }
    
    manager->download_count++;
    pthread_mutex_unlock(&manager->lock);
    return 0;
}

// 显示下载进度
static void show_progress(download_manager_t *manager)
{
    printf("\033[2J\033[H");  // 清屏
    printf("Download Manager Status:\n\n");
    
    pthread_mutex_lock(&manager->lock);
    
    for (int i = 0; i < manager->download_count; i++) {
        download_item_t *item = &manager->downloads[i];
        
        printf("[%d] %s\n", i + 1, item->filename);
        printf("    URL: %s\n", item->url);
        
        if (item->failed) {
            printf("    Status: Failed - %s\n", item->error);
        } else if (item->completed) {
            printf("    Status: Completed\n");
        } else {
            if (item->size > 0) {
                float percent = (float)item->downloaded / item->size * 100;
                printf("    Progress: %.1f%% (%zu/%zu bytes)\n",
                       percent, item->downloaded, item->size);
            } else {
                printf("    Progress: %zu bytes\n", item->downloaded);
            }
        }
        printf("\n");
    }
    
    pthread_mutex_unlock(&manager->lock);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: %s <url> <filename> [url filename ...]\n", argv[0]);
        return 1;
    }
    
    // 创建下载管理器
} 