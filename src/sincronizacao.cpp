#include "../include/sincronizacao.hpp"

#define ELEMENTOS_BUFFERS 10
const int SHM_SIZE = 1024; // Size of the shared memory segment

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

int AR_NIVEL = 0;
int WR_NIVEL = 0;
int AW_NIVEL = 0;
int WW_NIVEL = 0;

//Funções auxiliares de debbug
std::mutex mutex_log;

void log_message(const std::string& thread,
                 const std::string& mensagem){

    std::lock_guard<std::mutex> lock(mutex_log);

    auto agora = std::chrono::system_clock::now();

    auto tempo = std::chrono::system_clock::to_time_t(agora);

    std::cout
        << "[" << std::put_time(std::localtime(&tempo), "%H:%M:%S") << "] "
        << "[" << thread << "] "
        << mensagem
        << std::endl;
}

float numero_aleatorio_debugg(){

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(1.0f, 100.0f);

    return dis(gen);
}

//FUNÇÕES DO SISTEMA 

void comando_navegacao(){

}

void controle_navegacao(std::mutex &mtx, std::vector <float> &BUFFER){

    int idx = -1; // -1 para corrigir o inicio de leitura do vetor

    for (int i = 0; i<20; i++){
        
        idx++;
        idx = idx%ELEMENTOS_BUFFERS;
        
        std::unique_lock<std::mutex> lock (mtx);

        while((AW_NAVEGACAO + WW_NAVEGACAO) > 0){

            WR_NAVEGACAO++;

            leitura_buffer_navegacao.wait(lock);

            WR_NAVEGACAO--;
        }

        AR_NAVEGACAO++;

        lock.unlock();

        //SEÇÃO CRÍTICA
        float leitura = BUFFER[idx];
        //SEÇÃO CRÍTICA

        lock.lock();

        AR_NAVEGACAO--;

        if(AR_NAVEGACAO == 0 && WW_NAVEGACAO > 0){
            escrita_buffer_navegacao.notify_one();
        }

        lock.unlock();

        log_message(
            "CONTROLE",
            "Posição lida (navegação): " + std::to_string(leitura)
        );

        // TESTE DE BUFFER CHEIO
        // Consumidor lento para encher o buffer
        std::this_thread::sleep_for(
            std::chrono::seconds(2)
        );

    }

}

void distancia_percorrida(std::mutex &mtx, std::vector<float> &BUFFER){

    int idx = -1; // -1 para corrigir o inicio de escrita
    
    for (int i = 0; i<20; i++){
        
        idx++;
        idx = idx % ELEMENTOS_BUFFERS;
        float escrita = numero_aleatorio_debugg();

        // Produtor lento no início
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::unique_lock<std::mutex> lock (mtx);
        
        //META-LOCKING
        while((AW_NAVEGACAO + AR_NAVEGACAO) > 0){

            log_message(
                "DISTANCIA",
                "Escritor aguardando acesso ao buffer navegacao"
            );

            WW_NAVEGACAO++;

            escrita_buffer_navegacao.wait(lock);

            WW_NAVEGACAO--;

            log_message(
                "DISTANCIA",
                "Escritor retomou execução"
            );
        }

        AW_NAVEGACAO++;

        lock.unlock();

        //SEÇÃO CRÍTICA

        BUFFER[idx] = escrita;

        //SEÇÃO CRÍTICA

        lock.lock();

        AW_NAVEGACAO--;

        if(WW_NAVEGACAO > 0){
            escrita_buffer_navegacao.notify_one();
        }

        else if(WR_NAVEGACAO > 0){
            leitura_buffer_navegacao.notify_all();
        }
        
        lock.unlock();

        log_message(
            "DISTANCIA",
            "Posição escrita (navegação): " + std::to_string(escrita)
        );
    }
}

