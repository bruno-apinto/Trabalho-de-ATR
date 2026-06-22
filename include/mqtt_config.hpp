#ifndef MQTT_CONFIG_HPP
#define MQTT_CONFIG_HPP

#include <string>

namespace MQTT_CONFIG {
    // Configuração do broker MQTT
    constexpr const char* BROKER_ADDRESS = "tcp://localhost:1883";
    
    // IDs dos clientes MQTT
    constexpr const char* CLIENT_ID_PUBLISHER = "robot_publisher";
    constexpr const char* CLIENT_ID_SUBSCRIBER = "robot_subscriber";
    
    // Tópicos para publicação (Robot -> Remote Operation)
    namespace PUBLISH_TOPICS {
        constexpr const char* SENSOR_DATA = "robot/sensors/lidar";
        constexpr const char* ENCODER_DATA = "robot/sensors/encoder";
        constexpr const char* INSPECTION_STATUS = "robot/status/inspection";
        constexpr const char* NAVIGATION_STATUS = "robot/status/navigation";
        constexpr const char* TUNNEL_PROFILE = "robot/data/tunnel_profile";
        constexpr const char* VELOCITY_DATA = "robot/data/velocity";
        constexpr const char* CONFIDENCE_DATA = "robot/data/confidence";
    }
    
    // Tópicos para subscrição (Remote Operation -> Robot)
    namespace SUBSCRIBE_TOPICS {
        constexpr const char* COMMAND_NAVIGATION = "robot/commands/navigation";
        constexpr const char* COMMAND_MODE = "robot/commands/mode";
        constexpr const char* COMMAND_VELOCITY = "robot/commands/velocity";
        constexpr const char* COMMAND_CAMERA = "robot/commands/camera";
        constexpr const char* COMMAND_THRESHOLD = "robot/commands/threshold"; // limiar de variação severa
    }
    
    // Configurações de qualidade de serviço
    constexpr int QOS_SENSOR_DATA = 1;      // At least once - dados de sensores
    constexpr int QOS_COMMANDS = 1;         // At least once - comandos críticos
    constexpr int QOS_STATUS = 0;           // At most once - status não crítico
    
    // Timeout de conexão
    constexpr int CONNECT_TIMEOUT_MS = 10000;
    constexpr int SUBSCRIBE_TIMEOUT_MS = 5000;
    
    // Keep alive
    constexpr int KEEP_ALIVE_INTERVAL = 60;
}

#endif
