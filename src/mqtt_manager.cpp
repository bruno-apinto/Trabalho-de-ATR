#include "../include/mqtt_manager.hpp"
#include <iostream>

MQTTManager::MQTTManager(MemoriaCompartilhada* shm, std::mutex& shm_mutex)
    : shm_(shm), shm_mutex_(shm_mutex) {
}

MQTTManager::~MQTTManager() {
    shutdown();
}

bool MQTTManager::initialize() {
    std::cout << "[MQTTManager] Inicializando gerenciador MQTT..." << std::endl;
    
    // Criar e iniciar publisher com mutex para proteção da memória compartilhada
    publisher_ = std::make_unique<MQTTPublisher>(shm_, shm_mutex_);
    if (!publisher_->start()) {
        std::cerr << "[MQTTManager] Falha ao iniciar publisher" << std::endl;
        return false;
    }
    
    // Criar e iniciar subscriber com mutex para proteção da memória compartilhada
    subscriber_ = std::make_unique<MQTTSubscriber>(shm_, shm_mutex_);
    if (!subscriber_->start()) {
        std::cerr << "[MQTTManager] Falha ao iniciar subscriber" << std::endl;
        publisher_->stop();
        return false;
    }
    
    std::cout << "[MQTTManager] Gerenciador MQTT inicializado com sucesso" << std::endl;
    return true;
}

void MQTTManager::shutdown() {
    std::cout << "[MQTTManager] Encerrando gerenciador MQTT..." << std::endl;
    
    if (subscriber_) {
        subscriber_->stop();
        subscriber_.reset();
    }
    
    if (publisher_) {
        publisher_->stop();
        publisher_.reset();
    }
    
    std::cout << "[MQTTManager] Gerenciador MQTT encerrado" << std::endl;
}

bool MQTTManager::is_operational() const {
    return publisher_ && publisher_->is_running() &&
           subscriber_ && subscriber_->is_running();
}