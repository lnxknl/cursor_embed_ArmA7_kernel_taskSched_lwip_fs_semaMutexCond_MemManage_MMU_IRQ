#include "a2sa.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// VAD状态
typedef struct {
    float energy_threshold;
    float min_duration;
    float max_silence;
    
    bool is_speech;
    float speech_duration;
    float silence_duration;
    
    float *energy_buffer;
    size_t buffer_size;
    size_t buffer_pos;
} vad_state_t;

// 创建VAD状态
static vad_state_t *create_vad_state(const a2sa_vad_config_t *config,
                                   size_t buffer_size)
{
    vad_state_t *state = calloc(1, sizeof(vad_state_t));
    if (!state) return NULL;
    
    state->energy_threshold = config->energy_threshold;
    state->min_duration = config->min_duration;
    state->max_silence = config->max_silence;
    
    state->energy_buffer = malloc(buffer_size * sizeof(float));
    if (!state->energy_buffer) {
        free(state);
        return NULL;
    }
    
    state->buffer_size = buffer_size;
    return state;
}

// 销毁VAD状态
static void destroy_vad_state(vad_state_t *state)
{
    if (state) {
        free(state->energy_buffer);
        free(state);
    }
}

// 计算帧能量
static float compute_frame_energy(const float *frame, size_t size)
{
    float energy = 0.0f;
    for (size_t i = 0; i < size; i++) {
        energy += frame[i] * frame[i];
    }
    return energy / size;
}

// 更新VAD状态
static bool update_vad_state(vad_state_t *state,
                           float energy,
                           float frame_duration)
{
    // 更新能量缓冲区
    state->energy_buffer[state->buffer_pos] = energy;
    state->buffer_pos = (state->buffer_pos + 1) % state->buffer_size;
    
    // 计算平均能量
    float avg_energy = 0.0f;
    for (size_t i = 0; i < state->buffer_size; i++) {
        avg_energy += state->energy_buffer[i];
    }
    avg_energy /= state->buffer_size;
    
    bool is_active = avg_energy > state->energy_threshold;
    
    if (is_active) {
        state->speech_duration += frame_duration;
        state->silence_duration = 0.0f;
    } else {
        state->silence_duration += frame_duration;
    }
    
    // 状态转换逻辑
    if (state->is_speech) {
        if (state->silence_duration > state->max_silence) {
            state->is_speech = false;
            state->speech_duration = 0.0f;
        }
    } else {
        if (is_active && state->speech_duration > state->min_duration) {
            state->is_speech = true;
            state->silence_duration = 0.0f;
        }
    }
    
    return state->is_speech;
} 