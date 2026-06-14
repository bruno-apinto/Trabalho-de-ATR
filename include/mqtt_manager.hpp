#ifndef MQTT_MANAGER_HPP
#define MQTT_MANAGER_HPP

#include "mqtt_publisher.hpp"
#include "mqtt_subscriber.hpp"
#include "shared_memory.hpp"
#include <memory>
#include <mutex>

/**
 * @class MQTTManager
 * @brief Gerenciador centralizado de comunicação MQTT
 */
class MQTTManager {
public:
    /**
     * @brief Construtor
     * @param shm Ponteiro para memória compartilhada
     * @param shm_mutex Mutex para proteger acesso à memória compartilhada
     */
    explicit MQTTManager(MemoriaCompartilhada* shm, std::mutex& shm_mutex);
    
    /**
     * @brief Destrutor
     */
    ~MQTTManager();
    
    /**
     * @brief Inicializar gerenciador MQTT
     * @return true se inicializado com sucesso
     */
    bool initialize();
    
    /**
     * @brief Finalizar gerenciador MQTT
     */
    void shutdown();
    
    /**
     * @brief Verificar se está operacional
     * @return true se publisher e subscriber estão rodando
     */
    bool is_operational() const;
    
    /**
     * @brief Obter publicador
     */
    MQTTPublisher* get_publisher() { return publisher_.get(); }
    
    /**
     * @brief Obter subscritor
     */
    MQTTSubscriber* get_subscriber() { return subscriber_.get(); }
    
private:
    MemoriaCompartilhada* shm_;
    std::mutex& shm_mutex_;
    std::unique_ptr<MQTTPublisher> publisher_;
    std::unique_ptr<MQTTSubscriber> subscriber_;
};

#endif
