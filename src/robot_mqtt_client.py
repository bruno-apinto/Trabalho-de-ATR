#!/usr/bin/env python3
"""
Cliente MQTT para Operação Remota
Exemplo de como integrar MQTT com a interface remota do robô
"""

import paho.mqtt.client as mqtt
import time

class RobotMQTTClient:
    def __init__(self, broker_address="localhost", broker_port=1883):
        self.broker_address = broker_address
        self.broker_port = broker_port
        self.client = mqtt.Client(client_id="remote_operation_client")
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self.client.on_disconnect = self._on_disconnect
        
        # Armazenar dados recebidos do robô
        self.robot_data = {
            "lidar": 0,
            "encoder": False,
            "inspection_active": False,
            "automatic_mode": False,
            "velocity": 0,
            "tunnel_profile": [],   # lista de (x, y, confianca)
            "confidence": 0.0,
        }
        
        # Callbacks para atualizações de dados
        self.data_callbacks = []
        
    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print("[MQTT] Conectado ao broker MQTT")
            client.subscribe("robot/sensors/lidar", qos=1)
            client.subscribe("robot/sensors/encoder", qos=1)
            client.subscribe("robot/status/inspection", qos=1)
            client.subscribe("robot/status/navigation", qos=1)
            client.subscribe("robot/data/velocity", qos=1)
            client.subscribe("robot/data/tunnel_profile", qos=1)  # perfil do teto (x,y,conf)
            client.subscribe("robot/data/confidence", qos=1)       # nível de confiança
        else:
            print(f"[MQTT] Falha na conexão: {rc}")
    
    def _on_disconnect(self, client, userdata, rc):
        if rc != 0:
            print(f"[MQTT] Desconexão não esperada: {rc}")
        else:
            print("[MQTT] Desconectado com sucesso")
    
    def _on_message(self, client, userdata, msg):
        """Callback ao receber mensagens do robô"""
        topic = msg.topic
        payload = msg.payload.decode('utf-8')
        
        print(f"[MQTT] Mensagem recebida em {topic}: {payload}")
        
        # Atualizar dados do robô baseado no tópico
        if topic == "robot/sensors/lidar":
            try:
                self.robot_data["lidar"] = int(payload)
            except ValueError:
                pass
        elif topic == "robot/sensors/encoder":
            self.robot_data["encoder"] = payload == "1"
        elif topic == "robot/status/inspection":
            self.robot_data["inspection_active"] = payload == "1"
        elif topic == "robot/status/navigation":
            self.robot_data["automatic_mode"] = payload == "1"
        elif topic == "robot/data/velocity":
            try:
                self.robot_data["velocity"] = int(payload)
            except ValueError:
                pass
        elif topic == "robot/data/tunnel_profile":
            # Formato: "x,y,confianca"
            try:
                partes = payload.split(",")
                if len(partes) == 3:
                    ponto = (float(partes[0]), float(partes[1]), float(partes[2]))
                    self.robot_data["tunnel_profile"].append(ponto)
                    # Mantém apenas os últimos 200 pontos para não crescer indefinidamente
                    if len(self.robot_data["tunnel_profile"]) > 200:
                        self.robot_data["tunnel_profile"].pop(0)
            except ValueError:
                pass
        elif topic == "robot/data/confidence":
            try:
                self.robot_data["confidence"] = float(payload)
            except ValueError:
                pass
        
        # Chamar callbacks registrados
        for callback in self.data_callbacks:
            callback(topic, payload)
    
    def connect(self):
        """Conectar ao broker MQTT"""
        try:
            self.client.connect(self.broker_address, self.broker_port, keepalive=60)
            self.client.loop_start()  # Iniciar loop em thread separada
            print(f"[MQTT] Conectando a {self.broker_address}:{self.broker_port}")
        except Exception as e:
            print(f"[MQTT] Erro na conexão: {e}")
            return False
        return True
    
    def disconnect(self):
        """Desconectar do broker MQTT"""
        self.client.loop_stop()
        self.client.disconnect()
    
    def send_navigation_command(self, command):
        """
        Enviar comando de navegação
        Comandos: "forward", "backward", "stop"
        """
        self.client.publish("robot/commands/navigation", command, qos=1)
        print(f"[MQTT] Comando de navegação enviado: {command}")
    
    def send_mode_command(self, automatic=True):
        """Enviar comando de modo (automático/manual)"""
        value = "1" if automatic else "0"
        self.client.publish("robot/commands/mode", value, qos=1)
        print(f"[MQTT] Modo alterado para: {'AUTOMÁTICO' if automatic else 'MANUAL'}")
    
    def send_velocity_command(self, velocity):
        """Enviar comando de velocidade (-100 a 100)"""
        if -100 <= velocity <= 100:
            self.client.publish("robot/commands/velocity", str(velocity), qos=1)
            print(f"[MQTT] Velocidade enviada: {velocity}")
        else:
            print(f"[MQTT] Velocidade inválida: {velocity}")
    
    def send_camera_command(self, enable=True):
        """Enviar comando para ligar/desligar câmera"""
        value = "1" if enable else "0"
        self.client.publish("robot/commands/camera", value, qos=1)
        print(f"[MQTT] Câmera: {'LIGADA' if enable else 'DESLIGADA'}")

    def send_threshold_command(self, threshold: float):
        """Enviar novo limiar de variação severa para o robô"""
        if 0.0 < threshold <= 200.0:
            self.client.publish("robot/commands/threshold", f"{threshold:.1f}", qos=1)
            print(f"[MQTT] Limiar de variação atualizado para: {threshold:.1f}")
    
    def register_callback(self, callback):
        """Registrar callback para receber atualizações de dados"""
        self.data_callbacks.append(callback)
    
    def get_robot_data(self):
        """Obter dados atuais do robô"""
        return self.robot_data.copy()


