#include "sincronizacao.hpp"

#define ELEMENTOS_BUFFERS 10
const int SHM_SIZE = 1024; // Size of the shared memory segment

//Sincronização de buffers
std::condition_variable leitura_buffer_navegacao;
int dados_navegacao = 0;
std::condition_variable escrita_buffer_navegacao;

std::condition_variable leitura_buffer_nivel;
int dados_nivel = 0;
std::condition_variable escrita_buffer_nivel;

//sensores e atuadores disponíveis no caminhão:
bool i_encoder; //Variável que simula a entrada de um encoder, que troca de estado a cada metro percorrido pelo robô
int i_lidar; //Resposta do sensor LIDAR do veículo exibindo a distância no eixo y, com relação à altura do robô 
bool o_liga_camera; //Comando para ligar a câmera e realizar fotos da falha detectada na superfície
int o_aceleracao; //Determina a aceleração do veículo em percentual (-100 a 100%)

//estados e comandos:
bool e_inspecao; //Estado que indica falha detectada na superfície e o robô está tirando fotos e navegando com velocidade limitada (1:falha, 0: sem falha)
bool e_automatico; //Estado que identifica o modo de operação do robô (0: manual, 1: automático)
bool c_automatico; //Comando para passar o robô para o modo automático (true). O reset desse comando
bool c_man; //Comando para passar o robô para o modo manual (true).
int j_sp_velocidade; //Setpoint de velocidade do robô para o controlador de velocidade.

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

//sincronização
std::condition_variable camera;
std::condition_variable devagar;

//variaveis de condição buffers
std::condition_variable leitura_buffer_navegacao;
int dados_navegacao = 0;
std::condition_variable escrita_buffer_navegacao;

std::condition_variable leitura_buffer_nivel;
int dados_nivel = 0;
std::condition_variable escrita_buffer_nivel;

/**
 * @brief Tarefa que recebe comandos do sistema de operação remoto e traduz os comandos em setpoint de velocidade
 *  e liga/desliga para o controle de navegação. O comando de navegação que deve implementar a lógica de manual/ 
 * automático. Exerce a função de ESCRITOR sobre o BUFFER_NAVEGACAO
 * 
 */
void comando_navegacao(){

}

/**
 * @brief implementação de um controlador PID responsável pelo acionamento dos motores (controle de velocidade)
 *  a partir de um setpoint de velocidade recebido pelo Comando de Navegação. Exerce a função de LEITOR do BUFFER _NAVEGACAO
 * 
 * @param mtx mutex utilizado
 * @param BUFFER historico de posições
 */
void controle_navegacao(std::mutex &mtx, std::vector <float> &BUFFER){

    int idx = -1; // -1 para corrigir o inicio de leitura do vetor

    for (int i = 0; i<20; i++){
        
        idx++;
        idx = idx%ELEMENTOS_BUFFERS;
        
        std::unique_lock<std::mutex> lock (mtx);

        while(dados_navegacao == 0){

            log_message(
                "CONTROLE",
                "TESTE: buffer vazio -> consumidor bloqueado aguardando dados"
            );

            leitura_buffer_navegacao.wait(lock);
        }

        //SEÇÃO CRÍTICA
        log_message(
            "CONTROLE",
            "Posição lida (navegação): " + std::to_string(BUFFER[idx])
        );
        //SEÇÃO CRÍTICA

        // TESTE DE BUFFER CHEIO
        // Consumidor lento para encher o buffer
        std::this_thread::sleep_for(
            std::chrono::seconds(2)
        );

        lock.unlock();

        dados_navegacao--;

        escrita_buffer_navegacao.notify_one();

    }

}

