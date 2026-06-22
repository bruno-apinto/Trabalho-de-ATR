#ifndef MQTT_PUBLISHER_HPP
#define MQTT_PUBLISHER_HPP

#include "mqtt_client.hpp"
#include "mqtt_config.hpp"
#include "shared_memory.hpp"
#include <thread>
#include <atomic>
#include <memory>
#include <mutex>

struct MemoriaCompartilhada;

/**
 * @class MQTTPublisher
 * @brief Gerencia a publicação de dados do robô via MQTT
 */
class MQTTPublisher {
public:
    /**
     * @brief Construtor
     * @param shm Ponteiro para memória compartilhada do robô
     * @param shm_mutex Mutex para proteger acesso à memória compartilhada
     */
    explicit MQTTPublisher(MemoriaCompartilhada* shm, std::mutex& shm_mutex);
    
    /**
     * @brief Destrutor
     */
    ~MQTTPublisher();
    
    /**
     * @brief Iniciar publicador
     * @return true se iniciado com sucesso
     */
    bool start();
    
    /**
     * @brief Parar publicador
     */
    void stop();
    
    /**
     * @brief Verificar se está rodando
     * @return true se ativo
     */
    bool is_running() const { return running_; }
    
    /**
     * @brief Obter cliente MQTT
     * @return Ponteiro para MQTTClient
     */
    MQTTClient* get_client() { return mqtt_client_.get(); }
    
private:
    MemoriaCompartilhada* shm_;
    std::mutex& shm_mutex_;
    std::unique_ptr<MQTTClient> mqtt_client_;
    std::thread publish_thread_;
    std::atomic<bool> running_;
    
    /**
     * @brief Loop de publicação (executado em thread separada)
     */
    void publish_loop();
};

#endif 
