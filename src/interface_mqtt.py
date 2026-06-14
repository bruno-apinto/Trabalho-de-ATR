"""
Interface Pygame com Integração MQTT
Exibe visualização do robô e permite controle remoto via MQTT
"""

import pygame
import sys
import threading
import time
from pathlib import Path
from robot_mqtt_client import RobotMQTTClient

# CONFIGURAÇÃO PYGAME

pygame.init()

FPS = 30
fpsClock = pygame.time.Clock()

largura_tela = 1000
altura_tela = 600

DISPLAYSURF = pygame.display.set_mode((largura_tela, altura_tela))
pygame.display.set_caption('Robô de Inspeção - Etapa 2')

WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
GREEN = (0, 255, 0)
RED = (255, 0, 0)
YELLOW = (255, 255, 0)
BLUE = (0, 0, 255)

BASE_DIR = Path(__file__).resolve().parent.parent

# Carregar imagens
backImg = pygame.image.load(str(BASE_DIR / 'src' / 'tunel.jpeg'))
altura_tunel = 400
backImg = pygame.transform.scale(backImg, (largura_tela, altura_tunel))

tunel_x = 0
tunel_y = (altura_tela - altura_tunel) // 2

carImg = pygame.image.load(str(BASE_DIR / 'src' / 'carrinho.png'))
largura_carro = carImg.get_width()
altura_carro = carImg.get_height()

carx = 0
cary = altura_tela // 2 - altura_carro // 2 + 65

# CLASSE DE INTERFACE COM MQTT

class InterfaceRobo:
    def __init__(self):
        """Inicializar interface e conectar ao MQTT"""
        self.robot = RobotMQTTClient(broker_address="localhost")
        
        # Estado atual
        self.velocidade = 5
        self.andando = False
        self.lidar_distance = 0
        self.encoder_state = False
        self.camera_on = False
        self.modo_automatico = False
        self.inspecao_ativa = False
        self.tunel_x = 0
        
        # Conectar ao MQTT
        print("Conectando ao robô via MQTT...")
        if self.robot.connect():
            print("✓ Conectado ao robô!")
            # Registrar callback para receber dados
            self.robot.register_callback(self.on_robot_update)
        else:
            print("✗ Erro ao conectar ao robô")
    
    def on_robot_update(self, topic, payload):
        """Callback quando dados chegam do robô"""
        try:
            if "lidar" in topic:
                self.lidar_distance = int(payload) if payload.isdigit() else 0
            elif "encoder" in topic:
                self.encoder_state = payload in ["1", "true", "True"]
            elif "navigation" in topic:
                self.modo_automatico = payload in ["1", "true", "True"]
            elif "inspection" in topic:
                self.inspecao_ativa = payload in ["1", "true", "True"]
        except:
            pass
    
    def send_navigation_command(self, cmd):
        """Enviar comando de navegação"""
        self.robot.send_navigation_command(cmd)
        print(f"Comando enviado: {cmd}")
    
    def send_velocity(self, vel):
        """Enviar setpoint de velocidade (-100 a 100)"""
        self.robot.send_velocity_command(int(vel))
    
    def send_mode(self, auto):
        """Enviar comando de modo (automático/manual)"""
        self.robot.send_mode_command(auto)
        self.modo_automatico = auto
    
    def send_camera(self, on):
        """Enviar comando de câmera"""
        self.robot.send_camera_command(on)
        self.camera_on = on
    
    def update_simulation(self, keys):
        """Atualizar simulação visual"""
        # A tecla ESPAÇO agora é tratada exclusivamente no evento KEYDOWN
        
        if self.andando:
            self.tunel_x -= self.velocidade
        
        if keys[pygame.K_LEFT]:
            self.tunel_x += self.velocidade
        
        if keys[pygame.K_RIGHT]:
            self.tunel_x -= self.velocidade
        
        # Repetição infinita do túnel
        if self.tunel_x <= -largura_tela:
            self.tunel_x = 0
        if self.tunel_x >= largura_tela:
            self.tunel_x = 0
    
    def draw(self):
        """Desenhar interface"""
        DISPLAYSURF.fill(WHITE)
        
        # Desenhar túnel
        DISPLAYSURF.blit(backImg, (self.tunel_x, tunel_y))
        DISPLAYSURF.blit(backImg, (self.tunel_x + largura_tela, tunel_y))
        DISPLAYSURF.blit(backImg, (self.tunel_x - largura_tela, tunel_y))
        
        # Desenhar carrinho
        DISPLAYSURF.blit(carImg, (carx, cary))
        
        # Desenhar status
        self.draw_status()
        
        pygame.display.update()
    
    def draw_status(self):
        """Desenhar painel de status"""
        font = pygame.font.Font(None, 24)
        y_offset = 20
        
        # Linha 1: Modo
        modo_text = "AUTOMÁTICO" if self.modo_automatico else "MANUAL"
        color = GREEN if self.modo_automatico else RED
        text = font.render(f"Modo: {modo_text}", True, color)
        DISPLAYSURF.blit(text, (largura_tela - 300, y_offset))
        
        # Linha 2: Inspeção
        y_offset += 30
        inspecao_text = "INSPEÇÃO ATIVA" if self.inspecao_ativa else "Sem inspeção"
        color = YELLOW if self.inspecao_ativa else BLACK
        text = font.render(inspecao_text, True, color)
        DISPLAYSURF.blit(text, (largura_tela - 300, y_offset))
        
        # Linha 3: Câmera
        y_offset += 30
        camera_text = "CÂMERA ON" if self.camera_on else "Câmera OFF"
        color = BLUE if self.camera_on else BLACK
        text = font.render(camera_text, True, color)
        DISPLAYSURF.blit(text, (largura_tela - 300, y_offset))
        
        # Linha 4: LIDAR
        y_offset += 30
        text = font.render(f"LIDAR: {self.lidar_distance}mm", True, BLACK)
        DISPLAYSURF.blit(text, (largura_tela - 300, y_offset))
        
        # Linha 5: Encoder
        y_offset += 30
        encoder_text = "Encoder: ON" if self.encoder_state else "Encoder: OFF"
        text = font.render(encoder_text, True, BLACK)
        DISPLAYSURF.blit(text, (largura_tela - 300, y_offset))
        
        # Instruções
        y_offset = altura_tela - 100
        small_font = pygame.font.Font(None, 18)
        instructions = [
            "CONTROLES:",
            "ESPAÇO: Ligar/Desligar",
            "SETAS: Controlar posição (simulação)",
            "A: Modo Automático | M: Modo Manual",
            "C: Câmera ON/OFF | Q: Sair"
        ]
        for i, instr in enumerate(instructions):
            text = small_font.render(instr, True, BLACK)
            DISPLAYSURF.blit(text, (10, y_offset + i * 18))
    
    def disconnect(self):
        """Desconectar do MQTT"""
        self.robot.disconnect()
        print("Desconectado")


