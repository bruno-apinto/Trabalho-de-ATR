import pygame, sys

from pygame.locals import *

from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent

pygame.init()

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

# Posições das falhas na parte superior do túnel (teto)
falhas = [
    {'img': falha_buraco, 'x': 300, 'y': tunel_y + 0},
    {'img': falha_protuberancia, 'x': 700, 'y': tunel_y + 0},
]

velocidade = 5

andando = False

while True: # ciclo principal

    for event in pygame.event.get():
        if event.type == QUIT:
            pygame.quit()
            sys.exit()

        if event.type == KEYDOWN:
            if event.key == K_SPACE:
                andando = not andando

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

    # Desenhar falhas na parte superior do túnel
    for falha in falhas:
        falha_x = falha['x'] + tunel_x
        falha_y = falha['y']
        DISPLAYSURF.blit(falha['img'], (falha_x, falha_y))
        # Repetir falhas para o efeito de loop infinito
        DISPLAYSURF.blit(falha['img'], (falha_x + largura_tela, falha_y))
        DISPLAYSURF.blit(falha['img'], (falha_x - largura_tela, falha_y))

    DISPLAYSURF.blit(carImg, (carx, cary))

    pygame.display.update()
    fpsClock.tick(FPS)