void distancia_percorrida(std::mutex &mtx, std::vector<float> &BUFFER){

    int idx = -1; // -1 para corrigir o inicio de escrita
    
    for (int i = 0; i<20; i++){
        
        idx++;
        idx = idx % ELEMENTOS_BUFFERS;

        // TESTE DE BUFFER VAZIO
        // Produtor lento no início

        std::this_thread::sleep_for(
            std::chrono::seconds(1)
        );

        float escrita = numero_aleatorio_debugg();
    
        std::unique_lock<std::mutex> lock (mtx);
        
        while(dados_navegacao >=10){

            log_message(
                "DISTANCIA",
                "TESTE: buffer cheio -> produtor bloqueado aguardando espaço"
            );

            escrita_buffer_navegacao.wait(lock);
        }

        //SEÇÃO CRÍTICA
        BUFFER[idx] = escrita;

        log_message(
            "DISTANCIA",
            "Posição escrita (navegação): " + std::to_string(escrita)
        );

        log_message(
            "DISTANCIA",
            "Quantidade de dados no buffer: "
            + std::to_string(dados_navegacao + 1)
        );

        //SEÇÃO CRÍTICA

        lock.unlock();

        dados_navegacao++;

        leitura_buffer_navegacao.notify_one();

    }
}

/**
 * @brief Registra os dados coletados pelo lidar num Banco de Dados. Atua como LEITOR do BUFFER_NIVEL.
 * 
 * @param mtx mutex para sincronizar o buffer compartilhado
 * @param BUFFER buffer de dados coletados pelo lidar
 */
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

        while(dados_nivel == 0){

            log_message(
                "COLETOR",
                "Buffer vazio -> aguardando dados"
            );

            leitura_buffer_nivel.wait(lock);
        }
        //SEÇÃO CRÍTICA
        log_message(
            "COLETOR",
            "Posição lida (nível): " + std::to_string(BUFFER[idx])
        );
        //SEÇÃO CRÍTICA

        lock.unlock();
        dados_nivel--;
        escrita_buffer_nivel.notify_one();

        //std::this_thread::sleep_for (std::chrono::microseconds(50));

    }

}
  
/**
 * @brief Recebe os valores do lidar para reconstruir o teto do túnel. Atua como
 * ESCRITOR do BUFFER_NIVEL
 * 
 * @param mtx mutex para buffer compartilhado
 * @param BUFFER vetor de dados do nível da distancia do teto
 */
void reconstrucao_teto(std::mutex &mtx, std::vector <float> &BUFFER, MemoriaCompartilhada* shm){

    int idx = -1; // -1 para corrigir o inicio de escrita
    
    for (int i = 0; i<20; i++){
        
        idx++;
        idx = idx % ELEMENTOS_BUFFERS;

        float escrita = numero_aleatorio_debugg();
    
        std::unique_lock<std::mutex> lock (mtx);
        
        while(dados_nivel >= 10){
            
            log_message(
                "RECONSTRUCAO",
                "Buffer cheio -> produtor aguardando espaço"
            );

            escrita_buffer_nivel.wait(lock);
        }

        //SEÇÃO CRÍTICA
        BUFFER[idx] = escrita;

        log_message(
            "RECONSTRUCAO",
            "Posição escrita (nível): " + std::to_string(escrita)
        );

        //SEÇÃO CRÍTICA

        lock.unlock();

        dados_nivel++;

        leitura_buffer_nivel.notify_one();

    }

    bool encontrou_falha = true; // teste

    if(encontrou_falha){
        std::lock_guard<std::mutex> lock(mtx);
        shm->e_inspecao = true;
        shm->o_liga_camera = true;
        shm->j_sp_velocidade = 10;
    }

        std::cout << "Falha detectada. Câmera acionada." << std::endl;

        camera.notify_one();
        devagar.notify_one();
        
}


void inspecao_camera(std::mutex& mtx, MemoriaCompartilhada* shm){

    std::unique_lock<std::mutex> lock(mtx); // Protocolo de entrada
        while(!shm->o_liga_camera){
            camera.wait(lock); // Espera até que haja uma falha para inspecionar
        }

        //inspeciona...

        shm->o_liga_camera = false;
        shm->e_inspecao = false;
        lock.unlock();
}

void simulacao(){

    log_message(
        "SIMULACAO",
        "Thread inicializada"
    );

}