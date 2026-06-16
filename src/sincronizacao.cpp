#include "sincronizacao.hpp"

#define ELEMENTOS_BUFFERS 10

//sincronização
std::condition_variable camera;

//teste para finalizar a camera
int eventos_camera = 0;
bool finalizar_camera = false;

//Variaveis de condição buffers
std::condition_variable leitura_buffer_navegacao;
std::condition_variable escrita_buffer_navegacao;

int AR_NAVEGACAO = 0;
int WR_NAVEGACAO = 0;
int AW_NAVEGACAO = 0;
int WW_NAVEGACAO = 0;

std::condition_variable leitura_buffer_nivel; 
std::condition_variable escrita_buffer_nivel;
int dados_nivel = 0;

//Funções auxiliares de debbug
std::mutex mutex_log;
int eventos_camera = 0;
int miss[4] = {0, 0, 0, 0};
int executado[4] = {0, 0, 0, 0};

// Tolerância máxima de atraso em microssegundos antes de considerar um "Miss de Deadline"
const long long LIMITE_ATRASO_US = 5000; 

void log_message(const std::string& thread, const std::string& mensagem) {
    std::lock_guard<std::mutex> lock(mutex_log);
    auto agora = std::chrono::system_clock::now();
    auto tempo = std::chrono::system_clock::to_time_t(agora);
    std::cout << "[" << std::put_time(std::localtime(&tempo), "%H:%M:%S") << "] "
              << "[" << thread << "] " << mensagem << std::endl;
}

float numero_aleatorio_debugg() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(1.0f, 100.0f);
    return dis(gen);
}

