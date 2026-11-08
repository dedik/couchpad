#pragma once

// Klient MQTT 3.1.1 po zwykłym TCP z API esp-mqtt, w zakresie używanym przez firmware.
// Każdy klient ma własny wątek, który łączy, odbiera i woła handlery zdarzeń — tak jak
// zadanie esp-mqtt. Wiadomości przychodzą w całości (`current_data_offset == 0`,
// `data_len == total_data_len`), więc ścieżki sklejania w klientach firmware'u
// dostają zawsze jeden kawałek.

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct compat_mqtt_client* esp_mqtt_client_handle_t;

typedef enum {
    MQTT_EVENT_ANY = -1,
    MQTT_EVENT_ERROR = 0,
    MQTT_EVENT_CONNECTED,
    MQTT_EVENT_DISCONNECTED,
    MQTT_EVENT_SUBSCRIBED,
    MQTT_EVENT_UNSUBSCRIBED,
    MQTT_EVENT_PUBLISHED,
    MQTT_EVENT_DATA,
    MQTT_EVENT_BEFORE_CONNECT,
    MQTT_EVENT_DELETED,
} esp_mqtt_event_id_t;

typedef enum {
    MQTT_ERROR_TYPE_NONE = 0,
    MQTT_ERROR_TYPE_TCP_TRANSPORT,
    MQTT_ERROR_TYPE_CONNECTION_REFUSED,
    MQTT_ERROR_TYPE_SUBSCRIBE_FAILED,
} esp_mqtt_error_type_t;

typedef enum {
    MQTT_CONNECTION_ACCEPTED = 0,
    MQTT_CONNECTION_REFUSE_PROTOCOL,
    MQTT_CONNECTION_REFUSE_ID_REJECTED,
    MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE,
    MQTT_CONNECTION_REFUSE_BAD_USERNAME,
    MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED,
} esp_mqtt_connect_return_code_t;

typedef enum {
    MQTT_TRANSPORT_UNKNOWN = 0,
    MQTT_TRANSPORT_OVER_TCP,
    MQTT_TRANSPORT_OVER_SSL,
    MQTT_TRANSPORT_OVER_WS,
    MQTT_TRANSPORT_OVER_WSS,
} esp_mqtt_transport_t;

typedef struct {
    esp_err_t esp_tls_last_esp_err;
    int esp_tls_stack_err;
    int esp_tls_cert_verify_flags;
    esp_mqtt_error_type_t error_type;
    esp_mqtt_connect_return_code_t connect_return_code;
    int esp_transport_sock_errno;
} esp_mqtt_error_codes_t;

typedef struct {
    esp_mqtt_event_id_t event_id;
    esp_mqtt_client_handle_t client;
    char* data;
    int data_len;
    int total_data_len;
    int current_data_offset;
    char* topic;
    int topic_len;
    int msg_id;
    int session_present;
    esp_mqtt_error_codes_t* error_handle;
    bool retain;
    int qos;
    bool dup;
} esp_mqtt_event_t;

typedef esp_mqtt_event_t* esp_mqtt_event_handle_t;

typedef struct {
    struct {
        struct {
            const char* uri;
            const char* hostname;
            esp_mqtt_transport_t transport;
            const char* path;
            uint32_t port;
        } address;
    } broker;
    struct {
        const char* username;
        const char* client_id;
        bool set_null_client_id;
        struct {
            const char* password;
        } authentication;
    } credentials;
    struct {
        struct {
            const char* topic;
            const char* msg;
            int msg_len;
            int qos;
            int retain;
        } last_will;
        bool disable_clean_session;
        int keepalive;
        bool disable_keepalive;
    } session;
    struct {
        int reconnect_timeout_ms;
        int timeout_ms;
        int refresh_connection_after_ms;
        bool disable_auto_reconnect;
    } network;
    struct {
        int size;
        int out_size;
    } buffer;
} esp_mqtt_client_config_t;

esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t* config);
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client,
                                         esp_mqtt_event_id_t event, esp_event_handler_t handler,
                                         void* arg);
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client);
esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t client);
esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t client);
esp_err_t esp_mqtt_client_reconnect(esp_mqtt_client_handle_t client);
esp_err_t esp_mqtt_client_disconnect(esp_mqtt_client_handle_t client);

// Zwraca identyfikator wiadomości (0 dla QoS 0) albo -1 bez połączenia.
int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char* topic,
                            const char* data, int len, int qos, int retain);
int esp_mqtt_client_enqueue(esp_mqtt_client_handle_t client, const char* topic,
                            const char* data, int len, int qos, int retain, bool store);
int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client, const char* topic, int qos);
int esp_mqtt_client_unsubscribe(esp_mqtt_client_handle_t client, const char* topic);

#ifdef __cplusplus
}
#endif
