#include "../include/sincronizacao.hpp"

const int SHM_SIZE = 1024; // Size of the shared memory segment

// Sincronização e Condições
std::condition_variable camera;
int eventos_camera = 0;

// Variaveis de condição buffers (Mantidas para controle de fluxo rápido)
std::condition_variable leitura_buffer_navegacao;
std::condition_variable escrita_buffer_navegacao;

int AR_NAVEGACAO = 0;
int WR_NAVEGACAO = 0;
int AW_NAVEGACAO = 0;
int WW_NAVEGACAO = 0;

std::condition_variable leitura_buffer_nivel; 
std::condition_variable escrita_buffer_nivel;
int dados_nivel = 0;

// Funções auxiliares de debbug
std::mutex mutex_log;

void log_message(const std::string& thread, const std::string& mensagem){
    std::lock_guard<std::mutex> lock(mutex_log);
    auto agora = std::chrono::system_clock::now();
    auto tempo = std::chrono::system_clock::to_time_t(agora);
    std::cout << "[" << std::put_time(std::localtime(&tempo), "%H:%M:%S") << "] "
              << "[" << thread << "] " << mensagem << std::endl;
}

float numero_aleatorio_debugg(){
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(1.0f, 100.0f);
    return dis(gen);
}

// =========================================================================
// TAREFAS ASSÍNCRONAS DO SISTEMA
// =========================================================================

void comando_navegacao(const boost::system::error_code& e,
                       boost::asio::steady_timer* t, 
                       boost::asio::io_context::strand* strand,
                       MemoriaCompartilhada* shm)
{
    if (e) return;

    // CONDIÇÃO DE PARADA: Se o comando for desativado, não reagenda.
    if (shm->c_automatico == false) {
        log_message("COMANDO", "Modo automático desativado. Encerrando tarefa.");
        return;
    }

    //std::cout << "Processo comando navegação executando...\n";
    
    // Atualiza tempo e reagenda
    t->expires_at(t->expiry() + boost::asio::chrono::microseconds(PERIODO_COMANDO));
    t->async_wait(boost::asio::bind_executor(*strand, 
        std::bind(comando_navegacao, std::placeholders::_1, t, strand, shm)));

}

void controle_navegacao(const boost::system::error_code& e,
                        boost::asio::steady_timer* t, 
                        boost::asio::io_context::strand* strand,
                        std::mutex &mtx, 
                        std::vector<float> &BUFFER,
                        MemoriaCompartilhada* shm)
{
    if (e) return;
    if (shm->c_automatico == false) return; // PARADA GLOBAL

    int idx = -1; // Static preserva o valor entre as chamadas da função
    idx = (idx + 1) % ELEMENTOS_BUFFERS;

    std::unique_lock<std::mutex> lock(mtx);

    while((AW_NAVEGACAO + WW_NAVEGACAO) > 0){
        log_message("CONTROLE", "Leitor aguardando acesso ao buffer navegacao");
        WR_NAVEGACAO++;
        leitura_buffer_navegacao.wait(lock);
        WR_NAVEGACAO--;
    }

    AR_NAVEGACAO++;
    lock.unlock();

    // SEÇÃO CRÍTICA
    float leitura = BUFFER[idx];
    // SEÇÃO CRÍTICA

    lock.lock();
    AR_NAVEGACAO--;
    if(AR_NAVEGACAO == 0 && WW_NAVEGACAO > 0){
        escrita_buffer_navegacao.notify_one();
    }
    lock.unlock();

    log_message("CONTROLE", "Posição lida (navegação): " + std::to_string(leitura));

    t->expires_at(t->expiry() + boost::asio::chrono::microseconds(PERIODO_CONTROLE));
    t->async_wait(boost::asio::bind_executor(*strand, 
        std::bind(controle_navegacao, std::placeholders::_1, t, strand, std::ref(mtx), std::ref(BUFFER), shm)));
}

void distancia_percorrida(const boost::system::error_code& e,
                          boost::asio::steady_timer* t, 
                          boost::asio::io_context::strand* strand, 
                          std::mutex &mtx, 
                          std::vector<float> &BUFFER,
                          MemoriaCompartilhada* shm)
{
    if (e) return;
    if (shm->c_automatico == false) return; // PARADA GLOBAL

    static int idx = -1; 
    idx = (idx + 1) % ELEMENTOS_BUFFERS;
    float escrita = numero_aleatorio_debugg();

    std::unique_lock<std::mutex> lock(mtx);
    
    // META-LOCKING
    while((AW_NAVEGACAO + AR_NAVEGACAO) > 0){
        log_message("DISTANCIA", "Escritor aguardando acesso ao buffer navegacao");
        WW_NAVEGACAO++;
        escrita_buffer_navegacao.wait(lock);
        WW_NAVEGACAO--;
    }

    AW_NAVEGACAO++;
    lock.unlock();

    // SEÇÃO CRÍTICA
    BUFFER[idx] = escrita;
    // SEÇÃO CRÍTICA

    lock.lock();
    AW_NAVEGACAO--;

    if(WW_NAVEGACAO > 0) escrita_buffer_navegacao.notify_one();
    else if(WR_NAVEGACAO > 0) leitura_buffer_navegacao.notify_all();
    
    lock.unlock();

    log_message("DISTANCIA", "Posição escrita (navegação): " + std::to_string(escrita));

    t->expires_at(t->expiry() + boost::asio::chrono::microseconds(PERIODO_DISTANCIA));
    t->async_wait(boost::asio::bind_executor(*strand, 
        std::bind(distancia_percorrida, std::placeholders::_1, t, strand, std::ref(mtx), std::ref(BUFFER), shm)));
}

