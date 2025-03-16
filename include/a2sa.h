#ifndef A2SA_H
#define A2SA_H

#include <stdint.h>
#include <stdbool.h>

// 音频格式定义
typedef enum {
    A2SA_FORMAT_PCM_S16LE = 0,  // 16位有符号小端PCM
    A2SA_FORMAT_PCM_S24LE,      // 24位有符号小端PCM
    A2SA_FORMAT_PCM_S32LE,      // 32位有符号小端PCM
    A2SA_FORMAT_FLOAT32LE,      // 32位浮点小端
    A2SA_FORMAT_FLOAT64LE       // 64位浮点小端
} a2sa_format_t;

// 音频参数结构
typedef struct {
    a2sa_format_t format;    // 音频格式
    uint32_t sample_rate;    // 采样率
    uint8_t channels;        // 通道数
    uint32_t frame_size;     // 帧大小
} a2sa_audio_params_t;

// 特征提取配置
typedef struct {
    uint32_t window_size;    // 窗口大小
    uint32_t hop_size;       // 跳跃大小
    uint32_t mel_bands;      // Mel滤波器组数量
    float min_freq;          // 最小频率
    float max_freq;          // 最大频率
    bool use_power;          // 是否使用功率谱
} a2sa_feature_config_t;

// 语音活动检测配置
typedef struct {
    float energy_threshold;  // 能量阈值
    float min_duration;      // 最小持续时间
    float max_silence;       // 最大静音时间
} a2sa_vad_config_t;

// 语音识别结果
typedef struct {
    char *text;             // 识别文本
    float confidence;       // 置信度
    double start_time;      // 开始时间
    double end_time;        // 结束时间
} a2sa_recognition_result_t;

// 音频处理句柄
typedef struct a2sa_handle a2sa_t;

// API函数声明
a2sa_t *a2sa_create(const a2sa_audio_params_t *params);
void a2sa_destroy(a2sa_t *handle);

int a2sa_process_audio(a2sa_t *handle, const void *data, size_t size);
int a2sa_extract_features(a2sa_t *handle, const a2sa_feature_config_t *config);
int a2sa_detect_speech(a2sa_t *handle, const a2sa_vad_config_t *config);
int a2sa_recognize_speech(a2sa_t *handle, a2sa_recognition_result_t *result);

// 高级功能
int a2sa_set_language(a2sa_t *handle, const char *language);
int a2sa_enable_punctuation(a2sa_t *handle, bool enable);
int a2sa_set_model_path(a2sa_t *handle, const char *path);

#endif 