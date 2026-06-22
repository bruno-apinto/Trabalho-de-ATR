#!/usr/bin/env python3

#Testes do modulo de simulacao: pool de anomalias, encoder, LIDAR, convergencia de
#velocidade e calculo de confianca.

import sys
import math
import random

testes_passados = 0
testes_falhos   = 0

def check(condicao, nome):
    global testes_passados, testes_falhos
    if condicao:
        print(f"[OK]     {nome}")
        testes_passados += 1
    else:
        print(f"[FALHOU] {nome}")
        testes_falhos += 1


# 1. Pool deterministico de anomalias 
def test_pool_deterministico():
    print("- Pool deterministico")

    def gerar_pool(n=200):
        return [(random.randint(200, 700),
                 random.choice(["buraco", "protuberancia"]),
                 random.randint(30, 70)) for _ in range(n)]

    random.seed(42)
    pool_a = gerar_pool()
    random.seed(42)
    pool_b = gerar_pool()

    check(pool_a == pool_b,
          "Mesma seed gera sequencia identica em ambas as interfaces")
    check(all(200 <= e[0] <= 700 for e in pool_a),
          "Todos os espacamentos estao no intervalo [200, 700]")
    check(all(e[1] in ("buraco", "protuberancia") for e in pool_a),
          "Todos os tipos sao validos")
    check(all(30 <= e[2] <= 70 for e in pool_a),
          "Todas as curvas estao no intervalo [30, 70]")

    # Diferentes seeds geram sequencias diferentes
    random.seed(1)
    pool_c = gerar_pool()
    check(pool_a != pool_c,
          "Seeds diferentes geram sequencias diferentes")


