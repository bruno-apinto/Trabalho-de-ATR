#include "../include/mqtt_publisher.hpp"
#include <iostream>
#include <chrono>

MQTTPublisher::MQTTPublisher(MemoriaCompartilhada* shm, std::mutex& shm_mutex)
    : shm_(shm), shm_mutex_(shm_mutex), running_(false) {

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
    const int PUBLISH_INTERVAL_MS = 100;

    while (running_) {
        try {
            std::lock_guard<std::mutex> lock(shm_mutex_);

            // Publicar sensores e estados periodicamente (sem detecção de mudança)
            mqtt_client_->publish_int(MQTT_CONFIG::PUBLISH_TOPICS::SENSOR_DATA,
                                      shm_->i_lidar, MQTT_CONFIG::QOS_SENSOR_DATA);
            mqtt_client_->publish_bool(MQTT_CONFIG::PUBLISH_TOPICS::ENCODER_DATA,
                                       shm_->i_encoder, MQTT_CONFIG::QOS_SENSOR_DATA);
            mqtt_client_->publish_bool(MQTT_CONFIG::PUBLISH_TOPICS::INSPECTION_STATUS,
                                       shm_->e_inspecao, MQTT_CONFIG::QOS_STATUS);
            mqtt_client_->publish_bool(MQTT_CONFIG::PUBLISH_TOPICS::NAVIGATION_STATUS,
                                       shm_->e_automatico, MQTT_CONFIG::QOS_STATUS);
            mqtt_client_->publish_int(MQTT_CONFIG::PUBLISH_TOPICS::VELOCITY_DATA,
                                      shm_->j_sp_velocidade, MQTT_CONFIG::QOS_SENSOR_DATA);

            // Publicar perfil do teto quando reconstrução_teto sinalizar novo ponto
            if (shm_->perfil_novo) {
                // Formato: "x,y,confianca"
                std::string perfil_payload =
                    std::to_string(shm_->posicao_x) + "," +
                    std::to_string(shm_->perfil_y) + "," +
                    std::to_string(shm_->perfil_confianca);

                mqtt_client_->publish(MQTT_CONFIG::PUBLISH_TOPICS::TUNNEL_PROFILE,
                                      perfil_payload, MQTT_CONFIG::QOS_SENSOR_DATA);
                mqtt_client_->publish(MQTT_CONFIG::PUBLISH_TOPICS::CONFIDENCE_DATA,
                                      std::to_string(shm_->perfil_confianca),
                                      MQTT_CONFIG::QOS_SENSOR_DATA);
                shm_->perfil_novo = false;
            }

        } catch (const std::exception& e) {
            std::cerr << "[Publisher] Erro ao publicar: " << e.what() << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(PUBLISH_INTERVAL_MS));
    }
}