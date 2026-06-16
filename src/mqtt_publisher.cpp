#include "../include/mqtt_publisher.hpp"
#include <iostream>
#include <chrono>

MQTTPublisher::MQTTPublisher(MemoriaCompartilhada* shm, std::mutex& shm_mutex)
    : shm_(shm), shm_mutex_(shm_mutex), running_(false),
      last_lidar_value_(-1), last_encoder_state_(false),
      last_inspection_state_(false), last_auto_state_(false),
      last_velocity_(0) {
    
    mqtt_client_ = std::make_unique<MQTTClient>(
        MQTT_CONFIG::BROKER_ADDRESS,
        MQTT_CONFIG::CLIENT_ID_PUBLISHER
    );
}

MQTTPublisher::~MQTTPublisher() {
    stop();
}

bool MQTTPublisher::start() {
    if (running_) {
        std::cout << "[Publisher] Já está rodando" << std::endl;
        return true;
    }
    
    // Conectar ao broker
    if (!mqtt_client_->connect()) {
        std::cerr << "[Publisher] Falha ao conectar ao broker" << std::endl;
        return false;
    }
    
    running_ = true;
    publish_thread_ = std::thread(&MQTTPublisher::publish_loop, this);
    
    std::cout << "[Publisher] Publicador MQTT iniciado" << std::endl;
    return true;
}

void MQTTPublisher::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (publish_thread_.joinable()) {
        publish_thread_.join();
    }
    
    mqtt_client_->disconnect();
    std::cout << "[Publisher] Publicador MQTT parado" << std::endl;
}

void MQTTPublisher::publish_loop() {
    const int PUBLISH_INTERVAL_MS = 100; // Publicar a cada 100ms
    
    while (running_) {
        try {
            // Proteger acesso à memória compartilhada com mutex
            {
                std::lock_guard<std::mutex> lock(shm_mutex_);
                
                // Publicar dados do LIDAR (sensor de distância)
                if (shm_->i_lidar != last_lidar_value_) {
                    mqtt_client_->publish_int(
                        MQTT_CONFIG::PUBLISH_TOPICS::SENSOR_DATA,
                        shm_->i_lidar,
                        MQTT_CONFIG::QOS_SENSOR_DATA
                    );
                    last_lidar_value_ = shm_->i_lidar;
                }
                
                // Publicar estado do encoder
                if (shm_->i_encoder != last_encoder_state_) {
                    mqtt_client_->publish_bool(
                        MQTT_CONFIG::PUBLISH_TOPICS::ENCODER_DATA,
                        shm_->i_encoder,
                        MQTT_CONFIG::QOS_SENSOR_DATA
                    );
                    last_encoder_state_ = shm_->i_encoder;
                }
                
                // Publicar status de inspeção
                if (shm_->e_inspecao != last_inspection_state_) {
                    mqtt_client_->publish_bool(
                        MQTT_CONFIG::PUBLISH_TOPICS::INSPECTION_STATUS,
                        shm_->e_inspecao,
                        MQTT_CONFIG::QOS_STATUS
                    );
                    last_inspection_state_ = shm_->e_inspecao;
                }
                
                // Publicar modo de operação (automático/manual)
                if (shm_->e_automatico != last_auto_state_) {
                    mqtt_client_->publish_bool(
                        MQTT_CONFIG::PUBLISH_TOPICS::NAVIGATION_STATUS,
                        shm_->e_automatico,
                        MQTT_CONFIG::QOS_STATUS
                    );
                    last_auto_state_ = shm_->e_automatico;
                }
                
                // Publicar velocidade no tópico correto VELOCITY_DATA
                if (shm_->j_sp_velocidade != last_velocity_) {
                    mqtt_client_->publish_int(
                        MQTT_CONFIG::PUBLISH_TOPICS::VELOCITY_DATA,
                        shm_->j_sp_velocidade,
                        MQTT_CONFIG::QOS_SENSOR_DATA
                    );
                    last_velocity_ = shm_->j_sp_velocidade;
                }
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[Publisher] Erro ao publicar: " << e.what() << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(PUBLISH_INTERVAL_MS));
    }
}