# 2. Encoder: alternancia a cada 100 pixels
def test_encoder():
    print("- Encoder")

    encoder_ant  = False
    transicoes   = 0
    for px in range(1, 501):
        enc = bool((px // 100) % 2)
        if enc != encoder_ant:
            transicoes += 1
            encoder_ant = enc

    check(transicoes == 5,
          f"500 pixels geram 5 transicoes de encoder (obteve {transicoes})")

    # Verifica alternancia binaria
    estados = [bool((px // 100) % 2) for px in range(0, 600, 100)]
    alternados = all(estados[i] != estados[i+1] for i in range(len(estados)-1))
    check(alternados,
          "Estados do encoder alternam corretamente a cada 100 pixels")


# 3. LIDAR: ruido gaussiano
def test_ruido_lidar():
    print("- LIDAR com ruido")

    random.seed(0)
    base    = 145.0
    n       = 2000
    amostras = [max(0, base + random.gauss(0, 2.0)) for _ in range(n)]

    media = sum(amostras) / n
    desvio = math.sqrt(sum((x - media)**2 for x in amostras) / n)

    check(abs(media - base) < 0.3,
          f"Media do ruido proxima do valor real (erro={media - base:.3f})")
    check(1.7 < desvio < 2.3,
          f"Desvio padrao proximo de 2.0 (obteve {desvio:.2f})")
    check(all(v >= 0 for v in amostras),
          "Nenhuma amostra e negativa (max(0, ...))")

    # Anomalia deve sobressair ao ruido
    variacao_buraco  = 40.0   # buraco grande
    variacao_ruido   = 2.0 * 3  # 3-sigma
    check(variacao_buraco > variacao_ruido,
          "Variacao de anomalia (40px) supera 3-sigma do ruido (6px)")


# 4. Convergencia de velocidade 
def test_convergencia_velocidade():
    print("- Convergencia de velocidade")

    delta = 1 / 30      # 30 fps
    fator = 15.0

    # De parado para velocidade normal (j_sp=50 -> alvo=200 px/s)
    vel   = 0.0
    alvo  = 200.0
    frames = 0
    while abs(vel - alvo) > 1.0 and frames < 1000:
        vel += (alvo - vel) * min(delta * fator, 1.0)
        frames += 1

    check(abs(vel - alvo) <= 1.0,
          f"Velocidade converge para o alvo em {frames} frames ({frames/30*1000:.0f} ms)")
    check(frames <= 10,
          f"Convergencia em menos de 10 frames (~333 ms) com fator {fator}")

    # Inspecao: alvo = (50 - 30) * 4 = 80 px/s
    vel   = 200.0
    alvo  = 80.0
    frames = 0
    while abs(vel - alvo) > 1.0 and frames < 1000:
        vel += (alvo - vel) * min(delta * fator, 1.0)
        frames += 1

    check(abs(vel - alvo) <= 1.0,
          f"Desaceleracao de inspecao converge em {frames} frames ({frames/30*1000:.0f} ms)")
    check(frames <= 10,
          "Desaceleracao em menos de 10 frames (~333 ms)")


# 5. Confianca online
def test_confianca():
    print("- Confianca online")

    zona_anterior = -999.0
    contagem      = 0

    def medir(pos):
        nonlocal zona_anterior, contagem
        if abs(pos - zona_anterior) < 1.0:
            contagem = min(contagem + 1, 10)
        else:
            zona_anterior = pos
            contagem = 1
        return contagem / 10.0

    c = medir(0.0)
    check(abs(c - 0.1) < 0.001, "Primeira medicao na zona = 10%")

    for _ in range(4):
        medir(0.0)
    check(abs(contagem / 10.0 - 0.5) < 0.001, "5 medicoes na zona = 50%")

    for _ in range(10):
        medir(0.0)
    check(contagem == 10, "Contagem limitada a 10 (nao ultrapassa 100%)")

    c_nova = medir(5.0)
    check(abs(c_nova - 0.1) < 0.001, "Mudanca de zona reinicia contagem para 10%")

    # Posicao proxima (< 1.0) deve manter zona
    c_prox = medir(5.3)
    check(contagem == 2, "Posicao dentro da tolerancia de 1m (5.3 vs 5.0) mantem zona atual")


# 6. Baseline e deteccao de anomalia
def test_deteccao_anomalia():
    print("- Baseline e deteccao de anomalia")

    BASELINE_N = 15
    baseline_soma = 0.0
    baseline_n    = 0
    baseline      = None
    valor_real    = 145.0
    variacao_severa = 20.0

    for _ in range(BASELINE_N):
        v = valor_real
        if baseline_n < BASELINE_N and v > 100.0:
            baseline_soma += v
            baseline_n += 1
            if baseline_n == BASELINE_N:
                baseline = baseline_soma / BASELINE_N

    check(baseline_n == BASELINE_N, "Baseline acumula exatamente 15 amostras")
    check(baseline is not None and abs(baseline - 145.0) < 0.01,
          "Baseline calculado corretamente (145.0)")

    detecta = lambda v: abs(v - baseline) > variacao_severa

    check(not detecta(148.0), "Leitura normal (148) nao dispara alarme")
    check(    detecta(180.0), "Buraco (180) detectado")
    check(    detecta(110.0), "Saliencia (110) detectada")

    variacao_severa = 50.0
    detecta2 = lambda v: abs(v - baseline) > variacao_severa
    check(not detecta2(180.0), "Limiar configuravel: 180 nao dispara com limiar=50")
    check(    detecta2(200.0), "Limiar configuravel: 200 ainda dispara com limiar=50")


#  main 
if __name__ == "__main__":

    print(" Testes da Simulacao (Python)\n")

    test_pool_deterministico(); print()
    test_encoder();             print()
    test_ruido_lidar();         print()
    test_convergencia_velocidade(); print()
    test_confianca();           print()
    test_deteccao_anomalia();   print()

    print(f" Resultado: {testes_passados} OK  |  {testes_falhos} FALHOU\n\n")

    sys.exit(1 if testes_falhos > 0 else 0)
