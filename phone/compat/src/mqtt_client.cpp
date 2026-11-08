// Minimalny klient MQTT 3.1.1 (TCP, bez TLS) z API esp-mqtt.
//
// Wątek klienta łączy się, wysyła CONNECT z testamentem, a po CONNACK woła handler
// MQTT_EVENT_CONNECTED — w nim klienci firmware'u subskrybują swoje tematy, tak jak
// w zadaniu esp-mqtt. Publikacja może przyjść z dowolnego wątku (zadanie wysyłki
// MqttBus); zapis do gniazda chroni osobny mutex.

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "esp_log.h"
#include "mqtt_client.h"

namespace {

constexpr char kTag[] = "mqtt_compat";
constexpr size_t kMaxPacket = 1024 * 1024;
const esp_event_base_t kMqttBase = "MQTT_EVENTS";

using Clock = std::chrono::steady_clock;

enum PacketType : uint8_t {
    kConnect = 1,
    kConnack = 2,
    kPublish = 3,
    kPuback = 4,
    kPubrec = 5,
    kPubrel = 6,
    kPubcomp = 7,
    kSubscribe = 8,
    kSuback = 9,
    kUnsubscribe = 10,
    kUnsuback = 11,
    kPingreq = 12,
    kPingresp = 13,
    kDisconnect = 14,
};

void put_u16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value & 0xff));
}