void reconstrucao_teto(std::mutex &mtx_navegacao, std::mutex &mtx_nivel, std::mutex &mtx_camera, std::vector <float> &BUFFER_NAVEGACAO, std::vector <float> &BUFFER_NIVEL, MemoriaCompartilhada* shm){

    log_message("RECONSTRUCAO", "Thread inicializada");

    int idx_navegacao = -1;
    int idx_nivel = -1;
    
    for (int i = 0; i<20; i++){ //No final será trocado para while true
        
        idx_navegacao++;
        idx_navegacao = idx_navegacao % ELEMENTOS_BUFFERS;

        std::unique_lock<std::mutex> lock_navegacao(mtx_navegacao);

        while((AW_NAVEGACAO + WW_NAVEGACAO) > 0){

            WR_NAVEGACAO++;

            leitura_buffer_navegacao.wait(lock_navegacao);

            WR_NAVEGACAO--;
        }

        AR_NAVEGACAO++;

        lock_navegacao.unlock();

        //SEÇÃO CRÍTICA NAVEGACAO

        float leitura_navegacao = BUFFER_NAVEGACAO[idx_navegacao];

        //SEÇÃO CRÍTICA NAVEGACAO

        lock_navegacao.lock();

        AR_NAVEGACAO--;

        if(AR_NAVEGACAO == 0 && WW_NAVEGACAO > 0){
            escrita_buffer_navegacao.notify_one();
        }

        lock_navegacao.unlock();

        idx_nivel++;
        idx_nivel = idx_nivel % ELEMENTOS_BUFFERS;

        float escrita = leitura_navegacao + numero_aleatorio_debugg();

        std::unique_lock<std::mutex> lock_nivel (mtx_nivel);
        
        while((AW_NIVEL + AR_NIVEL) > 0){

            log_message("RECONSTRUCAO", "Escritor aguardando acesso ao buffer nivel");

            WW_NIVEL++;

            escrita_buffer_nivel.wait(lock_nivel);

            WW_NIVEL--;

            log_message("RECONSTRUCAO","Escritor retomou execução");
        }

        AW_NIVEL++;

        lock_nivel.unlock();

        //SEÇÃO CRÍTICA NIVEL

        BUFFER_NIVEL[idx_nivel] = escrita;

        //SEÇÃO CRÍTICA NIVEL

        lock_nivel.lock();

        AW_NIVEL--;

        if (WW_NIVEL > 0){
            escrita_buffer_nivel.notify_one();
        }

        else if (WR_NIVEL > 0){
            leitura_buffer_nivel.notify_all();
        }

        lock_nivel.unlock();

        log_message("RECONSTRUCAO", "Posição escrita (nível): " + std::to_string(escrita));

        bool encontrou_falha = true; // teste

        if(encontrou_falha){

            std::lock_guard<std::mutex> lock_camera(mtx_camera);

            shm->e_inspecao = true;
            shm->o_liga_camera = true;
            shm->j_sp_velocidade = 10;

            eventos_camera++;

            log_message(
                "RECONSTRUCAO",
                "Falha detectada -> câmera acionada e velocidade reduzida"
            );

            camera.notify_one();
        }
    }

        std::lock_guard<std::mutex> lock_camera(mtx_camera);
        finalizar_camera = true;

    camera.notify_one();

    log_message("RECONSTRUCAO", "Thread finalizada");
}

void coletor_dados(std::mutex &mtx, std::vector <float> &BUFFER){

    log_message(
        "COLETOR",
        "Thread inicializada"
    );

    int idx = -1; // -1 para corrigir o inicio de leitura do vetor

    for (int i = 0; i<20; i++){
        
        idx++;
        idx = idx%ELEMENTOS_BUFFERS;
        
        std::unique_lock<std::mutex> lock (mtx);

        while((AW_NIVEL + WW_NIVEL) > 0){

            WR_NIVEL++;

            leitura_buffer_nivel.wait(lock);

            WR_NIVEL--;
        }

        AR_NIVEL++;

        lock.unlock();

        //SEÇÃO CRÍTICA

        float leitura = BUFFER[idx];

        //SEÇÃO CRÍTICA

        lock.lock();

        AR_NIVEL--;

        if(AR_NIVEL == 0 && WW_NIVEL > 0){
            escrita_buffer_nivel.notify_one();
        }

        lock.unlock();

        log_message(
            "COLETOR",
            "Posição lida (nível): " + std::to_string(leitura)
        );

        //std::this_thread::sleep_for (std::chrono::microseconds(50));

    }

}

void inspecao_camera(std::mutex& mtx, MemoriaCompartilhada* shm){

    std::unique_lock<std::mutex> lock(mtx); // Protocolo de entrada

    while(true){

        while(!(eventos_camera > 0 || finalizar_camera)){
            camera.wait(lock); // Espera até que haja uma falha para inspecionar
        }

        if (eventos_camera == 0 && finalizar_camera) {
            break;
        }

         eventos_camera--;

        //inspeciona...

        shm->o_liga_camera = false;
        shm->e_inspecao = false;
        
    }
}