void reconstrucao_teto(const boost::system::error_code& e,
                       boost::asio::steady_timer* t, 
                       boost::asio::io_context::strand* strand,
                       std::mutex &mtx_navegacao, std::mutex &mtx_nivel, std::mutex &mtx_camera, 
                       std::vector<float> &BUFFER_NAVEGACAO, std::vector<float> &BUFFER_NIVEL, 
                       MemoriaCompartilhada* shm)
{
    if (e) return;
    if (shm->c_automatico == false) return; // PARADA GLOBAL

    static int idx_navegacao = -1;
    static int idx_nivel = -1;
    
    idx_navegacao = (idx_navegacao + 1) % ELEMENTOS_BUFFERS;

    std::unique_lock<std::mutex> lock_navegacao(mtx_navegacao);
    while((AW_NAVEGACAO + WW_NAVEGACAO) > 0){
        WR_NAVEGACAO++;
        leitura_buffer_navegacao.wait(lock_navegacao);
        WR_NAVEGACAO--;
    }
    AR_NAVEGACAO++;
    lock_navegacao.unlock();

    float leitura_navegacao = BUFFER_NAVEGACAO[idx_navegacao];

    lock_navegacao.lock();
    AR_NAVEGACAO--;
    if(AR_NAVEGACAO == 0 && WW_NAVEGACAO > 0) escrita_buffer_navegacao.notify_one();
    lock_navegacao.unlock();

    idx_nivel = (idx_nivel + 1) % ELEMENTOS_BUFFERS;
    float escrita = leitura_navegacao + numero_aleatorio_debugg();

    std::unique_lock<std::mutex> lock_nivel(mtx_nivel);
    while(dados_nivel >= 10){
        escrita_buffer_nivel.wait(lock_nivel);
    } 

    BUFFER_NIVEL[idx_nivel] = escrita;
    dados_nivel++;
    log_message("RECONSTRUCAO", "Ocupação Nível: " + std::to_string(dados_nivel) + "/" + std::to_string(ELEMENTOS_BUFFERS));

    leitura_buffer_nivel.notify_one();
    lock_nivel.unlock();

    // Simulação de falha (ajustada para ocorrer aleatoriamente, já que não temos o loop 'i')
    bool encontrou_falha = false; 

    if(encontrou_falha){
        std::lock_guard<std::mutex> lock_camera(mtx_camera);
        shm->e_inspecao = true;
        shm->o_liga_camera = true;
        shm->j_sp_velocidade = 10;
        eventos_camera++;

        log_message("RECONSTRUCAO", "Falha detectada -> câmera acionada e velocidade reduzida");
    }

    t->expires_at(t->expiry() + boost::asio::chrono::microseconds(PERIODO_RECONSTRUCAO));
    t->async_wait(boost::asio::bind_executor(*strand, 
        std::bind(reconstrucao_teto, std::placeholders::_1, t, strand, 
                  std::ref(mtx_navegacao), std::ref(mtx_nivel), std::ref(mtx_camera), 
                  std::ref(BUFFER_NAVEGACAO), std::ref(BUFFER_NIVEL), shm)));
}

void coletor_dados(const boost::system::error_code& e,
                   boost::asio::steady_timer* t, 
                   boost::asio::io_context::strand* strand,
                   std::mutex &mtx, std::vector<float> &BUFFER,
                   MemoriaCompartilhada* shm)
{
    if (e) return;
    if (shm->c_automatico == false) return; // PARADA GLOBAL

    static int idx = -1;
    idx = (idx + 1) % ELEMENTOS_BUFFERS;

    std::unique_lock<std::mutex> lock(mtx);

    while(dados_nivel == 0){
        leitura_buffer_nivel.wait(lock);
    }

    log_message("COLETOR", "Posição lida (nível): " + std::to_string(BUFFER[idx]));
    dados_nivel--;
    escrita_buffer_nivel.notify_one();
    
    lock.unlock();

    t->expires_at(t->expiry() + boost::asio::chrono::microseconds(PERIODO_COLETOR));
    t->async_wait(boost::asio::bind_executor(*strand, 
        std::bind(coletor_dados, std::placeholders::_1, t, strand, std::ref(mtx), std::ref(BUFFER), shm)));
}

void inspecao_camera(const boost::system::error_code& e,
                     boost::asio::steady_timer* t, 
                     boost::asio::io_context::strand* strand,
                     std::mutex& mtx, MemoriaCompartilhada* shm)
{
    if (e) return;
    if (shm->c_automatico == false) return; // PARADA GLOBAL

    std::unique_lock<std::mutex> lock(mtx);

    // Substituição da condition_variable: 
    // Em vez de bloquear a thread do Asio esperando a falha, verificamos rapidamente.
    // Se não houver eventos, saímos e o timer tenta de novo depois.
    if (eventos_camera > 0) {
        eventos_camera--;
        
        log_message("CAMERA", "Inspeção iniciada");
        // ... tempo de inspeção ...
        shm->o_liga_camera = false;
        shm->e_inspecao = false;
        log_message("CAMERA", "Inspeção finalizada");
    }

    lock.unlock(); 

    t->expires_at(t->expiry() + boost::asio::chrono::microseconds(PERIODO_CAMERA));
    t->async_wait(boost::asio::bind_executor(*strand, 
        std::bind(inspecao_camera, std::placeholders::_1, t, strand, std::ref(mtx), shm)));
}