void put_string(std::vector<uint8_t>& out, const std::string& value) {
    put_u16(out, static_cast<uint16_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

std::vector<uint8_t> frame(uint8_t header, const std::vector<uint8_t>& body) {
    std::vector<uint8_t> out;
    out.push_back(header);
    size_t length = body.size();
    do {
        uint8_t byte = length % 128;
        length /= 128;
        if (length > 0) byte |= 0x80;
        out.push_back(byte);
    } while (length > 0);
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

struct Handler {
    esp_mqtt_event_id_t event;
    esp_event_handler_t handler;
    void* arg;
};

}  // namespace

struct compat_mqtt_client {
    std::string host;
    uint16_t port = 1883;
    std::string client_id;
    std::string username;
    std::string password;
    bool has_credentials = false;
    std::string will_topic;
    std::string will_message;
    int will_qos = 0;
    bool will_retain = false;
    int keepalive_s = 120;
    int reconnect_ms = 10000;
    int timeout_ms = 10000;

    std::vector<Handler> handlers;

    std::thread worker;
    std::atomic<bool> running{false};
    std::mutex sleep_mutex;
    std::condition_variable sleep_cv;

    std::mutex write_mutex;
    std::atomic<int> fd{-1};
    std::atomic<bool> connected{false};
    std::atomic<uint16_t> next_id{1};
    Clock::time_point last_sent{};

    uint16_t allocate_id() {
        uint16_t id = next_id.fetch_add(1);
        if (id == 0) id = next_id.fetch_add(1);
        return id;
    }

    void dispatch(esp_mqtt_event_t& event) {
        event.client = this;
        for (const auto& handler : handlers) {
            if (handler.event == MQTT_EVENT_ANY || handler.event == event.event_id) {
                handler.handler(handler.arg, kMqttBase, event.event_id, &event);
            }
        }
    }

    void dispatch_simple(esp_mqtt_event_id_t id) {
        esp_mqtt_event_t event{};
        event.event_id = id;
        dispatch(event);
    }

    void dispatch_error(esp_mqtt_error_type_t type, esp_mqtt_connect_return_code_t code,
                        int sock_errno) {
        esp_mqtt_error_codes_t error{};
        error.error_type = type;
        error.connect_return_code = code;
        error.esp_transport_sock_errno = sock_errno;
        esp_mqtt_event_t event{};
        event.event_id = MQTT_EVENT_ERROR;
        event.error_handle = &error;
        dispatch(event);
    }

    bool send_all(const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(write_mutex);
        const int socket_fd = fd.load();
        if (socket_fd < 0) return false;
        size_t sent = 0;
        while (sent < data.size()) {
#ifdef MSG_NOSIGNAL
            const ssize_t n = ::send(socket_fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
#else
            const ssize_t n = ::send(socket_fd, data.data() + sent, data.size() - sent, 0);
#endif
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) return false;
            sent += static_cast<size_t>(n);
        }
        last_sent = Clock::now();
        return true;
    }

    // Czeka na dane do `deadline`. false przy błędzie, końcu połączenia albo zatrzymaniu.
    bool read_exact(uint8_t* out, size_t length, Clock::time_point deadline) {
        size_t got = 0;
        while (got < length) {
            if (!running) return false;
            const auto left =
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
            if (left.count() <= 0) return false;
            pollfd p{fd.load(), POLLIN, 0};
            const int ready = ::poll(&p, 1, static_cast<int>(std::min<long long>(left.count(), 500)));
            if (ready < 0 && errno == EINTR) continue;
            if (ready < 0) return false;
            if (ready == 0) continue;
            const ssize_t n = ::recv(p.fd, out + got, length - got, 0);
            if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
            if (n <= 0) return false;
            got += static_cast<size_t>(n);
        }
        return true;
    }

    bool read_packet(uint8_t& header, std::vector<uint8_t>& body, Clock::time_point deadline) {
        if (!read_exact(&header, 1, deadline)) return false;
        size_t length = 0;
        size_t multiplier = 1;
        for (int i = 0; i < 4; ++i) {
            uint8_t byte = 0;
            if (!read_exact(&byte, 1, deadline)) return false;
            length += (byte & 0x7f) * multiplier;
            multiplier *= 128;
            if ((byte & 0x80) == 0) break;
            if (i == 3) return false;
        }
        if (length > kMaxPacket) {
            ESP_LOGW(kTag, "za duza wiadomosc: %zu B", length);
            return false;
        }
        body.resize(length);
        // Ciało pakietu ma już swój nagłówek, więc dajemy mu osobny, hojny limit.
        const auto body_deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
        return length == 0 || read_exact(body.data(), length, body_deadline);
    }

    int open_socket(int& out_errno) {
        out_errno = 0;
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* result = nullptr;
        const std::string service = std::to_string(port);
        if (getaddrinfo(host.c_str(), service.c_str(), &hints, &result) != 0 || result == nullptr) {
            out_errno = EHOSTUNREACH;
            return -1;
        }
        int socket_fd = -1;
        for (addrinfo* ai = result; ai != nullptr && socket_fd < 0; ai = ai->ai_next) {
            socket_fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
            if (socket_fd < 0) continue;
#ifdef SO_NOSIGPIPE
            int one_sig = 1;
            setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE, &one_sig, sizeof(one_sig));
#endif
            const int flags = fcntl(socket_fd, F_GETFL, 0);
            fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
            int rc = ::connect(socket_fd, ai->ai_addr, ai->ai_addrlen);
            if (rc != 0 && errno == EINPROGRESS) {
                pollfd p{socket_fd, POLLOUT, 0};
                rc = ::poll(&p, 1, timeout_ms) == 1 ? 0 : -1;
                if (rc == 0) {
                    int error = 0;
                    socklen_t len = sizeof(error);
                    getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &error, &len);
                    if (error != 0) {
                        out_errno = error;
                        rc = -1;
                    }
                } else if (out_errno == 0) {
                    out_errno = ETIMEDOUT;
                }
            } else if (rc != 0) {
                out_errno = errno;
            }
            if (rc != 0) {
                ::close(socket_fd);
                socket_fd = -1;
                continue;
            }
            fcntl(socket_fd, F_SETFL, flags);
            int one = 1;
            setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        }
        freeaddrinfo(result);
        return socket_fd;
    }

    void close_socket() {
        std::lock_guard<std::mutex> lock(write_mutex);
        const int socket_fd = fd.exchange(-1);
        if (socket_fd >= 0) {
            ::shutdown(socket_fd, SHUT_RDWR);
            ::close(socket_fd);
        }
    }

    std::vector<uint8_t> connect_packet() const {
        std::vector<uint8_t> body;
        put_string(body, "MQTT");
        body.push_back(4);  // MQTT 3.1.1
        uint8_t flags = 0x02;  // czysta sesja
        if (!will_topic.empty()) {
            flags |= 0x04;
            flags |= static_cast<uint8_t>((will_qos & 3) << 3);
            if (will_retain) flags |= 0x20;
        }
        if (has_credentials) {
            flags |= 0x80;
            if (!password.empty()) flags |= 0x40;
        }
        body.push_back(flags);
        put_u16(body, static_cast<uint16_t>(keepalive_s));
        put_string(body, client_id);
        if (!will_topic.empty()) {
            put_string(body, will_topic);
            put_string(body, will_message);
        }
        if (has_credentials) {
            put_string(body, username);
            if (!password.empty()) put_string(body, password);
        }
        return frame(kConnect << 4, body);
    }

    void sleep_before_retry() {
        std::unique_lock<std::mutex> lock(sleep_mutex);
        sleep_cv.wait_for(lock, std::chrono::milliseconds(reconnect_ms), [this] { return !running; });
    }

    void handle_publish(uint8_t header, std::vector<uint8_t>& body) {
        const int qos = (header >> 1) & 3;
        if (body.size() < 2) return;
        const size_t topic_len = (static_cast<size_t>(body[0]) << 8) | body[1];
        size_t offset = 2 + topic_len;
        if (offset > body.size()) return;
        uint16_t packet_id = 0;
        if (qos > 0) {
            if (offset + 2 > body.size()) return;
            packet_id = static_cast<uint16_t>((body[offset] << 8) | body[offset + 1]);
            offset += 2;
        }
        esp_mqtt_event_t event{};
        event.event_id = MQTT_EVENT_DATA;
        event.topic = reinterpret_cast<char*>(body.data() + 2);
        event.topic_len = static_cast<int>(topic_len);
        event.data = reinterpret_cast<char*>(body.data() + offset);
        event.data_len = static_cast<int>(body.size() - offset);
        event.total_data_len = event.data_len;
        event.current_data_offset = 0;
        event.qos = qos;
        event.retain = (header & 1) != 0;
        event.dup = (header & 8) != 0;
        event.msg_id = packet_id;
        dispatch(event);
        if (qos == 1) {
            std::vector<uint8_t> ack;
            put_u16(ack, packet_id);
            send_all(frame(kPuback << 4, ack));
        } else if (qos == 2) {
            std::vector<uint8_t> rec;
            put_u16(rec, packet_id);
            send_all(frame(kPubrec << 4, rec));
        }
    }

    void run() {
        while (running) {
            int sock_errno = 0;
            const int socket_fd = open_socket(sock_errno);
            if (socket_fd < 0) {
                if (!running) break;
                ESP_LOGW(kTag, "brak polaczenia z brokerem (errno %d)", sock_errno);
                dispatch_error(MQTT_ERROR_TYPE_TCP_TRANSPORT, MQTT_CONNECTION_ACCEPTED, sock_errno);
                dispatch_simple(MQTT_EVENT_DISCONNECTED);
                sleep_before_retry();
                continue;
            }
            fd = socket_fd;
            if (!send_all(connect_packet())) {
                close_socket();
                dispatch_error(MQTT_ERROR_TYPE_TCP_TRANSPORT, MQTT_CONNECTION_ACCEPTED, errno);
                dispatch_simple(MQTT_EVENT_DISCONNECTED);
                sleep_before_retry();
                continue;
            }
            uint8_t header = 0;
            std::vector<uint8_t> body;
            const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
            if (!read_packet(header, body, deadline) || (header >> 4) != kConnack ||
                body.size() < 2) {
                close_socket();
                if (!running) break;
                dispatch_error(MQTT_ERROR_TYPE_TCP_TRANSPORT, MQTT_CONNECTION_ACCEPTED, ETIMEDOUT);
                dispatch_simple(MQTT_EVENT_DISCONNECTED);
                sleep_before_retry();
                continue;
            }
            if (body[1] != 0) {
                ESP_LOGW(kTag, "broker odmowil polaczenia, kod %d", body[1]);
                close_socket();
                dispatch_error(MQTT_ERROR_TYPE_CONNECTION_REFUSED,
                               static_cast<esp_mqtt_connect_return_code_t>(body[1]), 0);
                dispatch_simple(MQTT_EVENT_DISCONNECTED);
                sleep_before_retry();
                continue;
            }

            connected = true;
            ESP_LOGI(kTag, "polaczono z brokerem");
            {
                esp_mqtt_event_t event{};
                event.event_id = MQTT_EVENT_CONNECTED;
                event.session_present = body[0] & 1;
                dispatch(event);
            }

            auto last_received = Clock::now();
            bool ping_pending = false;
            const auto keepalive = std::chrono::seconds(keepalive_s);
            while (running) {
                if (Clock::now() - last_sent >= keepalive / 2 && !ping_pending) {
                    if (!send_all(frame(kPingreq << 4, {}))) break;
                    ping_pending = true;
                }
                if (Clock::now() - last_received > keepalive + keepalive / 2) {
                    ESP_LOGW(kTag, "broker milczy, zrywam polaczenie");
                    break;
                }
                pollfd p{fd.load(), POLLIN, 0};
                const int ready = ::poll(&p, 1, 500);
                if (ready < 0 && errno == EINTR) continue;
                if (ready < 0) break;
                if (ready == 0) continue;
                if (!read_packet(header, body,
                                 Clock::now() + std::chrono::milliseconds(timeout_ms))) {
                    break;
                }
                last_received = Clock::now();
                switch (header >> 4) {
                    case kPublish:
                        handle_publish(header, body);
                        break;
                    case kPubrel: {
                        if (body.size() >= 2) {
                            std::vector<uint8_t> comp(body.begin(), body.begin() + 2);
                            send_all(frame(kPubcomp << 4, comp));
                        }
                        break;
                    }
                    case kPingresp:
                        ping_pending = false;
                        break;
                    case kSuback: {
                        esp_mqtt_event_t event{};
                        event.event_id = MQTT_EVENT_SUBSCRIBED;
                        if (body.size() >= 2) event.msg_id = (body[0] << 8) | body[1];
                        dispatch(event);
                        break;
                    }
                    case kPuback: {
                        esp_mqtt_event_t event{};
                        event.event_id = MQTT_EVENT_PUBLISHED;
                        if (body.size() >= 2) event.msg_id = (body[0] << 8) | body[1];
                        dispatch(event);
                        break;
                    }
                    default:
                        break;
                }
            }

            const bool was_running = running;
            connected = false;
            close_socket();
            if (was_running) {
                ESP_LOGW(kTag, "rozlaczono z brokerem");
                dispatch_error(MQTT_ERROR_TYPE_TCP_TRANSPORT, MQTT_CONNECTION_ACCEPTED, ECONNRESET);
            }
            dispatch_simple(MQTT_EVENT_DISCONNECTED);
            if (was_running) sleep_before_retry();
        }
    }
};

