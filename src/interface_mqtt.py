import pygame
import sys
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

# CARREGAMENTO DAS IMAGENS

# Fundo do túnel
backImg = pygame.image.load(str(BASE_DIR / 'src' / 'tunel.jpeg'))

altura_tunel = 400

backImg = pygame.transform.scale(
    backImg,
    (largura_tela, altura_tunel)
)

tunel_y = (altura_tela - altura_tunel) // 2

# Carrinho
carImg = pygame.image.load(
    str(BASE_DIR / 'src' / 'carrinho.png')
)

largura_carro = carImg.get_width()
altura_carro = carImg.get_height()

# Redimensionar carro
carImg = pygame.transform.scale(
    carImg,
    (
        int(largura_carro * 0.8),
        int(altura_carro * 0.8)
    )
)

largura_carro = carImg.get_width()
altura_carro = carImg.get_height()

carx = 0
cary = altura_tela // 2 - altura_carro // 2 + 82

# Falhas do túnel
falha_buraco = pygame.image.load(
    str(BASE_DIR / 'src' / 'falha_buraco.png')
)

falha_buraco = pygame.transform.scale(
    falha_buraco,
    (250, 147)
)

falha_protuberancia = pygame.image.load(
    str(BASE_DIR / 'src' / 'falha_protuberancia.png')
)

falha_protuberancia = pygame.transform.scale(
    falha_protuberancia,
    (250, 147)
)

# Lista de falhas
falhas = [
    {'img': falha_buraco, 'x': 300},
    {'img': falha_protuberancia, 'x': 700},
    {'img': falha_buraco, 'x': 1300},
    {'img': falha_protuberancia, 'x': 1800},
    {'img': falha_buraco, 'x': 2400},
    {'img': falha_protuberancia, 'x': 3000},
]

# CLASSE PRINCIPAL
class InterfaceRobo:

    def __init__(self):

        """Inicializar interface e conectar ao MQTT"""

        self.robot = RobotMQTTClient(
            broker_address="localhost"
        )

        # Estados do robô
        self.velocidade_atual = 0
        self.encoder_state = False
        self.lidar_distance = 0
        self.camera_on = False
        self.modo_automatico = False
        self.inspecao_ativa = False

        # Movimento visual
        self.posicao = 0
        self.tunel_x = 0

        # Controle local visual
        self.andando = False

        # MQTT
        print("Conectando ao robô via MQTT...")

        if self.robot.connect():

            print("✓ Conectado ao robô!")

            self.robot.register_callback(
                self.on_robot_update
            )

        else:
            print("✗ Erro ao conectar ao robô")

    # CALLBACK MQTT
    def on_robot_update(self, topic, payload):

        """Receber atualizações MQTT"""

        try:

            # LIDAR
            if "lidar" in topic:

                self.lidar_distance = (
                    int(payload)
                    if payload.isdigit()
                    else 0
                )

            # Encoder
            elif "encoder" in topic:

                self.encoder_state = (
                    payload in ["1", "true", "True"]
                )

            # Navegação automática
            elif "navigation" in topic:

                self.modo_automatico = (
                    payload in ["1", "true", "True"]
                )

            # Inspeção
            elif "inspection" in topic:

                self.inspecao_ativa = (
                    payload in ["1", "true", "True"]
                )

            # Velocidade
            elif "velocity" in topic:

                if payload.lstrip('-').isdigit():
                    self.velocidade_atual = int(payload)

        except Exception:
            pass

    # ENVIO DE COMANDOS MQTT
    def send_navigation_command(self, cmd):

        self.robot.send_navigation_command(cmd)

        print(f"Comando enviado: {cmd}")

    def send_velocity(self, vel):

        self.robot.send_velocity_command(int(vel))

    def send_mode(self, auto):

        self.robot.send_mode_command(auto)

        self.modo_automatico = auto

    def send_camera(self, on):

        self.robot.send_camera_command(on)

        self.camera_on = on

    # SIMULAÇÃO VISUAL
    def update_simulation(self, delta_time, keys):

        """
        Atualizar movimentação visual
        """

        # Movimento automático
        self.posicao += (
            self.velocidade_atual * 2 * delta_time
        )

        # Movimento manual opcional
        if keys[pygame.K_LEFT]:
            self.posicao -= 200 * delta_time

        if keys[pygame.K_RIGHT]:
            self.posicao += 200 * delta_time

        # Fundo infinito
        self.tunel_x = -int(
            self.posicao % largura_tela
        )

    # DESENHO DAS ANOMALIAS
    def draw_anomalies(self):

        for falha in falhas:

            x_tela = int(
                falha['x'] - self.posicao
            )

            if -300 < x_tela < largura_tela + 300:

                DISPLAYSURF.blit(
                    falha['img'],
                    (x_tela, tunel_y)
                )


    # VISUALIZAÇÃO DO LIDAR
    def draw_lidar(self):

        sensorx = int(carx + 70)
        sensory = cary + 24

        lidar_y = tunel_y + self.lidar_distance

        pygame.draw.line(
            DISPLAYSURF,
            GREEN,
            (sensorx, sensory),
            (sensorx, lidar_y),
            2
        )


    # ALERTAS
    def draw_alerts(self):

        if self.inspecao_ativa:

            font = pygame.font.SysFont(
                None,
                36
            )

            texto = font.render(
                "INSPEÇÃO ATIVA",
                True,
                RED
            )

            DISPLAYSURF.blit(
                texto,
                (10, 10)
            )

    # STATUS
    def draw_status(self):

        font = pygame.font.Font(None, 24)

        y_offset = 20

        # Modo
        modo_text = (
            "AUTOMÁTICO"
            if self.modo_automatico
            else "MANUAL"
        )

        color = (
            GREEN
            if self.modo_automatico
            else RED
        )

        text = font.render(
            f"Modo: {modo_text}",
            True,
            color
        )

        DISPLAYSURF.blit(
            text,
            (largura_tela - 300, y_offset)
        )

        # Inspeção
        y_offset += 30

        inspecao_text = (
            "INSPEÇÃO ATIVA"
            if self.inspecao_ativa
            else "Sem inspeção"
        )

        color = (
            YELLOW
            if self.inspecao_ativa
            else BLACK
        )

        text = font.render(
            inspecao_text,
            True,
            color
        )

        DISPLAYSURF.blit(
            text,
            (largura_tela - 300, y_offset)
        )

        # Câmera
        y_offset += 30

        camera_text = (
            "CÂMERA ON"
            if self.camera_on
            else "Câmera OFF"
        )

        color = (
            BLUE
            if self.camera_on
            else BLACK
        )

        text = font.render(
            camera_text,
            True,
            color
        )

        DISPLAYSURF.blit(
            text,
            (largura_tela - 300, y_offset)
        )

        # LIDAR
        y_offset += 30

        text = font.render(
            f"LIDAR: {self.lidar_distance} mm",
            True,
            BLACK
        )

        DISPLAYSURF.blit(
            text,
            (largura_tela - 300, y_offset)
        )

        # Encoder
        y_offset += 30

        encoder_text = (
            "Encoder: ON"
            if self.encoder_state
            else "Encoder: OFF"
        )

        text = font.render(
            encoder_text,
            True,
            BLACK
        )

        DISPLAYSURF.blit(
            text,
            (largura_tela - 300, y_offset)
        )

        # Velocidade
        y_offset += 30

        text = font.render(
            f"Velocidade: {self.velocidade_atual}",
            True,
            BLACK
        )

        DISPLAYSURF.blit(
            text,
            (largura_tela - 300, y_offset)
        )

        # Instruções
        y_offset = altura_tela - 120

        small_font = pygame.font.Font(None, 18)

        instructions = [
            "CONTROLES:",
            "1-9: Ajustar velocidade",
            "A: Modo Automático",
            "M: Modo Manual",
            "C: Câmera ON/OFF",
            "SETAS: Movimento manual",
            "Q: Sair"
        ]

        for i, instr in enumerate(instructions):

            text = small_font.render(
                instr,
                True,
                BLACK
            )

            DISPLAYSURF.blit(
                text,
                (10, y_offset + i * 18)
            )

    # DESENHO PRINCIPAL
    def draw(self):

        DISPLAYSURF.fill(WHITE)

        # Fundo do túnel
        DISPLAYSURF.blit(
            backImg,
            (self.tunel_x, tunel_y)
        )

        DISPLAYSURF.blit(
            backImg,
            (self.tunel_x + largura_tela, tunel_y)
        )

        DISPLAYSURF.blit(
            backImg,
            (self.tunel_x - largura_tela, tunel_y)
        )

        # Anomalias
        self.draw_anomalies()

        # LIDAR
        self.draw_lidar()

        # Alertas
        self.draw_alerts()

        # Carrinho
        DISPLAYSURF.blit(
            carImg,
            (carx, cary)
        )

        # Status
        self.draw_status()

        pygame.display.update()

    # DESCONECTAR MQTT
    def disconnect(self):

        self.robot.disconnect()

        print("Desconectado")

