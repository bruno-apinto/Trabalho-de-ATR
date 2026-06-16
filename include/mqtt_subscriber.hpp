#ifndef MQTT_SUBSCRIBER_HPP
#define MQTT_SUBSCRIBER_HPP

#include "mqtt_client.hpp"
#include "mqtt_config.hpp"
#include "shared_memory.hpp"
#include <thread>
#include <atomic>
#include <memory>
#include <mutex>

struct MemoriaCompartilhada;

/**
 * @class MQTTSubscriber
 * @brief Gerencia a subscrição de comandos para o robô via MQTT
 */
class MQTTSubscriber {
public:
    /**
     * @brief Construtor
     * @param shm Ponteiro para memória compartilhada do robô
     * @param shm_mutex Mutex para proteger acesso à memória compartilhada
     */
    explicit MQTTSubscriber(MemoriaCompartilhada* shm, std::mutex& shm_mutex);
    
    /**
     * @brief Destrutor
     */
    ~MQTTSubscriber();
    
    /**
     * @brief Iniciar subscritor
     * @return true se iniciado com sucesso
     */
    bool start();
    
    /**
     * @brief Parar subscritor
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
    std::thread subscriber_thread_;
    std::atomic<bool> running_;
    
    /**
     * @brief Loop do subscritor (executado em thread dedicada)
     */
    void subscriber_loop();
    
    /**
     * @brief Callback para comandos de navegação
     */
    void on_navigation_command(const std::string& topic, const std::string& payload);
    
    /**
     * @brief Callback para comandos de modo (automático/manual)
     */
    void on_mode_command(const std::string& topic, const std::string& payload);
    
    /**
     * @brief Callback para setpoint de velocidade
     */
    void on_velocity_command(const std::string& topic, const std::string& payload);
    
    /**
     * @brief Callback para comando de câmera
     */
    void on_camera_command(const std::string& topic, const std::string& payload);
};

#endif