# LOOP PRINCIPAL

def main():
    """Loop principal da interface"""
    interface = InterfaceRobo()
    
    try:
        running = True
        while running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                
                if event.type == pygame.KEYDOWN:
                    # Controles
                    if event.key == pygame.K_SPACE:
                        interface.andando = not interface.andando
                    
                    if event.key == pygame.K_a:  # Modo Automático
                        interface.send_mode(True)
                    
                    if event.key == pygame.K_m:  # Modo Manual
                        interface.send_mode(False)
                    
                    if event.key == pygame.K_c:  # Câmera
                        interface.send_camera(not interface.camera_on)
                    
                    if event.key == pygame.K_f:  # Forward
                        interface.send_navigation_command("forward")
                    
                    if event.key == pygame.K_b:  # Backward
                        interface.send_navigation_command("backward")
                    
                    if event.key == pygame.K_s:  # Stop
                        interface.send_navigation_command("stop")
                    
                    if event.key == pygame.K_q:  # Quit
                        running = False
                    
                    # Velocidade: teclas 1-9 ajustam setpoint
                    if pygame.K_1 <= event.key <= pygame.K_9:
                        i = event.key - pygame.K_1 + 1
                        vel = (i * 100) // 10  # 10, 20, ..., 90
                        interface.send_velocity(vel)
            
            keys = pygame.key.get_pressed()
            
            # Atualizar simulação
            interface.update_simulation(keys)
            
            # Desenhar
            interface.draw()
            
            fpsClock.tick(FPS)
    
    except KeyboardInterrupt:
        print("\nInterrompido pelo usuário")
    
    finally:
        interface.disconnect()
        pygame.quit()
        sys.exit()


if __name__ == "__main__":
    main()
