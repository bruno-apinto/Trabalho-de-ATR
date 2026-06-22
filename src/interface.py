import pygame, sys, ctypes
from pygame.locals import *
import mmap
from pathlib import Path
import time, math, random

BASE_DIR = Path(__file__).resolve().parent

SHM_FILE = "/tmp/memoria_compartilhada.bin" 

class MemoriaCompartilhada(ctypes.Structure):
    _fields_ = [
        ("i_encoder",        ctypes.c_bool),
        ("i_lidar",          ctypes.c_int),
        ("o_liga_camera",    ctypes.c_bool),
        ("o_aceleracao",     ctypes.c_int),
        ("e_inspecao",       ctypes.c_bool),
        ("e_automatico",     ctypes.c_bool),
        ("c_automatico",     ctypes.c_bool),
        ("c_man",            ctypes.c_bool),
        ("j_sp_velocidade",  ctypes.c_int),
        ("c_encerrar",       ctypes.c_bool),
        # Campos adicionados na Etapa 2 (devem espelhar o struct C++ exatamente)
        ("variacao_severa",  ctypes.c_float),
        ("posicao_x",        ctypes.c_float),
        ("perfil_y",         ctypes.c_float),
        ("perfil_confianca", ctypes.c_float),
        ("perfil_novo",      ctypes.c_bool),
    ]

def abrir_memoria_compartilhada(): #Abre a memória compartilhada criada pelo processo C++.
                                   #Faz algumas tentativas porque a interface pode iniciar quase junto da criação da memória.
    tamanho = ctypes.sizeof(MemoriaCompartilhada)

    for _ in range(100):
        try:
            arquivo = open(SHM_FILE, "r+b")
            mm = mmap.mmap(arquivo.fileno(), tamanho) #mapeia o arquivo em memória
            memoria = MemoriaCompartilhada.from_buffer(mm) # interpreta a struct MemoriaCompartilhada

            print("Interface conectada ao arquivo mapeado")

            return arquivo, mm, memoria
        
        except FileNotFoundError:
            print("Aguardando memória compartilhada ser criada pela main...")
            time.sleep(0.1)

    raise RuntimeError("Não foi possível abrir a memória compartilhada.")

pygame.init()

arquivo_shm, mmap_shm, memoria = abrir_memoria_compartilhada()

print("Interface conectada à memória compartilhada")

FPS = 30 # frames por segundo
fpsClock = pygame.time.Clock()

largura_tela = 1000
altura_tela = 600

DISPLAY = pygame.display.set_mode((largura_tela, altura_tela))
pygame.display.set_caption('Robot Animation')

WHITE = (255, 255, 255)

backImg = pygame.image.load(str(BASE_DIR / 'tunel.jpeg'))
altura_tunel = 400
backImg = pygame.transform.scale(backImg, (largura_tela, altura_tunel))

tunel_x = 0
tunel_y = (altura_tela - altura_tunel) // 2
base_teto = tunel_y + 45

carImg = pygame.image.load(str(BASE_DIR / 'carrinho.png'))
largura_carro = carImg.get_width()
altura_carro = carImg.get_height()

# Diminuir tamanho do carro
carImg = pygame.transform.scale(carImg, (int(largura_carro * 0.8), int(altura_carro * 0.8)))
largura_carro = carImg.get_width()
altura_carro = carImg.get_height()

carx = 0
cary = altura_tela // 2 - altura_carro // 2 + 82

# Carregar imagens de falhas
falha_buraco = pygame.image.load(str(BASE_DIR / 'falha_buraco.png'))
falha_buraco = pygame.transform.scale(falha_buraco, (250, 147))

falha_protuberancia = pygame.image.load(str(BASE_DIR / 'falha_protuberancia.png'))
falha_protuberancia = pygame.transform.scale(falha_protuberancia, (250, 147))


velocidade = 0.0 #velocidade do carrinho em pixels/segundo

posicao = 0.0 #posição do carrinho em pixels

anomalias = []

# Pool determinístico: gerado uma vez com seed fixo → mesma sequência em ambas as interfaces.
# Cada entrada i do pool define completamente a i-ésima anomalia (espaçamento, tipo, curva).
# Ambas as interfaces consomem o pool na mesma ordem → falhas sempre idênticas.
random.seed(42)
_N_POOL = 2000
_pool = [(random.randint(200, 700),
          random.choice(["buraco", "protuberancia"]),
          random.randint(30, 70)) for _ in range(_N_POOL)]
_seq = 0   # próximo índice do pool a consumir

def cria_anomalia(posicao_x):
    global _seq
    _, tipo, curva = _pool[_seq % _N_POOL]
    _seq += 1
    imagem = falha_buraco if tipo == "buraco" else falha_protuberancia
    redimensionada = pygame.transform.smoothscale(imagem, (230, 149))
    return {"tipo": tipo, "x": float(posicao_x), "largura": 200, "altura": 130,
            "curva": curva, "img": redimensionada}

