#include "a2sa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <portaudio.h>

#define SAMPLE_RATE 16000
#define FRAMES_PER_BUFFER 1024
#define NUM_CHANNELS 1

typedef struct {
    a2sa_t *a2sa;
    bool is_running;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} transcription_system_t;

// 音频回调函数
static int audio_callback(const void *input,
                         void *output,
                         unsigned long frameCount,
                         const PaStreamCallbackTimeInfo *timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData)
{
    transcription_system_t *sys = (transcription_system_t *)userData;
    
    // 处理音频数据
    pthread_mutex_lock(&sys->lock);
    a2sa_process_audio(sys->a2sa, input, frameCount * sizeof(float));
    pthread_cond_signal(&sys->cond);
    pthread_mutex_unlock(&sys->lock);
    
    return paContinue;
}

// 处理线程函数
static void *processing_thread(void *arg)
{
    transcription_system_t *sys = (transcription_system_t *)arg;
    
    // 配置特征提取
    a2sa_feature_config_t feature_config = {
        .window_size = 512,
        .hop_size = 256,
        .mel_bands = 80,
        .min_freq = 125.0f,
        .max_freq = 7500.0f,
        .use_power = true
    };
    
    // 配置VAD
    a2sa_vad_config_t vad_config = {
        .energy_threshold = 0.1f,
        .min_duration = 0.3f,
        .max_silence = 0.5f
    };
    
    while (sys->is_running) {
        pthread_mutex_lock(&sys->lock);
        pthread_cond_wait(&sys->cond, &sys->lock);
        
        // 提取特征
        a2sa_extract_features(sys->a2sa, &feature_config);
        
        // 检测语音
        if (a2sa_detect_speech(sys->a2sa, &vad_config) == 1) {
            // 执行识别
            a2sa_recognition_result_t result;
            if (a2sa_recognize_speech(sys->a2sa, &result) == 0) {
                printf("[%.2f - %.2f] %s (%.2f%%)\n",
                       result.start_time,
                       result.end_time,
                       result.text,
                       result.confidence * 100.0f);
                free(result.text);
            }
        }
        
        pthread_mutex_unlock(&sys->lock);
    }
    
    return NULL;
}

int main(int argc, char *argv[])
{
    // 初始化PortAudio
    Pa_Initialize();
    
    // 创建转写系统
    transcription_system_t sys = {0};
    
    // 配置音频参数
    a2sa_audio_params_t audio_params = {
        .format = A2SA_FORMAT_FLOAT32LE,
        .sample_rate = SAMPLE_RATE,
        .channels = NUM_CHANNELS,
        .frame_size = FRAMES_PER_BUFFER
    };
    
    // 创建A2SA实例
    sys.a2sa = a2sa_create(&audio_params);
    if (!sys.a2sa) {
        printf("Failed to create A2SA instance\n");
        return 1;
    }
    
    // 设置语言和模型
    a2sa_set_language(sys.a2sa, "en-US");
    a2sa_enable_punctuation(sys.a2sa, true);
    a2sa_set_model_path(sys.a2sa, "./models");
    
    // 初始化同步原语
    pthread_mutex_init(&sys.lock, NULL);
    pthread_cond_init(&sys.cond, NULL);
    sys.is_running = true;
    
    // 创建处理线程
    pthread_t thread;
    pthread_create(&thread, NULL, processing_thread, &sys);
    
    // 打开音频流
    PaStream *stream;
    PaError err = Pa_OpenDefaultStream(&stream,
                                     NUM_CHANNELS, 0,
                                     paFloat32,
                                     SAMPLE_RATE,
                                     FRAMES_PER_BUFFER,
                                     audio_callback,
                                     &sys);
    
    if (err != paNoError) {
        printf("Failed to open audio stream: %s\n",
               Pa_GetErrorText(err));
        return 1;
    }
    
    // 启动音频流
    Pa_StartStream(stream);
    
    printf("Real-time transcription system started.\n");
    printf("Press Enter to stop...\n");
    getchar();
    
    // 清理
    sys.is_running = false;
    pthread_cond_signal(&sys.cond);
    pthread_join(thread, NULL);
    
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    
    pthread_mutex_destroy(&sys.lock);
    pthread_cond_destroy(&sys.cond);
    
    a2sa_destroy(sys.a2sa);
    
    return 0;
} 