#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <vector>

// Framework minimo de testes
static int testes_passados = 0;
static int testes_falhos   = 0;

void check(bool condicao, const std::string& nome) {
    if (condicao) {
        std::cout << "[OK]     " << nome << "\n";
        testes_passados++;
    } else {
        std::cout << "[FALHOU] " << nome << "\n";
        testes_falhos++;
    }
}

// 1. Encoder: deteccao de transicao 
void test_encoder() {
    std::cout << "- Encoder\n";

    bool ultimo = false;
    int posicao_x = 0;

    // Primeira transicao 0->1
    bool atual = true;
    if (atual != ultimo) { posicao_x++; ultimo = atual; }
    check(posicao_x == 1, "Transicao 0->1 incrementa posicao_x");

    // Mesmo estado: sem incremento
    atual = true;
    if (atual != ultimo) { posicao_x++; ultimo = atual; }
    check(posicao_x == 1, "Estado repetido nao incrementa posicao_x");

    // Segunda transicao 1->0
    atual = false;
    if (atual != ultimo) { posicao_x++; ultimo = atual; }
    check(posicao_x == 2, "Transicao 1->0 incrementa posicao_x");

    // Sequencia de 10 transicoes
    for (int i = 0; i < 10; i++) {
        atual = !ultimo;
        if (atual != ultimo) { posicao_x++; ultimo = atual; }
    }
    check(posicao_x == 12, "10 transicoes consecutivas incrementam 10 vezes");
}

// 2. Filtro de media movel (janela de 2 amostras) 
void test_filtro_media_movel() {
    std::cout << "- Filtro de media movel\n";

    float janela[2] = {};
    int   idx       = 0;
    bool  cheia     = false;

    auto inserir = [&](float v) -> float {
        janela[idx % 2] = v;
        idx++;
        if (idx >= 2) cheia = true;
        int n = cheia ? 2 : idx;
        float soma = 0;
        for (int i = 0; i < n; i++) soma += janela[i];
        return soma / n;
    };

    float m1 = inserir(100.0f);
    check(std::fabs(m1 - 100.0f) < 0.01f, "Primeira amostra retorna valor puro (janela incompleta)");

    float m2 = inserir(200.0f);
    check(std::fabs(m2 - 150.0f) < 0.01f, "Media de 100 e 200 = 150");

    float m3 = inserir(50.0f);
    check(std::fabs(m3 - 125.0f) < 0.01f, "Janela deslizante: media de 200 e 50 = 125");

    float m4 = inserir(50.0f);
    check(std::fabs(m4 - 50.0f) < 0.01f,  "Duas amostras identicas: media = proprio valor");
}

// 3. Calculo do baseline
void test_baseline() {
    std::cout << "- Baseline\n";

    const int BASELINE_AMOSTRAS = 15;
    float baseline      = 0.0f;
    float baseline_soma = 0.0f;
    int   baseline_n    = 0;

    // Amostras validas (> 100)
    for (int i = 0; i < BASELINE_AMOSTRAS; i++) {
        float v = 145.0f;
        if (baseline_n < BASELINE_AMOSTRAS && v > 100.0f) {
            baseline_soma += v;
            baseline_n++;
            if (baseline_n == BASELINE_AMOSTRAS)
                baseline = baseline_soma / BASELINE_AMOSTRAS;
        }
    }
    check(baseline_n == BASELINE_AMOSTRAS, "Acumula exatamente 15 amostras");
    check(std::fabs(baseline - 145.0f) < 0.01f, "Baseline calculado corretamente");

    // Amostras invalidas (< 100) devem ser rejeitadas
    int n_invalido = 0;
    float v_invalido = 50.0f;
    if (v_invalido > 100.0f) n_invalido++;
    check(n_invalido == 0, "Amostras abaixo de 100 sao rejeitadas no startup");

    // Amostra adicional nao altera baseline ja estabelecido
    int n_antes = baseline_n;
    float v_extra = 200.0f;
    if (baseline_n < BASELINE_AMOSTRAS && v_extra > 100.0f) baseline_n++;
    check(baseline_n == n_antes, "Baseline nao e alterado apos estabelecido");
}

