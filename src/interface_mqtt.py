import pygame
import sys
import random
from pathlib import Path

from robot_mqtt_client import RobotMQTTClient

# CONFIGURAÇÃO PYGAME

pygame.init()

FPS = 30
fpsClock = pygame.time.Clock()

largura_tela = 1000
altura_tela = 760

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

tunel_y = 10  # túnel no topo, deixa espaço abaixo para gráfico e instruções

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
cary = tunel_y + altura_tunel // 2 - altura_carro // 2 + 82

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

# Pool determinístico: mesma seed e mesma lógica que interface.py → sequência idêntica.
# Cada índice do pool define completamente uma anomalia (espaçamento, tipo, curva).
random.seed(42)
_N_POOL = 2000
_pool = [(random.randint(200, 700),
          random.choice(["buraco", "protuberancia"]),
          random.randint(30, 70)) for _ in range(_N_POOL)]
_seq = 0

anomalias = []

def cria_anomalia(posicao_x):
    global _seq
    _, tipo, curva = _pool[_seq % _N_POOL]
    _seq += 1
    imagem = falha_buraco if tipo == "buraco" else falha_protuberancia
    redimensionada = pygame.transform.smoothscale(imagem, (230, 149))
    return {"tipo": tipo, "x": float(posicao_x), "largura": 200,
            "altura": 130, "curva": curva, "img": redimensionada}

def anomalias_iniciais():
    global _seq
    _seq = 0
    anomalias.clear()
    proxima_posicao = largura_tela
    for i in range(5):
        proxima_posicao += _pool[i][0]
        anomalias.append(cria_anomalia(proxima_posicao))

def anomalias_automaticas(posicao):
    if not anomalias:
        return
    maior_posicao = max(a["x"] for a in anomalias)
    for indice, anomalia in enumerate(anomalias):
        if anomalia["x"] + anomalia["largura"] < posicao - 200:
            maior_posicao += _pool[_seq % _N_POOL][0]
            anomalias[indice] = cria_anomalia(maior_posicao)

def sync_anomaly_state(target_pos):
    """Avança o estado das anomalias até target_pos, espelhando a simulação."""
    anomalias_iniciais()
    mudou = True
    while mudou:
        mudou = False
        maior_posicao = max(a["x"] for a in anomalias)
        for indice, anomalia in enumerate(anomalias):
            if anomalia["x"] + anomalia["largura"] < target_pos - 200:
                maior_posicao += _pool[_seq % _N_POOL][0]
                anomalias[indice] = cria_anomalia(maior_posicao)
                mudou = True

