#include "mqtt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <json-c/json.h>

/* Device types */
#define DEVICE_TYPE_TEMPERATURE    1
#define DEVICE_TYPE_HUMIDITY      2
#define DEVICE_TYPE_LIGHT         3
#define DEVICE_TYPE_MOTION        4

/* Topics */
#define TOPIC_TEMPERATURE    "home/sensors/temperature"
#define TOPIC_HUMIDITY      "home/sensors/humidity"
#define TOPIC_LIGHT         "home/sensors/light"
#define TOPIC_MOTION        "home/sensors/motion"
#define TOPIC_CONTROL       "home/control/#"
#define TOPIC_STATUS        "home/status"

/* Global variables */
static mqtt_client_t *client = NULL;
static volatile bool running = true;

/* Device state */
typedef struct {
    float temperature;
    float humidity;
    bool light_on;
    bool motion_detected;
    time_t last_motion;
} device_state_t;

static device_state_t state = {
    .temperature = 20.0,
    .humidity = 50.0,
    .light_on = false,
    .motion_detected = false,
    .last_motion = 0
};

/* Callback functions */
static void on_connect(mqtt_client_t *client, void *user_data)
{
    printf("Connected to MQTT broker\n");
    
    /* Subscribe to control topics */
    mqtt_client_subscribe(client, TOPIC_CONTROL, MQTT_QOS1);
}

static void on_disconnect(mqtt_client_t *client, void *user_data)
{
    printf("Disconnected from MQTT broker\n");
}

static void on_message(mqtt_client_t *client, const char *topic, const void *payload, size_t payload_len, void *user_data)
{
    char *payload_str = strndup(payload, payload_len);
    printf("Received message: topic=%s, payload=%s\n", topic, payload_str);
    
    /* Parse control messages */
    if (strncmp(topic, "home/control/", 13) == 0) {
        const char *device = topic + 13;
        
        if (strcmp(device, "light") == 0) {
            if (strcmp(payload_str, "ON") == 0) {
                state.light_on = true;
            } else if (strcmp(payload_str, "OFF") == 0) {
                state.light_on = false;
            }
            
            /* Publish status update */
            json_object *obj = json_object_new_object();
            json_object_object_add(obj, "device", json_object_new_string("light"));
            json_object_object_add(obj, "state", json_object_new_string(state.light_on ? "ON" : "OFF"));
            
            const char *status = json_object_to_json_string(obj);
            mqtt_client_publish(client, TOPIC_STATUS, status, strlen(status), MQTT_QOS1, true);
            
            json_object_put(obj);
        }
    }
    
    free(payload_str);
}

/* Simulate sensor readings */
static void update_sensors(void)
{
    /* Update temperature (simulate daily variation) */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    float hour = tm->tm_hour + tm->tm_min / 60.0;
    state.temperature = 20.0 + 5.0 * sin(2 * M_PI * (hour - 6) / 24);
    
    /* Update humidity (inverse correlation with temperature) */
    state.humidity = 50.0 - (state.temperature - 20.0) * 2;
    
    /* Simulate random motion detection */
    if (rand() % 100 < 10) {  // 10% chance of motion
        state.motion_detected = true;
        state.last_motion = now;
    } else if (now - state.last_motion > 30) {  // Motion timeout
        state.motion_detected = false;
    }
}

/* Publish sensor data */
static void publish_sensor_data(mqtt_client_t *client)
{
    char payload[100];
    json_object *obj;
    
    /* Temperature */
    obj = json_object_new_object();
    json_object_object_add(obj, "value", json_object_new_double(state.temperature));
    json_object_object_add(obj, "unit", json_object_new_string("C"));
    
    const char *temp_json = json_object_to_json_string(obj);
    mqtt_client_publish(client, TOPIC_TEMPERATURE, temp_json, strlen(temp_json), MQTT_QOS1, true);
    
    json_object_put(obj);
    
    /* Humidity */
    obj = json_object_new_object();
    json_object_object_add(obj, "value", json_object_new_double(state.humidity));
    json_object_object_add(obj, "unit", json_object_new_string("%"));
    
    const char *humid_json = json_object_to_json_string(obj);
    mqtt_client_publish(client, TOPIC_HUMIDITY, humid_json, strlen(humid_json), MQTT_QOS1, true);
    
    json_object_put(obj);
    
    /* Light status */
    snprintf(payload, sizeof(payload), "%s", state.light_on ? "ON" : "OFF");
    mqtt_client_publish(client, TOPIC_LIGHT, payload, strlen(payload), MQTT_QOS1, true);
    
    /* Motion status */
    if (state.motion_detected) {
        obj = json_object_new_object();
        json_object_object_add(obj, "detected", json_object_new_boolean(true));
        json_object_object_add(obj, "timestamp", json_object_new_int64(state.last_motion));
        
        const char *motion_json = json_object_to_json_string(obj);
        mqtt_client_publish(client, TOPIC_MOTION, motion_json, strlen(motion_json), MQTT_QOS1, false);
        
        json_object_put(obj);
    }
}

/* Signal handler */
static void signal_handler(int signum)
{
    running = false;
}

int main(int argc, char *argv[])
{
    /* Set up signal handling */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize random seed */
    srand(time(NULL));
    
    /* Create MQTT client */
    client = mqtt_client_new();
    if (!client) {
        printf("Failed to create MQTT client\n");
        return 1;
    }
    
    /* Configure MQTT client */
    mqtt_config_t config = {
        .client_id = "smart_home_controller",
        .username = "user",
        .password = "password",
        .host = "localhost",
        .port = 1883,
        .keepalive = 60,
        .clean_session = true
    };
    
    if (mqtt_client_set_config(client, &config) != MQTT_OK) {
        printf("Failed to configure MQTT client\n");
        mqtt_client_destroy(client);
        return 1;
    }
    
    /* Set callbacks */
    mqtt_client_set_connect_callback(client, on_connect, NULL);
    mqtt_client_set_disconnect_callback(client, on_disconnect, NULL);
    mqtt_client_set_publish_callback(client, on_message, NULL);
    
    /* Connect to broker */
    if (mqtt_client_connect(client) != MQTT_OK) {
        printf("Failed to connect to MQTT broker\n");
        mqtt_client_destroy(client);
        return 1;
    }
    
    /* Main loop */
    while (running) {
        /* Update sensor readings */
        update_sensors();
        
        /* Publish sensor data */
        publish_sensor_data(client);
        
        /* Process MQTT messages */
        mqtt_client_yield(client, 1000);
        
        /* Wait before next update */
        sleep(5);
    }
    
    /* Cleanup */
    mqtt_client_disconnect(client);
    mqtt_client_destroy(client);
    
    return 0;
} 