extern "C" {

esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t* config) {
    if (config == nullptr) return nullptr;
    auto* client = new compat_mqtt_client;
    if (config->broker.address.hostname != nullptr) {
        client->host = config->broker.address.hostname;
    }
    if (config->broker.address.port != 0) {
        client->port = static_cast<uint16_t>(config->broker.address.port);
    }
    if (config->credentials.client_id != nullptr) {
        client->client_id = config->credentials.client_id;
    }
    if (config->credentials.username != nullptr) {
        client->username = config->credentials.username;
        client->has_credentials = true;
        if (config->credentials.authentication.password != nullptr) {
            client->password = config->credentials.authentication.password;
        }
    }
    if (config->session.last_will.topic != nullptr) {
        client->will_topic = config->session.last_will.topic;
        if (config->session.last_will.msg != nullptr) {
            client->will_message =
                config->session.last_will.msg_len > 0
                    ? std::string(config->session.last_will.msg, config->session.last_will.msg_len)
                    : std::string(config->session.last_will.msg);
        }
        client->will_qos = config->session.last_will.qos;
        client->will_retain = config->session.last_will.retain != 0;
    }
    if (config->session.keepalive > 0) client->keepalive_s = config->session.keepalive;
    if (config->network.reconnect_timeout_ms > 0) {
        client->reconnect_ms = config->network.reconnect_timeout_ms;
    }
    if (config->network.timeout_ms > 0) client->timeout_ms = config->network.timeout_ms;
    if (client->host.empty()) {
        delete client;
        return nullptr;
    }
    return client;
}

esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client,
                                         esp_mqtt_event_id_t event, esp_event_handler_t handler,
                                         void* arg) {
    if (client == nullptr || handler == nullptr) return ESP_ERR_INVALID_ARG;
    client->handlers.push_back(Handler{event, handler, arg});
    return ESP_OK;
}

esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client) {
    if (client == nullptr) return ESP_ERR_INVALID_ARG;
    if (client->running) return ESP_FAIL;
    if (client->worker.joinable()) client->worker.join();
    client->running = true;
    client->worker = std::thread([client] { client->run(); });
    return ESP_OK;
}

esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t client) {
    if (client == nullptr) return ESP_ERR_INVALID_ARG;
    if (!client->running) return ESP_FAIL;
    if (client->connected) {
        client->send_all(frame(kDisconnect << 4, {}));
    }
    {
        std::lock_guard<std::mutex> lock(client->sleep_mutex);
        client->running = false;
    }
    client->sleep_cv.notify_all();
    {
        // Odblokowuje poll/recv wątku klienta.
        std::lock_guard<std::mutex> lock(client->write_mutex);
        const int socket_fd = client->fd.load();
        if (socket_fd >= 0) ::shutdown(socket_fd, SHUT_RDWR);
    }
    if (client->worker.joinable()) {
        if (client->worker.get_id() == std::this_thread::get_id()) {
            client->worker.detach();
        } else {
            client->worker.join();
        }
    }
    return ESP_OK;
}

esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t client) {
    if (client == nullptr) return ESP_ERR_INVALID_ARG;
    if (client->running) esp_mqtt_client_stop(client);
    delete client;
    return ESP_OK;
}

esp_err_t esp_mqtt_client_reconnect(esp_mqtt_client_handle_t client) {
    if (client == nullptr) return ESP_ERR_INVALID_ARG;
    client->sleep_cv.notify_all();
    return ESP_OK;
}

esp_err_t esp_mqtt_client_disconnect(esp_mqtt_client_handle_t client) {
    if (client == nullptr) return ESP_ERR_INVALID_ARG;
    client->close_socket();
    return ESP_OK;
}

int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char* topic, const char* data,
                            int len, int qos, int retain) {
    if (client == nullptr || topic == nullptr || !client->connected) return -1;
    const size_t length = len > 0 ? static_cast<size_t>(len) : (data ? std::strlen(data) : 0);
    std::vector<uint8_t> body;
    put_string(body, topic);
    uint16_t id = 0;
    qos = qos < 0 ? 0 : qos > 2 ? 2 : qos;
    if (qos > 0) {
        id = client->allocate_id();
        put_u16(body, id);
    }
    if (length > 0) body.insert(body.end(), data, data + length);
    const uint8_t header =
        static_cast<uint8_t>((kPublish << 4) | (qos << 1) | (retain ? 1 : 0));
    return client->send_all(frame(header, body)) ? id : -1;
}

int esp_mqtt_client_enqueue(esp_mqtt_client_handle_t client, const char* topic, const char* data,
                            int len, int qos, int retain, bool) {
    return esp_mqtt_client_publish(client, topic, data, len, qos, retain);
}

int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client, const char* topic, int qos) {
    if (client == nullptr || topic == nullptr || !client->connected) return -1;
    std::vector<uint8_t> body;
    const uint16_t id = client->allocate_id();
    put_u16(body, id);
    put_string(body, topic);
    body.push_back(static_cast<uint8_t>(qos & 3));
    return client->send_all(frame((kSubscribe << 4) | 0x02, body)) ? id : -1;
}

int esp_mqtt_client_unsubscribe(esp_mqtt_client_handle_t client, const char* topic) {
    if (client == nullptr || topic == nullptr || !client->connected) return -1;
    std::vector<uint8_t> body;
    const uint16_t id = client->allocate_id();
    put_u16(body, id);
    put_string(body, topic);
    return client->send_all(frame((kUnsubscribe << 4) | 0x02, body)) ? id : -1;
}

}  // extern "C"
