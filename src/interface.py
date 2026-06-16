import pygame, sys, ctypes
from pygame.locals import *
import mmap
from pathlib import Path
import time, math, os, random

BASE_DIR = Path(__file__).resolve().parent

SHM_FILE = "/tmp/memoria_compartilhada.bin" 

class MemoriaCompartilhada(ctypes.Structure):
    _fields_ = [
        ("i_encoder", ctypes.c_bool),
        ("i_lidar", ctypes.c_int),

        ("o_liga_camera", ctypes.c_bool),
        ("o_aceleracao", ctypes.c_int),

        ("e_inspecao", ctypes.c_bool),
        ("e_automatico", ctypes.c_bool),
        ("c_automatico", ctypes.c_bool),
        ("c_man", ctypes.c_bool),
        ("j_sp_velocidade", ctypes.c_int),

        ("c_encerrar", ctypes.c_bool),
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

distancia_teto  = cary + base_teto # distância do teto até o topo do carro
altura_buraco = falha_buraco.get_height() # altura das falhas
altura_protuberancia = falha_protuberancia.get_height() # altura da protuberância

# Posições das falhas na parte superior do túnel (teto)
falhas = [
    {'img': falha_buraco, 'x': 300, 'y': tunel_y + 0},
    {'img': falha_protuberancia, 'x': 700, 'y': tunel_y + 0},
]

velocidade = 0.0 #velocidade do carrinho em pixels/segundo

posicao = 0.0 #posição do carrinho em pixels

anomalias = [] #lista de anomalias detectadas, para exibir na interface

def cria_anomalia(posicao_x):
    tipo = random.choice(["buraco", "protuberancia"])

    if tipo == "buraco":
        imagem = falha_buraco
    else:
        imagem = falha_protuberancia
    
    redimensionada = pygame.transform.smoothscale(imagem, (200, 120))
    curva = random.randint(30, 70)

    return {"tipo": tipo, "x": float(posicao_x), "largura": 200, "altura": 120, "curva": curva, "img": redimensionada}

def anomalias_iniciais():
    anomalias.clear() # apaga anomalias atuais
    proxima_posicao = largura_tela
    for _ in range(5):
        proxima_posicao += random.randint(200, 700)
        anomalias.append(cria_anomalia(proxima_posicao))

def anomalias_automaticas(): # faz o fundo ficar infinito

    if not anomalias:
        return
    
    maior_posicao = max((anomalia["x"] for anomalia in anomalias))

    for indice, anomalia in enumerate(anomalias):
        fim_anomalia = (anomalia["x"] + anomalia["largura"])

        if fim_anomalia < posicao - 200:
            maior_posicao += random.randint(200, 700)
            anomalias[indice] = cria_anomalia(maior_posicao)

anomalias_iniciais() # inicializa as anomalias

def desenha_anomalias():
    for anomalia in anomalias:
        x_tela = int(anomalia["x"] - posicao)
        largura = anomalia["largura"]
        altura = anomalia["altura"]

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

    return max(0, int(round(leitura)))

def atualiza_sensores():
    memoria.i_encoder = bool((int(posicao / 100))%2) # cada 100 pixels é equivalente a 1 unidade do encoder
    memoria.i_lidar = calcula_lidar()

def display_lidar():
    sensorx = int(carx + 70)
    sensory = cary + 24
    diferenca = distancia_teto
    pygame.draw.line(DISPLAY, (0, 255, 0), (sensorx, sensory), (sensorx, base_teto), 2)

def alertas():
    if memoria.e_inspecao:
        font = pygame.font.SysFont(None, 36)
        texto = font.render("INSPEÇÃO ATIVA", True, (255, 0, 0))
        DISPLAY.blit(texto, (10, 10))

def movimenta_carrinho(delta_tempo):
    global posicao, velocidade

    aceleracao = max(int(memoria.o_aceleracao), 0.5)
    velocidade += 50 * aceleracao * delta_tempo
    posicao += velocidade * delta_tempo

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

            # M: modo manual
            if event.key == K_m:
                memoria.c_man = True
                memoria.c_automatico = False
                memoria.e_automatico = False
                memoria.j_sp_velocidade = 0

            # Seta para cima: aumenta setpoint de velocidade
            if event.key == K_UP:
                memoria.j_sp_velocidade = min(100, memoria.j_sp_velocidade + 10)

            # Seta para baixo: diminui setpoint de velocidade
            if event.key == K_DOWN:
                memoria.j_sp_velocidade = max(0, memoria.j_sp_velocidade - 10)

    movimenta_carrinho(delta_tempo)
    atualiza_sensores()

    tunel_x = -int(posicao % largura_tela)

    DISPLAY.fill(WHITE)

    DISPLAY.blit(backImg, (tunel_x, tunel_y))
    DISPLAY.blit(backImg, (tunel_x + largura_tela, tunel_y))
    DISPLAY.blit(backImg, (tunel_x - largura_tela, tunel_y))

    desenha_anomalias()
    display_lidar()
    alertas()

    DISPLAY.blit(carImg, (carx, cary))

    pygame.display.update()