#include "../include/mqtt_client.hpp"
#include "../include/mqtt_config.hpp"
#include <iostream>
#include <chrono>
#include <thread>

// MQTTClient::ActionListener Implementation
void MQTTClient::ActionListener::on_success(const mqtt::token& tok) {
    // silencioso — dispara a cada publicação
}

void MQTTClient::ActionListener::on_failure(const mqtt::token& tok) {
    std::cerr << "[MQTT] Ação falhou: " << tok.get_reason_code() << std::endl;
}

// MQTTClient::Callback Implementation
void MQTTClient::Callback::connection_lost(const std::string& cause) {
    std::cerr << "[MQTT] Conexão perdida: " << cause << std::endl;
    parent_->connected_ = false;
    if (parent_->connection_callback_) {
        parent_->connection_callback_(false);
    }
    // Tentar reconexão automática em background
    std::thread([parent = parent_]() {
        parent->try_reconnect();
    }).detach();
}

void MQTTClient::Callback::message_arrived(mqtt::const_message_ptr msg) {
    std::string topic = msg->get_topic();
    std::string payload = msg->to_string();
    
    // Chamar o callback registrado para este tópico
    auto it = parent_->callbacks_.find(topic);
    if (it != parent_->callbacks_.end()) {
        it->second(topic, payload);
    }
}

void MQTTClient::Callback::delivery_complete(mqtt::delivery_token_ptr token) {
    // silencioso — dispara a cada publicação
}

// MQTTClient Implementation
MQTTClient::MQTTClient(const std::string& broker_address, const std::string& client_id)
    : broker_address_(broker_address), client_id_(client_id),
      connected_(false), reconnect_count_(0) {
    
    try {
        client_ = std::make_unique<mqtt::async_client>(broker_address, client_id);
        callback_ptr_ = std::make_shared<Callback>(this);
        client_->set_callback(*callback_ptr_);
        std::cout << "[MQTT] Cliente MQTT criado: " << client_id << std::endl;
    } catch (const mqtt::exception& exc) {
        std::cerr << "[MQTT] Erro ao criar cliente: " << exc.what() << std::endl;
    }
}

MQTTClient::~MQTTClient() {
    try {
        if (connected_) {
            disconnect();
        }
    } catch (const std::exception& e) {
        std::cerr << "[MQTT] Erro ao desconectar: " << e.what() << std::endl;
    }
}

bool MQTTClient::connect(const std::string& username, const std::string& password) {
    if (!client_) {
        std::cerr << "[MQTT] Cliente não inicializado" << std::endl;
        return false;
    }
    
    try {
        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(MQTT_CONFIG::KEEP_ALIVE_INTERVAL);
        connOpts.set_clean_session(true);
        connOpts.set_automatic_reconnect(true);
        connOpts.set_connect_timeout(5); // timeout de 5 segundos
        
        // Configurar autenticação se fornecida
        if (!username.empty()) {
            connOpts.set_user_name(username);
            if (!password.empty()) {
                connOpts.set_password(password);
            }
        }
        
        std::cout << "[MQTT] Conectando ao broker: " << broker_address_ << std::endl;
        auto tok = client_->connect(connOpts);
        
        // Esperar pela conexão com timeout
        auto connect_status = tok->wait_for(std::chrono::milliseconds(MQTT_CONFIG::CONNECT_TIMEOUT_MS));
        
        if (!connect_status) {
            std::cerr << "[MQTT] Timeout ao conectar ao broker" << std::endl;
            connected_ = false;
            return false;
        }
        
        // Verificar se a conexão realmente foi estabelecida
        if (!client_->is_connected()) {
            std::cerr << "[MQTT] Conexão falhou: broker não confirmou conexão" << std::endl;
            connected_ = false;
            return false;
        }
        
        connected_ = true;
        reconnect_count_ = 0;
        std::cout << "[MQTT] Conectado ao broker com sucesso" << std::endl;
        
        if (connection_callback_) {
            connection_callback_(true);
        }
        
        return true;
    } catch (const mqtt::exception& exc) {
        std::cerr << "[MQTT] Erro na conexão: " << exc.what() << std::endl;
        connected_ = false;
        return false;
    }
}

bool MQTTClient::reconnect() {
    if (!client_) {
        std::cerr << "[MQTT] Cliente não inicializado para reconexão" << std::endl;
        return false;
    }
    
    std::cout << "[MQTT] Tentando reconexão..." << std::endl;
    return connect();
}