// =========================================================================
// FUNÇÃO AUXILIAR DE TEMPO REAL
// =========================================================================
void reagendar_tarefa(boost::asio::steady_timer* t, int periodo_us, const std::string& nome_tarefa) {
    auto agora = std::chrono::steady_clock::now();
    auto atraso = std::chrono::duration_cast<std::chrono::microseconds>(agora - t->expiry()).count();

    // Comentado para não poluir o log no modo Oversampling rápido
    // if (atraso > LIMITE_ATRASO_US) {
    //     log_message(nome_tarefa, "ALERTA: Deadline violado! Atraso de " + std::to_string(atraso) + " us");
    // }
void comando_navegacao(MemoriaCompartilhada* shm){

    shm->j_sp_velocidade = 50; // teste de escrita na memória compartilhada
    shm->c_automatico = true;
    shm->c_man = false;
    shm->e_automatico = true;

    log_message(
        "COMANDO",
        std::string("Comando de navegação escreveu na memória compartilhada: ") +
        "c_automatico = " + std::to_string(shm->c_automatico) +
        " | j_sp_velocidade = " + std::to_string(shm->j_sp_velocidade)
    );

    auto proximo_ciclo = t->expiry() + boost::asio::chrono::microseconds(periodo_us);
    
    if (proximo_ciclo < agora) {
        proximo_ciclo = agora + boost::asio::chrono::microseconds(periodo_us);
    }
    
    t->expires_at(proximo_ciclo);
}

// =========================================================================
// TAREFAS ASSÍNCRONAS DO SISTEMA
// =========================================================================

void comando_navegacao(const boost::system::error_code& e, boost::asio::steady_timer* t, 
                       boost::asio::io_context::strand* strand, MemoriaCompartilhada* shm) 
{
    if (e) return;
    if (!shm->c_automatico) {
        log_message("COMANDO", "Modo automático desativado. Encerrando.");
        return;
    }

    // Lógica do comando...

    reagendar_tarefa(t, PERIODO_COMANDO, "COMANDO");
    t->async_wait(boost::asio::bind_executor(*strand, 
        std::bind(comando_navegacao, std::placeholders::_1, t, strand, shm)));
}


void controle_navegacao(const boost::system::error_code& e, boost::asio::steady_timer* t, 
                        boost::asio::io_context::strand* strand, MemoriaCompartilhada* shm, VarCondSinc &sinc) 
{
    if (e) return;
    if (!shm->c_automatico) return;

    bool processou_algo = false;

    {
        std::lock_guard<std::mutex> lock(sinc.mtx_navegacao);
        
        while (sinc.disp_naveg_c > 0) {
            float leitura = sinc.BUFFER_NAVEGACAO[sinc.LER_IDX_NAVEG_C];
            sinc.LER_IDX_NAVEG_C = (sinc.LER_IDX_NAVEG_C + 1) % ELEMENTOS_BUFFERS;
            sinc.disp_naveg_c--;
            
            processou_algo = true;
        }
    }

    if (processou_algo) {
        executado[0]++;
    } else {
        miss[0]++; // Continua contabilizando miss apenas se não houver NADA para ler
    }

    reagendar_tarefa(t, PERIODO_CONTROLE, "CONTROLE");
    t->async_wait(boost::asio::bind_executor(*strand, 
        std::bind(controle_navegacao, std::placeholders::_1, t, strand, shm, std::ref(sinc))));
}


void distancia_percorrida(const boost::system::error_code& e, boost::asio::steady_timer* t, 
                          boost::asio::io_context::strand* strand, MemoriaCompartilhada* shm, VarCondSinc &sinc) 
{
    if (e) return;
    if (!shm->c_automatico) return;

    {
        std::lock_guard<std::mutex> lock(sinc.mtx_navegacao);
        
        // --- LÓGICA DE OVERSAMPLING (SOBRESCRITA) ---
        // Se o leitor do Controle ficou para trás, avança a leitura à força
        if (sinc.disp_naveg_c >= ELEMENTOS_BUFFERS) {
            sinc.LER_IDX_NAVEG_C = (sinc.LER_IDX_NAVEG_C + 1) % ELEMENTOS_BUFFERS;
            sinc.disp_naveg_c--; 
        }
        
        // Se o leitor da Reconstrução ficou para trás, avança a leitura à força
        if (sinc.disp_naveg_r >= ELEMENTOS_BUFFERS) {
            sinc.LER_IDX_NAVEG_R = (sinc.LER_IDX_NAVEG_R + 1) % ELEMENTOS_BUFFERS;
            sinc.disp_naveg_r--;
        }

        // Escreve o dado fresco com garantia de espaço
        sinc.BUFFER_NAVEGACAO[sinc.ESC_IDX_NAVEG] = 1.0f; // Teste com valor fixo
        sinc.ESC_IDX_NAVEG = (sinc.ESC_IDX_NAVEG + 1) % ELEMENTOS_BUFFERS;
        
        sinc.disp_naveg_c++; // Avisa o Controle
        sinc.disp_naveg_r++; // Avisa a Reconstrucao
    }

    executado[1]++; // Como sempre escreve, nunca vai dar Miss de escrita

    reagendar_tarefa(t, PERIODO_DISTANCIA, "DISTANCIA");
    t->async_wait(boost::asio::bind_executor(*strand, 
        std::bind(distancia_percorrida, std::placeholders::_1, t, strand, shm, std::ref(sinc))));
}


void reconstrucao_teto(const boost::system::error_code& e, boost::asio::steady_timer* t, 
                       boost::asio::io_context::strand* strand, MemoriaCompartilhada* shm, VarCondSinc &sinc) 
{
    if (e) return;
    if (!shm->c_automatico) return;

    std::vector<float> dados_para_processar;

    // ETAPA 1: Lê tudo da Navegação
    {
        std::lock_guard<std::mutex> lock_nav(sinc.mtx_navegacao);
        while (sinc.disp_naveg_r > 0) {
            dados_para_processar.push_back(sinc.BUFFER_NAVEGACAO[sinc.LER_IDX_NAVEG_R]);
            sinc.LER_IDX_NAVEG_R = (sinc.LER_IDX_NAVEG_R + 1) % ELEMENTOS_BUFFERS;
            sinc.disp_naveg_r--;
        }
    }

    // ETAPA 2: Processa e escreve no Nível
    if (!dados_para_processar.empty()) {
        std::lock_guard<std::mutex> lock_nivel(sinc.mtx_nivel);
        
        for (float dado_lido : dados_para_processar) {
            // --- LÓGICA DE OVERSAMPLING PARA O NÍVEL ---
            // Se o Coletor ficou para trás, joga fora o dado de nível mais antigo
            if (sinc.disp_nivel >= ELEMENTOS_BUFFERS) {
                sinc.LER_IDX_NIVEL = (sinc.LER_IDX_NIVEL + 1) % ELEMENTOS_BUFFERS;
                sinc.disp_nivel--;
            }

            sinc.BUFFER_NIVEL[sinc.ESC_IDX_NIVEL] = dado_lido; // Lógica de LIDAR aqui
            sinc.ESC_IDX_NIVEL = (sinc.ESC_IDX_NIVEL + 1) % ELEMENTOS_BUFFERS;
            sinc.disp_nivel++;
        }
        
        executado[2]++;
    } else {
        miss[2]++;
    }

    reagendar_tarefa(t, PERIODO_RECONSTRUCAO, "RECONSTRUCAO");
    t->async_wait(boost::asio::bind_executor(*strand, 
        std::bind(reconstrucao_teto, std::placeholders::_1, t, strand, shm, std::ref(sinc))));
}


void coletor_dados(const boost::system::error_code& e, boost::asio::steady_timer* t, 
                   boost::asio::io_context::strand* strand, MemoriaCompartilhada* shm, VarCondSinc &sinc) 
{
    if (e) return;
    if (!shm->c_automatico) return;

    bool processou_algo = false;

    {
        std::lock_guard<std::mutex> lock(sinc.mtx_nivel);
        
        while (sinc.disp_nivel > 0) {
            float leitura = sinc.BUFFER_NIVEL[sinc.LER_IDX_NIVEL];
            sinc.LER_IDX_NIVEL = (sinc.LER_IDX_NIVEL + 1) % ELEMENTOS_BUFFERS;
            sinc.disp_nivel--;
            
            // Lógica: Salva 'leitura' no Banco de Dados
            
            processou_algo = true;
        }
    }

    if (processou_algo) {
        executado[3]++;
    } else {
        miss[3]++;
    }

    // Exibição de debug consolidada 
    if ((executado[3] + miss[3]) % 100 == 0) {
        std::cout << "[STATUS] Miss: ";
        for (auto i : miss) std::cout << i << " ";
        std::cout << "| Exec: ";
        for (auto i : executado) std::cout << i << " ";
        std::cout << "\n";
    }

    reagendar_tarefa(t, PERIODO_COLETOR, "COLETOR");
    t->async_wait(boost::asio::bind_executor(*strand, 
        std::bind(coletor_dados, std::placeholders::_1, t, strand, shm, std::ref(sinc))));
}


void inspecao_camera(const boost::system::error_code& e, boost::asio::steady_timer* t, 
                     boost::asio::io_context::strand* strand, std::mutex& mtx, MemoriaCompartilhada* shm) 
{
    if (e) return;
    if (!shm->c_automatico) return;

    {
        std::lock_guard<std::mutex> lock(mtx);
        if (eventos_camera > 0) {
            eventos_camera--;
            
            log_message("CAMERA", "Inspeção iniciada");
            shm->o_liga_camera = false;
            shm->e_inspecao = false;
            log_message("CAMERA", "Inspeção finalizada");
        }
    }

    reagendar_tarefa(t, PERIODO_CAMERA, "CAMERA");
    t->async_wait(boost::asio::bind_executor(*strand, 
        std::bind(inspecao_camera, std::placeholders::_1, t, strand, std::ref(mtx), shm)));
}