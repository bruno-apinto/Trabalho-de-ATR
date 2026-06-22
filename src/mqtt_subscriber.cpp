#include "../include/mqtt_subscriber.hpp"
#include <iostream>
#include <chrono>

MQTTSubscriber::MQTTSubscriber(MemoriaCompartilhada* shm, std::mutex& shm_mutex)
    : shm_(shm), shm_mutex_(shm_mutex), running_(false) {
    
    mqtt_client_ = std::make_unique<MQTTClient>(
        MQTT_CONFIG::BROKER_ADDRESS,
        MQTT_CONFIG::CLIENT_ID_SUBSCRIBER
    );
}

MQTTSubscriber::~MQTTSubscriber() {
    stop();
}

bool MQTTSubscriber::start() {
    if (running_) {
        std::cout << "[Subscriber] Já está rodando" << std::endl;
        return true;
    }
    
    // Conectar ao broker
    if (!mqtt_client_->connect()) {
        std::cerr << "[Subscriber] Falha ao conectar ao broker" << std::endl;
        return false;
    }
    
    // Assinar aos tópicos de comando
    mqtt_client_->subscribe(
        MQTT_CONFIG::SUBSCRIBE_TOPICS::COMMAND_NAVIGATION,
        [this](const std::string& topic, const std::string& payload) {
            this->on_navigation_command(topic, payload);
        },
        MQTT_CONFIG::QOS_COMMANDS
    );
    
    mqtt_client_->subscribe(
        MQTT_CONFIG::SUBSCRIBE_TOPICS::COMMAND_MODE,
        [this](const std::string& topic, const std::string& payload) {
            this->on_mode_command(topic, payload);
        },
        MQTT_CONFIG::QOS_COMMANDS
    );
    
    mqtt_client_->subscribe(
        MQTT_CONFIG::SUBSCRIBE_TOPICS::COMMAND_VELOCITY,
        [this](const std::string& topic, const std::string& payload) {
            this->on_velocity_command(topic, payload);
        },
        MQTT_CONFIG::QOS_COMMANDS
    );
    
    mqtt_client_->subscribe(
        MQTT_CONFIG::SUBSCRIBE_TOPICS::COMMAND_CAMERA,
        [this](const std::string& topic, const std::string& payload) {
            this->on_camera_command(topic, payload);
        },
        MQTT_CONFIG::QOS_COMMANDS
    );

    mqtt_client_->subscribe(
        MQTT_CONFIG::SUBSCRIBE_TOPICS::COMMAND_THRESHOLD,
        [this](const std::string& topic, const std::string& payload) {
            this->on_threshold_command(topic, payload);
        },
        MQTT_CONFIG::QOS_COMMANDS
    );

    running_ = true;
    
    // Iniciar thread dedicada para o subscritor
    subscriber_thread_ = std::thread(&MQTTSubscriber::subscriber_loop, this);
    
    std::cout << "[Subscriber] Subscritor MQTT iniciado com thread dedicada" << std::endl;
    
    return true;
}

void MQTTSubscriber::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Aguardar thread do subscritor finalizar
    if (subscriber_thread_.joinable()) {
        subscriber_thread_.join();
    }
    
    // Desassinar de todos os tópicos
    mqtt_client_->unsubscribe(MQTT_CONFIG::SUBSCRIBE_TOPICS::COMMAND_NAVIGATION);
    mqtt_client_->unsubscribe(MQTT_CONFIG::SUBSCRIBE_TOPICS::COMMAND_MODE);
    mqtt_client_->unsubscribe(MQTT_CONFIG::SUBSCRIBE_TOPICS::COMMAND_VELOCITY);
    mqtt_client_->unsubscribe(MQTT_CONFIG::SUBSCRIBE_TOPICS::COMMAND_CAMERA);
    mqtt_client_->unsubscribe(MQTT_CONFIG::SUBSCRIBE_TOPICS::COMMAND_THRESHOLD);
    
    mqtt_client_->disconnect();
    
    std::cout << "[Subscriber] Subscritor MQTT parado" << std::endl;
}