# LOOP PRINCIPAL
def main():

    interface = InterfaceRobo()

    try:

        running = True

        while running:

            delta_time = (
                fpsClock.tick(FPS) / 1000.0
            )

            delta_time = min(
                delta_time,
                0.1
            )

            # EVENTOS
            for event in pygame.event.get():

                if event.type == pygame.QUIT:
                    running = False

                if event.type == pygame.KEYDOWN:

                    # Modos
                    if event.key == pygame.K_a:

                        interface.send_mode(True)

                    if event.key == pygame.K_m:

                        interface.send_mode(False)

                    # Câmera
                    if event.key == pygame.K_c:

                        interface.send_camera(
                            not interface.camera_on
                        )

                    # Navegação
                    if event.key == pygame.K_f:

                        interface.send_navigation_command(
                            "forward"
                        )

                    if event.key == pygame.K_b:

                        interface.send_navigation_command(
                            "backward"
                        )

                    if event.key == pygame.K_s:

                        interface.send_navigation_command(
                            "stop"
                        )

                    # Velocidade
                    if pygame.K_1 <= event.key <= pygame.K_9:

                        i = (
                            event.key
                            - pygame.K_1
                            + 1
                        )

                        vel = (
                            i * 100
                        ) // 10

                        interface.send_velocity(vel)

                    # Sair
                    if event.key == pygame.K_q:
                        running = False


            # TECLAS PRESSIONADAS
            keys = pygame.key.get_pressed()

            # ATUALIZAR
            interface.update_simulation(
                delta_time,
                keys
            )

            # DESENHAR
            interface.draw()

    except KeyboardInterrupt:

        print("\nInterrompido pelo usuário")

    finally:

        interface.disconnect()

        pygame.quit()

        sys.exit()

# EXECUÇÃO

if __name__ == "__main__":
    main()