// 4. Deteccao de anomalia 
void test_deteccao_anomalia() {
    std::cout << "- Deteccao de anomalia\n";

    float baseline       = 145.0f;
    float variacao_severa = 20.0f;

    auto detecta = [&](float leitura) {
        return std::fabs(leitura - baseline) > variacao_severa;
    };

    check(!detecta(148.0f), "Leitura normal (148) nao dispara alarme");
    check(!detecta(145.0f), "Leitura identica ao baseline nao dispara alarme");
    check( detecta(180.0f), "Buraco (teto mais distante: 180) detectado");
    check( detecta(110.0f), "Saliencia (teto mais proximo: 110) detectada");
    check(!detecta(164.9f), "Leitura no limite (164.9, variacao=19.9) nao dispara");
    check( detecta(165.1f), "Leitura acima do limite (165.1, variacao=20.1) dispara");

    // Limiar configuravel
    variacao_severa = 50.0f;
    check(!detecta(180.0f), "Limiar aumentado para 50: leitura 180 nao dispara mais");
    check( detecta(200.0f), "Limiar aumentado para 50: leitura 200 ainda dispara");
}

// 5. Calculo de confianca
void test_confianca() {
    std::cout << "- Confianca\n";

    float zona_anterior = -999.0f;
    int   contagem      = 0;

    auto medir = [&](float pos) -> float {
        if (std::fabs(pos - zona_anterior) < 1.0f)
            contagem = std::min(contagem + 1, 10);
        else {
            zona_anterior = pos;
            contagem = 1;
        }
        return contagem / 10.0f;
    };

    float c1 = medir(0.0f);
    check(std::fabs(c1 - 0.1f) < 0.01f, "Primeira medicao na zona = 10%");

    for (int i = 0; i < 4; i++) medir(0.0f);
    check(std::fabs(contagem / 10.0f - 0.5f) < 0.01f, "5 medicoes na mesma zona = 50%");

    for (int i = 0; i < 10; i++) medir(0.0f);
    check(contagem == 10, "Contagem limitada a 10 (nao ultrapassa 100%)");

    float c_nova = medir(5.0f);
    check(std::fabs(c_nova - 0.1f) < 0.01f, "Mudanca de zona reinicia contagem para 10%");
}

// 6. Logica de troca de modo 
void test_modo_navegacao() {
    std::cout << "- Modo de navegacao\n";

    bool e_automatico = false;
    bool c_automatico = false;
    bool c_man        = false;

    // Ativar automatico
    c_automatico = true;
    if (c_automatico) { e_automatico = true;  c_automatico = false; }
    check(e_automatico  == true,  "c_automatico ativa e_automatico");
    check(c_automatico  == false, "c_automatico e resetado apos processamento");

    // Ativar manual
    c_man = true;
    if (c_man) { e_automatico = false; c_man = false; }
    check(e_automatico == false, "c_man desativa e_automatico");
    check(c_man        == false, "c_man e resetado apos processamento");

    // Comandos simultaneos: manual tem precedencia (ordem de processamento)
    c_automatico = true;
    c_man        = true;
    if (c_automatico) { e_automatico = true;  c_automatico = false; }
    if (c_man)        { e_automatico = false; c_man = false; }
    check(e_automatico == false, "Quando ambos os comandos chegam juntos, manual tem precedencia");
}

// 7. Clamp da saida do PID
void test_pid_clamp() {
    std::cout << "- PID clamp\n";

    auto clamp = [](float v, float lo, float hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    };

    check(clamp( 150.0f, -100.0f, 100.0f) ==  100.0f, "Saida acima de 100 limitada a 100");
    check(clamp(-150.0f, -100.0f, 100.0f) == -100.0f, "Saida abaixo de -100 limitada a -100");
    check(clamp(  50.0f, -100.0f, 100.0f) ==   50.0f, "Saida dentro dos limites nao e alterada");
    check(clamp(   0.0f, -100.0f, 100.0f) ==    0.0f, "Saida zero nao e alterada");
    check(clamp( 100.0f, -100.0f, 100.0f) ==  100.0f, "Saida exatamente no limite superior e mantida");
    check(clamp(-100.0f, -100.0f, 100.0f) == -100.0f, "Saida exatamente no limite inferior e mantida");
}

// main
int main() {

    std::cout << " Testes do Sistema de Inspecao de Tuneis\n\n";

    test_encoder();             std::cout << "\n";
    test_filtro_media_movel();  std::cout << "\n";
    test_baseline();            std::cout << "\n";
    test_deteccao_anomalia();   std::cout << "\n";
    test_confianca();           std::cout << "\n";
    test_modo_navegacao();      std::cout << "\n";
    test_pid_clamp();           std::cout << "\n";

    std::cout << " Resultado: " << testes_passados << " OK  |  "
              << testes_falhos << " FALHOU\n\n";

    return testes_falhos > 0 ? 1 : 0;
}
