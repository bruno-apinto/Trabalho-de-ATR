#ifndef MQTT_CLIENT_HPP
#define MQTT_CLIENT_HPP

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mqtt/async_client.h>
#include <map>

/**
 * @class MQTTClient
 * @brief Classe para gerenciar conexões MQTT como Publisher e Subscriber
 */
class MQTTClient {
public:
    using MessageCallback = std::function<void(const std::string&, const std::string&)>;
    
    /**
     * @brief Classe interna para gerenciar callbacks de ação
     */
    class ActionListener : public virtual mqtt::iaction_listener {
    public:
        ActionListener() {}

        void on_success(const mqtt::token& tok) override;
        void on_failure(const mqtt::token& tok) override;
    };
    
    /**
     * @brief Classe interna para gerenciar callbacks de mensagem
     */
    class Callback : public virtual mqtt::callback {
    public:
        Callback(MQTTClient* parent) : parent_(parent) {}
        
        void connection_lost(const std::string& cause) override;
        void message_arrived(mqtt::const_message_ptr msg) override;
        void delivery_complete(mqtt::delivery_token_ptr token) override;
        
    private:
        MQTTClient* parent_;
    };
    
    /**
     * @brief Construtor
     * @param broker_address Endereço do broker MQTT (ex: tcp://localhost:1883)
     * @param client_id ID único do cliente MQTT
     */
    MQTTClient(const std::string& broker_address, const std::string& client_id);
    
    /**
     * @brief Destrutor
     */
    ~MQTTClient();
    
    /**
     * @brief Conectar ao broker MQTT
     * @param username Nome de usuário (opcional)
     * @param password Senha (opcional)
     * @return true se conectado com sucesso
     */
    bool connect(const std::string& username = "", const std::string& password = "");
    
    /**
     * @brief Tentar reconexão automática
     * @return true se reconectado com sucesso
     */
    bool reconnect();
    
    /**
     * @brief Obter o número de tentativas de reconexão
     */
    int get_reconnect_count() const { return reconnect_count_; }
    
    /**
     * @brief Força uma tentativa de reconexão (chamado quando conexão é perdida)
     */
    void try_reconnect();
    
    /**
     * @brief Desconectar do broker MQTT
     * @return true se desconectado com sucesso
     */
    bool disconnect();
    
    /**
     * @brief Verificar se está conectado
     * @return true se conectado
     */
    bool is_connected() const;
    
    /**
     * @brief Publicar mensagem em um tópico
     * @param topic Tópico MQTT
     * @param payload Payload da mensagem
     * @param qos Nível de qualidade de serviço (0, 1, ou 2)
     * @param retained Se a mensagem deve ser retida
     * @return true se publicado com sucesso
     */
    bool publish(const std::string& topic, const std::string& payload, 
                 int qos = 1, bool retained = false);
    
    /**
     * @brief Publicar mensagem numérica
     * @param topic Tópico MQTT
     * @param value Valor numérico
     * @param qos Nível de qualidade de serviço
     * @return true se publicado com sucesso
     */
    bool publish_int(const std::string& topic, int value, int qos = 1);
    
    /**
     * @brief Publicar mensagem booleana
     * @param topic Tópico MQTT
     * @param value Valor booleano
     * @param qos Nível de qualidade de serviço
     * @return true se publicado com sucesso
     */
    bool publish_bool(const std::string& topic, bool value, int qos = 1);
    
    /**
     * @brief Assinar um tópico
     * @param topic Tópico MQTT
     * @param callback Função de callback para mensagens recebidas
     * @param qos Nível de qualidade de serviço
     * @return true se assinado com sucesso
     */
    bool subscribe(const std::string& topic, MessageCallback callback, int qos = 1);
    
    /**
     * @brief Remover assinatura de um tópico
     * @param topic Tópico MQTT
     * @return true se removido com sucesso
     */
    bool unsubscribe(const std::string& topic);
    
    /**
     * @brief Definir callback de conexão
     * @param callback Função chamada quando conecta/desconecta
     */
    void set_connection_callback(std::function<void(bool)> callback);
    
private:
    std::string broker_address_;
    std::string client_id_;
    std::unique_ptr<mqtt::async_client> client_;
    std::map<std::string, MessageCallback> callbacks_;
    std::function<void(bool)> connection_callback_;
    bool connected_;
    int reconnect_count_;
    std::shared_ptr<Callback> callback_ptr_;
    std::shared_ptr<ActionListener> action_listener_ptr_;
    
    friend class ActionListener;
    friend class Callback;
};

#endif