def anomalias_iniciais():
    global _seq
    _seq = 0
    anomalias.clear()
    proxima_posicao = largura_tela
    for i in range(5):
        proxima_posicao += _pool[i][0]   # espaçamento do pool (mesma entrada que cria_anomalia usará)
        anomalias.append(cria_anomalia(proxima_posicao))

def anomalias_automaticas():
    if not anomalias:
        return
    maior_posicao = max(a["x"] for a in anomalias)
    for indice, anomalia in enumerate(anomalias):
        if anomalia["x"] + anomalia["largura"] < posicao - 200:
            maior_posicao += _pool[_seq % _N_POOL][0]   # espaçamento da próxima entrada do pool
            anomalias[indice] = cria_anomalia(maior_posicao)

anomalias_iniciais()

def desenha_anomalias():
    for anomalia in anomalias:
        x_tela = int(anomalia["x"] - posicao)
        largura = anomalia["largura"]

        if (x_tela < -largura or x_tela > largura_tela):
            continue

        DISPLAY.blit(anomalia["img"], (x_tela, tunel_y))

def calcula_lidar():

    sensor_x_posicao = (posicao + carx)

    leitura = float(base_teto)

    for anomalia in anomalias:
        inicio = anomalia["x"]

        fim = (anomalia["x"] + anomalia["largura"])

        if inicio <= sensor_x_posicao <= fim:
            proporcao = (sensor_x_posicao - inicio) / anomalia["largura"]

            variacao = (anomalia["curva"]* math.sin(math.pi * proporcao))

            if anomalia["tipo"] == "buraco":
                # O teto está mais distante.
                leitura += variacao

            elif anomalia["tipo"] == "protuberancia":
                # O teto está mais próximo.
                leitura -= variacao

            break

    leitura += random.gauss(0, 2.0)  # ruído gaussiano ±2mm (sensor real)
    return max(0, int(round(leitura)))

def atualiza_sensores():
    memoria.i_encoder = bool((int(posicao / 100))%2) # cada 100 pixels é equivalente a 1 unidade do encoder
    memoria.i_lidar = calcula_lidar()

def display_lidar():
    sensorx = int(carx + 70)
    sensory = cary + 24
    pygame.draw.line(DISPLAY, (0, 255, 0), (sensorx, sensory), (sensorx, base_teto), 2)


def movimenta_carrinho(delta_tempo):
    global posicao, velocidade

    # j_sp_velocidade (0-100) é mapeado para pixels/s com fator 4 → sp=50 ≈ 200 px/s
    # o_aceleracao é um ajuste adicional (ex: -30 durante inspeção)
    alvo = float(memoria.j_sp_velocidade) * 4.0 + float(memoria.o_aceleracao) * 4.0

    # Convergência suave com tau ≈ 0.07s (fator 15.0 → ~2 frames a 30fps)
    velocidade += (alvo - velocidade) * min(delta_tempo * 15.0, 1.0)
    posicao += velocidade * delta_tempo

def encerra_interface(): 
    global memoria
    try:
        memoria.c_encerrar = True
        memoria.j_sp_velocidade = 0
        memoria.c_automatico = False
        memoria.c_man = True
        mmap_shm.flush() # garante que as alterações sejam escritas na memória compartilhada
    except Exception:
        pass
    try:
        del memoria
        mmap_shm.close()
        arquivo_shm.close()
    except Exception:
        pass

    pygame.quit()
    sys.exit()

while True: # ciclo principal

    delta_tempo = fpsClock.tick(FPS) / 1000.0
    delta_tempo = min(delta_tempo, 0.1) # limita o delta para evitar saltos grandes

    for event in pygame.event.get():
        if event.type == QUIT:
            encerra_interface()

        if event.type == KEYDOWN:
            if event.key == K_SPACE: # espaço: liga/desliga automático

                if (memoria.j_sp_velocidade == 0):
                    memoria.c_automatico = True
                    memoria.c_man = False
                    memoria.e_automatico = True
                    memoria.j_sp_velocidade = 50
                else:
                    memoria.j_sp_velocidade = 0

            # M: modo manual (mantém velocidade atual; use setas para ajustar)
            if event.key == K_m:
                memoria.c_man = True
                memoria.c_automatico = False
                memoria.e_automatico = False

            # Seta para cima: aumenta setpoint de velocidade
            if event.key == K_UP:
                memoria.j_sp_velocidade = min(100, memoria.j_sp_velocidade + 10)

            # Seta para baixo: diminui setpoint de velocidade
            if event.key == K_DOWN:
                memoria.j_sp_velocidade = max(0, memoria.j_sp_velocidade - 10)

    movimenta_carrinho(delta_tempo)
    anomalias_automaticas()
    atualiza_sensores()

    tunel_x = -int(posicao % largura_tela)

    DISPLAY.fill(WHITE)

    DISPLAY.blit(backImg, (tunel_x, tunel_y))
    DISPLAY.blit(backImg, (tunel_x + largura_tela, tunel_y))
    DISPLAY.blit(backImg, (tunel_x - largura_tela, tunel_y))

    desenha_anomalias()
    display_lidar()


    DISPLAY.blit(carImg, (carx, cary))

    pygame.display.update()