anomalias_iniciais()

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
        self._modo_manual_local = False  # usuário escolheu manual explicitamente

        # Movimento visual
        self.posicao = 0
        self.tunel_x = 0

        # Controle local visual
        self.andando = False

        # Perfil do teto recebido via MQTT
        self.tunnel_profile = []  # lista de (x, y, confianca)
        self.confianca_atual = 0.0

        # Limiar de variação severa (editável pela operação remota)
        self.limiar = 10.0

        self.posicao_vel = 0.0  # velocidade atual para convergência suave
        self.posicao_x_real = 0.0  # posição x do robô em metros (via MQTT)
        self._pos_sincronizada = False  # ajuste inicial de lag feito?

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

            # Navegação automática — ignora se usuário escolheu manual explicitamente
            elif "navigation" in topic:

                novo_modo = payload in ["1", "true", "True"]
                if not self._modo_manual_local:
                    self.modo_automatico = novo_modo
                elif novo_modo:
                    # simulação voltou ao auto: respeita se usuário também pressionar 'A'
                    pass

            # Inspeção
            elif "inspection" in topic:

                self.inspecao_ativa = payload in ["1", "true", "True"]
                self.camera_on = self.inspecao_ativa  # câmera sincronizada com inspeção

            # Velocidade
            elif "velocity" in topic:

                if payload.lstrip('-').isdigit():
                    self.velocidade_atual = int(payload)

            # Perfil do teto (x,y,confianca)
            elif "tunnel_profile" in topic:

                try:
                    partes = payload.split(",")
                    if len(partes) == 3:
                        self.posicao_x_real = float(partes[0])
                        self.tunnel_profile.append(
                            (float(partes[0]), float(partes[1]), float(partes[2]))
                        )
                        if len(self.tunnel_profile) > 200:
                            self.tunnel_profile.pop(0)
                except ValueError:
                    pass

            # Nível de confiança
            elif "confidence" in topic:

                try:
                    self.confianca_atual = float(payload)
                except ValueError:
                    pass

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
        self._modo_manual_local = not auto

    def send_camera(self, on):

        self.robot.send_camera_command(on)

        self.camera_on = on

    def send_threshold(self, delta):

        self.limiar = max(1.0, min(200.0, self.limiar + delta))
        self.robot.send_threshold_command(self.limiar)
        print(f"Limiar enviado: {self.limiar:.1f}")

    # SIMULAÇÃO VISUAL
    def update_simulation(self, delta_time, keys):

        """
        Atualizar movimentação visual
        """

        # Mesma fórmula da interface.py: sp × 4 + aceleração × 4
        aceleracao_efetiva = -30 if (self.inspecao_ativa and self.modo_automatico) else 0
        alvo = (float(self.velocidade_atual) + aceleracao_efetiva) * 4.0
        self.posicao_vel += (alvo - self.posicao_vel) * min(delta_time * 15.0, 1.0)
        self.posicao += self.posicao_vel * delta_time

        # Compensação de lag MQTT: na primeira transição de encoder recebida,
        # adianta a posição pelo tempo estimado de atraso da rede (~150ms).
        # Depois disso ambas as interfaces rodam à mesma velocidade → offset zero.
        if not self._pos_sincronizada and self.posicao_x_real > 0:
            LAG_S = 0.15  # lag típico MQTT em segundos
            self.posicao = self.posicao_x_real * 100.0 + self.posicao_vel * LAG_S
            self._pos_sincronizada = True

        # Movimento manual opcional
        if keys[pygame.K_LEFT]:
            self.posicao -= 200 * delta_time

        if keys[pygame.K_RIGHT]:
            self.posicao += 200 * delta_time

        # Regenera anomalias conforme o robô avança (igual à interface.py)
        anomalias_automaticas(self.posicao)

        # Fundo infinito
        self.tunel_x = -int(
            self.posicao % largura_tela
        )

    # DESENHO DAS ANOMALIAS (mesmas posições da interface.py via seed compartilhado)
    def draw_anomalies(self):

        for anomalia in anomalias:

            x_tela = int(anomalia["x"] - self.posicao)

            if -300 < x_tela < largura_tela + 300:

                DISPLAYSURF.blit(anomalia["img"], (x_tela, tunel_y))


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
        pass

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

        # Velocidade — mostra velocidade suave (posicao_vel converge gradualmente)
        y_offset += 30

        text = font.render(
            f"Velocidade: {round(self.posicao_vel / 4)}",
            True,
            BLACK
        )

        DISPLAYSURF.blit(
            text,
            (largura_tela - 300, y_offset)
        )

        # Posição X
        y_offset += 30

        text = font.render(
            f"Posição X: {self.posicao_x_real:.2f} m",
            True,
            BLACK
        )

        DISPLAYSURF.blit(
            text,
            (largura_tela - 300, y_offset)
        )

        # Limiar de variação e confiança
        y_offset += 30

        text = font.render(
            f"Limiar: {self.limiar:.1f}  Conf: {self.confianca_atual:.0%}",
            True,
            BLACK
        )

        DISPLAYSURF.blit(text, (largura_tela - 300, y_offset))

        # Instruções (abaixo do gráfico)
        y_offset = tunel_y + altura_tunel + 140

        small_font = pygame.font.Font(None, 18)

        instructions = [
            "CONTROLES:",
            "1-9: Ajustar velocidade",
            "A: Modo Automático | M: Modo Manual",
            "C: Câmera ON/OFF",
            "SETAS: Mover",
            "Q: Sair"
        ]

        for i, instr in enumerate(instructions):

            text = small_font.render(instr, True, BLACK)
            DISPLAYSURF.blit(text, (10, y_offset + i * 18))

    # GRÁFICO DO PERFIL DO TETO
    def draw_tunnel_profile(self):

        if len(self.tunnel_profile) < 2:
            return

        font_eixo = pygame.font.Font(None, 16)
        font_titulo = pygame.font.Font(None, 18)

        # Margens para acomodar os eixos e rótulos
        margem_esq = 75   # espaço para label Y rotacionado + ticks
        margem_dir = 15
        margem_top = 22   # espaço para título
        margem_bot = 32   # espaço para label X + ticks

        grafico_x = 10
        grafico_y = tunel_y + altura_tunel + 10
        grafico_w = largura_tela - 20
        grafico_h = 120

        # Fundo e borda
        pygame.draw.rect(DISPLAYSURF, (230, 230, 230),
                         (grafico_x, grafico_y, grafico_w, grafico_h))
        pygame.draw.rect(DISPLAYSURF, BLACK,
                         (grafico_x, grafico_y, grafico_w, grafico_h), 1)

        # Título centralizado
        titulo = font_titulo.render("GRÁFICO DE DADOS LIDAR PROCESSADOS", True, BLACK)
        DISPLAYSURF.blit(titulo, (grafico_x + grafico_w // 2 - titulo.get_width() // 2,
                                  grafico_y + 4))

        # Área de plotagem dentro das margens
        plot_x = grafico_x + margem_esq
        plot_y = grafico_y + margem_top
        plot_w = grafico_w - margem_esq - margem_dir
        plot_h = grafico_h - margem_top - margem_bot

        # Eixos
        pygame.draw.line(DISPLAYSURF, BLACK,
                         (plot_x, plot_y), (plot_x, plot_y + plot_h), 2)          # eixo Y
        pygame.draw.line(DISPLAYSURF, BLACK,
                         (plot_x, plot_y + plot_h), (plot_x + plot_w, plot_y + plot_h), 2)  # eixo X

        # Seta eixo X
        pygame.draw.polygon(DISPLAYSURF, BLACK, [
            (plot_x + plot_w + 6, plot_y + plot_h),
            (plot_x + plot_w, plot_y + plot_h - 4),
            (plot_x + plot_w, plot_y + plot_h + 4),
        ])

        # Seta eixo Y
        pygame.draw.polygon(DISPLAYSURF, BLACK, [
            (plot_x, plot_y - 6),
            (plot_x - 4, plot_y),
            (plot_x + 4, plot_y),
        ])

        # Normaliza os valores y
        ys = [p[1] for p in self.tunnel_profile]
        y_min, y_max = min(ys), max(ys)
        y_range = max(y_max - y_min, 1.0)

        # Ticks e labels do eixo Y (3 valores: min, meio, max) — alinhados à direita antes do eixo
        for i, val in enumerate([y_min, (y_min + y_max) / 2, y_max]):
            sy = plot_y + plot_h - int((val - y_min) / y_range * plot_h)
            pygame.draw.line(DISPLAYSURF, BLACK, (plot_x - 5, sy), (plot_x, sy), 1)
            lbl = font_eixo.render(f"{val:.0f}", True, BLACK)
            DISPLAYSURF.blit(lbl, (plot_x - 8 - lbl.get_width(), sy - lbl.get_height() // 2))

        # Ticks e labels do eixo X (posição X em metros, 4 pontos)
        xs = [p[0] for p in self.tunnel_profile]
        x_min, x_max = xs[0], xs[-1]
        x_range = max(x_max - x_min, 0.01)
        for i in range(4):
            frac = i / 3
            sx = plot_x + int(frac * plot_w)
            val_x = x_min + frac * x_range
            pygame.draw.line(DISPLAYSURF, BLACK, (sx, plot_y + plot_h), (sx, plot_y + plot_h + 4), 1)
            lbl = font_eixo.render(f"{val_x:.1f}m", True, BLACK)
            DISPLAYSURF.blit(lbl, (sx - lbl.get_width() // 2, plot_y + plot_h + 6))

        # Label eixo Y rotacionado — posicionado na faixa à esquerda dos ticks
        label_y_surf = font_eixo.render("Dist. Teto (mm)", True, BLACK)
        label_y_rot = pygame.transform.rotate(label_y_surf, 90)
        DISPLAYSURF.blit(label_y_rot, (grafico_x + 2,
                                       plot_y + plot_h // 2 - label_y_rot.get_height() // 2))

        # Label eixo X
        label_x = font_eixo.render("Posição Horizontal (m)", True, BLACK)
        DISPLAYSURF.blit(label_x, (plot_x + plot_w // 2 - label_x.get_width() // 2,
                                   plot_y + plot_h + 16))

        # Linha do perfil
        n = len(self.tunnel_profile)
        pontos = []
        for i, ponto_perfil in enumerate(self.tunnel_profile):
            py = ponto_perfil[1]
            sx = plot_x + int(i / max(n - 1, 1) * plot_w)
            sy = plot_y + plot_h - int((py - y_min) / y_range * plot_h)
            sy = max(plot_y, min(plot_y + plot_h, sy))
            pontos.append((sx, sy))

        if len(pontos) >= 2:
            pygame.draw.lines(DISPLAYSURF, BLUE, False, pontos, 2)

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

        # Gráfico do perfil do teto (recebido via MQTT)
        self.draw_tunnel_profile()

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
                        # Restaura velocidade padrão (j_sp_velocidade pode ter sido
                        # alterado pelas teclas numéricas no modo manual)
                        interface.velocidade_atual = 50
                        interface.send_velocity(50)

                    if event.key == pygame.K_m:

                        interface.send_mode(False)

                    # Câmera
                    if event.key == pygame.K_c:

                        interface.send_camera(
                            not interface.camera_on
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

                        interface.velocidade_atual = vel  # atualização local imediata
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