void MQTTSubscriber::subscriber_loop() {
    const int LOOP_INTERVAL_MS = 500; // Verificar estado a cada 500ms
    
    while (running_) {
        // A thread dedicada mantém o subscritor vivo
        // e verifica periodicamente o estado da conexão
        if (!mqtt_client_->is_connected()) {
            std::cerr << "[Subscriber] Conexão perdida na thread do subscritor" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(LOOP_INTERVAL_MS));
    }
}

void MQTTSubscriber::on_navigation_command(const std::string& topic, const std::string& payload) {
    (void)topic;

    std::lock_guard<std::mutex> lock(shm_mutex_);

    if (payload == "forward") {
        shm_->j_sp_velocidade = std::min(shm_->j_sp_velocidade + 20, 100);
    } else if (payload == "backward") {
        shm_->j_sp_velocidade = std::max(shm_->j_sp_velocidade - 20, -100);
    } else if (payload == "stop") {
        shm_->j_sp_velocidade = 0;
    }
}

void MQTTSubscriber::on_mode_command(const std::string& topic, const std::string& payload) {
    (void)topic;

    std::cout << "[Subscriber] Comando de modo: " << payload << std::endl;
    
    // Proteger acesso à memória compartilhada com mutex
    std::lock_guard<std::mutex> lock(shm_mutex_);
    
    // Payload: "1" para automático, "0" para manual
    if (payload == "1" || payload == "true") {
        shm_->c_automatico = true;
        shm_->c_man = false;
        std::cout << "[Subscriber] Modo AUTOMÁTICO ativado" << std::endl;
    } else if (payload == "0" || payload == "false") {
        shm_->c_man = true;
        shm_->c_automatico = false;
        std::cout << "[Subscriber] Modo MANUAL ativado" << std::endl;
    }
}

void MQTTSubscriber::on_velocity_command(const std::string& topic, const std::string& payload) {
    (void)topic;

    std::cout << "[Subscriber] Comando de velocidade: " << payload << std::endl;
    
    try {
        int velocity = std::stoi(payload);
        
        // Limitar velocidade a valores razoáveis
        if (velocity >= -100 && velocity <= 100) {
            // Proteger acesso à memória compartilhada com mutex
            std::lock_guard<std::mutex> lock(shm_mutex_);
            shm_->j_sp_velocidade = velocity;
            std::cout << "[Subscriber] Velocidade setada para: " << velocity << std::endl;
        } else {
            std::cerr << "[Subscriber] Velocidade fora dos limites: " << velocity << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Subscriber] Erro ao processar velocidade: " << e.what() << std::endl;
    }
}

void MQTTSubscriber::on_camera_command(const std::string& topic, const std::string& payload) {
    (void)topic;

    std::cout << "[Subscriber] Comando de câmera: " << payload << std::endl;

    std::lock_guard<std::mutex> lock(shm_mutex_);

    if (payload == "1" || payload == "true") {
        shm_->o_liga_camera = true;
        std::cout << "[Subscriber] Câmera LIGADA" << std::endl;
    } else if (payload == "0" || payload == "false") {
        shm_->o_liga_camera = false;
        std::cout << "[Subscriber] Câmera DESLIGADA" << std::endl;
    }
}

void MQTTSubscriber::on_threshold_command(const std::string& topic, const std::string& payload) {
    (void)topic;

    try {
        float limiar = std::stof(payload);
        if (limiar > 0.0f && limiar <= 200.0f) {
            std::lock_guard<std::mutex> lock(shm_mutex_);
            shm_->variacao_severa = limiar;
            std::cout << "[Subscriber] Limiar de variação severa atualizado para: " << limiar << std::endl;
        } else {
            std::cerr << "[Subscriber] Limiar fora dos limites (0, 200]: " << limiar << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Subscriber] Erro ao processar limiar: " << e.what() << std::endl;
    }
}