import pygame, sys
import sys
import ctypes
from pygame.locals import *
import mmap
from pathlib import Path
import time

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

DISPLAYSURF = pygame.display.set_mode((largura_tela, altura_tela))
pygame.display.set_caption('Car Animation')

WHITE = (255, 255, 255)

backImg = pygame.image.load(str(BASE_DIR / 'tunel.jpeg'))
altura_tunel = 400
backImg = pygame.transform.scale(backImg, (largura_tela, altura_tunel))

tunel_x = 0
tunel_y = (altura_tela - altura_tunel) // 2

carImg = pygame.image.load(str(BASE_DIR / 'carrinho.png'))
largura_carro = carImg.get_width()
altura_carro = carImg.get_height()

carx = 0
cary = altura_tela // 2 - altura_carro // 2 + 65

velocidade = 5

andando = False

def encerra_interface(): 
    global memoria
    try:
        memoria.c_encerrar = True
        memoria.j_sp_velocidade = 0
        memoria.c_automatico = False
        memoria.c_man = True
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

    for event in pygame.event.get():
        if event.type == QUIT:
            encerra_interface()

        if event.type == KEYDOWN:
            if event.key == K_SPACE: # espaço: liga/desliga automático
                andando = not andando

                if andando:
                    memoria.c_automatico = True
                    memoria.c_man = False
                    memoria.e_automatico = True

                    if memoria.j_sp_velocidade == 0:
                        memoria.j_sp_velocidade == 50
                else:
                    memoria.j_sp_velocidade = 0

            # M: modo manual
            if event.key == K_m:
                andando = False

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

            # C: teste manual para ligar câmera
            if event.key == K_c:
                memoria.o_liga_camera = True

            # I: teste manual para estado de inspeção
            if event.key == K_i:
                memoria.e_inspecao = not memoria.e_inspecao


    DISPLAYSURF.fill(WHITE)

    keys = pygame.key.get_pressed()

    # Se apertar espaço uma vez, o carrinho começa a andar sozinho
    if andando:
        tunel_x -= velocidade

    # Movimento manual opcional
    if keys[K_LEFT]:
        tunel_x += velocidade

    if keys[K_RIGHT]:
        tunel_x -= velocidade

    # Repetição infinita do túnel
    if tunel_x <= -largura_tela:
        tunel_x = 0

    if tunel_x >= largura_tela:
        tunel_x = 0

    DISPLAYSURF.blit(backImg, (tunel_x, tunel_y))
    DISPLAYSURF.blit(backImg, (tunel_x + largura_tela, tunel_y))
    DISPLAYSURF.blit(backImg, (tunel_x - largura_tela, tunel_y))

    DISPLAYSURF.blit(carImg, (carx, cary))

    pygame.display.update()
    fpsClock.tick(FPS)