void MQTTClient::try_reconnect() {
    const int MAX_RECONNECT_ATTEMPTS = 5;
    const int RECONNECT_DELAY_MS = 2000;
    
    for (int attempt = 1; attempt <= MAX_RECONNECT_ATTEMPTS; attempt++) {
        std::cout << "[MQTT] Tentativa de reconexão " << attempt << "/" 
                  << MAX_RECONNECT_ATTEMPTS << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY_MS));
        
        if (reconnect()) {
            std::cout << "[MQTT] Reconectado com sucesso na tentativa " << attempt << std::endl;
            reconnect_count_ = 0;
            // Re-assinar todos os tópicos após reconexão
            for (const auto& kv : callbacks_) {
                try {
                    auto tok = client_->subscribe(kv.first, MQTT_CONFIG::QOS_COMMANDS);
                    tok->wait_for(std::chrono::milliseconds(MQTT_CONFIG::SUBSCRIBE_TIMEOUT_MS));
                    std::cout << "[MQTT] Re-assinado ao tópico: " << kv.first << std::endl;
                } catch (const mqtt::exception& e) {
                    std::cerr << "[MQTT] Erro ao re-assinar " << kv.first << ": " << e.what() << std::endl;
                }
            }
            return;
        }
        
        reconnect_count_++;
    }
    
    std::cerr << "[MQTT] Falha ao reconectar após " << MAX_RECONNECT_ATTEMPTS 
              << " tentativas" << std::endl;
}

bool MQTTClient::disconnect() {
    if (!client_ || !connected_) {
        return false;
    }
    
    try {
        std::cout << "[MQTT] Desconectando do broker..." << std::endl;
        auto tok = client_->disconnect();
        auto disconnect_status = tok->wait_for(std::chrono::milliseconds(5000));
        
        connected_ = false;
        
        if (!disconnect_status) {
            std::cerr << "[MQTT] Timeout ao desconectar" << std::endl;
        } else {
            std::cout << "[MQTT] Desconectado com sucesso" << std::endl;
        }
        
        if (connection_callback_) {
            connection_callback_(false);
        }
        
        return disconnect_status;
    } catch (const mqtt::exception& exc) {
        std::cerr << "[MQTT] Erro ao desconectar: " << exc.what() << std::endl;
        connected_ = false;
        return false;
    }
}

bool MQTTClient::is_connected() const {
    return connected_ && client_ && client_->is_connected();
}

bool MQTTClient::publish(const std::string& topic, const std::string& payload,
                         int qos, bool retained) {
    if (!is_connected()) {
        std::cerr << "[MQTT] Não conectado ao broker. Impossível publicar em " << topic << std::endl;
        return false;
    }
    
    try {
        auto msg = mqtt::make_message(topic, payload);
        msg->set_qos(qos);
        msg->set_retained(retained);
        
        auto tok = client_->publish(msg);
        auto publish_status = tok->wait_for(std::chrono::milliseconds(5000));
        
        if (!publish_status) {
            std::cerr << "[MQTT] Timeout ao publicar em " << topic << std::endl;
            return false;
        }
        
        return true;
    } catch (const mqtt::exception& exc) {
        std::cerr << "[MQTT] Erro ao publicar: " << exc.what() << std::endl;
        return false;
    }
}

bool MQTTClient::publish_int(const std::string& topic, int value, int qos) {
    return publish(topic, std::to_string(value), qos);
}

bool MQTTClient::publish_bool(const std::string& topic, bool value, int qos) {
    return publish(topic, value ? "1" : "0", qos);
}

bool MQTTClient::subscribe(const std::string& topic, MessageCallback callback, int qos) {
    if (!is_connected()) {
        std::cerr << "[MQTT] Não conectado ao broker. Impossível assinar " << topic << std::endl;
        return false;
    }
    
    try {
        // Armazenar o callback
        callbacks_[topic] = callback;
        
        // Assinar o tópico
        auto tok = client_->subscribe(topic, qos);
        auto subscribe_status = tok->wait_for(std::chrono::milliseconds(MQTT_CONFIG::SUBSCRIBE_TIMEOUT_MS));
        
        if (!subscribe_status) {
            std::cerr << "[MQTT] Timeout ao assinar " << topic << std::endl;
            callbacks_.erase(topic);
            return false;
        }
        
        std::cout << "[MQTT] Assinado ao tópico: " << topic << std::endl;
        
        return true;
    } catch (const mqtt::exception& exc) {
        std::cerr << "[MQTT] Erro ao assinar: " << exc.what() << std::endl;
        callbacks_.erase(topic);
        return false;
    }
}

bool MQTTClient::unsubscribe(const std::string& topic) {
    if (!is_connected()) {
        return false;
    }
    
    try {
        auto tok = client_->unsubscribe(topic);
        tok->wait_for(std::chrono::milliseconds(5000));
        
        callbacks_.erase(topic);
        std::cout << "[MQTT] Desassinado do tópico: " << topic << std::endl;
        
        return true;
    } catch (const mqtt::exception& exc) {
        std::cerr << "[MQTT] Erro ao desassinar: " << exc.what() << std::endl;
        return false;
    }
}

void MQTTClient::set_connection_callback(std::function<void(bool)> callback) {
    connection_callback_ = callback;
}