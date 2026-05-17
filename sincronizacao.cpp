#include "sincronizacao.hpp"

#define ELEMENTOS_BUFFERS 10
const int SHM_SIZE = 1024; // Size of the shared memory segment

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

//sincronização
std::condition_variable camera;
std::condition_variable devagar;

//Variaveis de condição buffers
std::condition_variable leitura_buffer_navegacao;
std::condition_variable escrita_buffer_navegacao;
int AR = 0; // active readers
int WR = 0; // waiting readers
int AW = 0; // active writers
int WW = 0; // waiting writers

std::condition_variable leitura_buffer_nivel; 
std::condition_variable escrita_buffer_nivel;
int dados_nivel = 0;

//Funções auxiliares de debbug
std::mutex mutex_log;

/**
 * @brief  Imprime a mensagem na tela
 * 
 * @param thread Nome da thread sendo executada
 * @param mensagem Mensagem a ser exibida
 */
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

/**
 * @brief Gera um numero aleatorio para simular o preenchimento dos buffers
 * 
 * @return float 
 */
float numero_aleatorio_debugg(){

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(1.0f, 100.0f);

    return dis(gen);
}

//FUNÇÕES DO SISTEMA 

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

            log_message(
                "CONTROLE",
                "Consumidor retomou execução"
            );
        }

        //SEÇÃO CRÍTICA
        float leitura = BUFFER[idx];
        dados_navegacao--;
        int quantidade = dados_navegacao;
        //SEÇÃO CRÍTICA

        lock.unlock();

        log_message(
            "CONTROLE",
            "Posição lida (navegação): " + std::to_string(leitura)
        );

        log_message(
            "CONTROLE",
            "Quantidade de dados no buffer: " + std::to_string(quantidade)
        );

        // TESTE DE BUFFER CHEIO
        // Consumidor lento para encher o buffer
        std::this_thread::sleep_for(
            std::chrono::seconds(2)
        );

        escrita_buffer_navegacao.notify_one();

        log_message(
            "CONTROLE",
            "notify_one enviado para produtor"
        );

    }

}

/**
 * @brief Calcula a distância que foi percorrida pelo carrinho
 * 
 * @param mtx mutex utilizado
 * @param BUFFER vetor de daods compartilhado
 */
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
        while((AW+WW) > 0){

            log_message(
                "DISTANCIA",
                "TESTE: buffer cheio -> produtor bloqueado aguardando espaço"
            );

            WR++;
            escrita_buffer_navegacao.wait(lock);
            WR--;

            log_message(
                "DISTANCIA",
                "Produtor retomou execução"
            );
        }
        AR++;

        lock.unlock();

        //SEÇÃO CRÍTICA

        BUFFER[idx] = escrita;

        //SEÇÃO CRÍTICA

        lock.lock();

        AR--;

        dados_navegacao++;
        int quantidade = dados_navegacao;

        if(AR == 0 && WW > 0){
            escrita_buffer_navegacao.notify_one();
        }
        
        lock.unlock();

        log_message(
            "DISTANCIA",
            "Posição escrita (navegação): " + std::to_string(escrita)
        );

        log_message(
            "DISTANCIA",
            "Quantidade de dados no buffer: "
            + std::to_string(quantidade)
        );

        leitura_buffer_navegacao.notify_one();

        log_message(
            "DISTANCIA",
            "notify_one enviado para consumidor"
        );
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

    log_message(
        "RECONSTRUCAO",
        "Thread inicializada"
    );

    int idx = -1; // -1 para corrigir o inicio de escrita
    
    for (int i = 0; i<20; i++){ //No final será trocado para while true
        
        idx++;
        idx = idx % ELEMENTOS_BUFFERS;
        float escrita = numero_aleatorio_debugg();
    
        //META-LOCKING
        std::unique_lock<std::mutex> lock (mtx);
        
        while( (AW + AR) > 0){

            log_message(
                "RECONSTRUCAO",
                "Buffer cheio -> produtor aguardando espaço"
            );

            WW++;
            escrita_buffer_navegacao.wait(lock);
            WW--;

            log_message(
                "RECONSTRUCAO",
                "Produtor retomou execução"
            );
        }

        AW++;
        lock.unlock();

        //SEÇÃO CRÍTICA

        BUFFER[idx] = escrita;

        //SEÇÃO CRÍTICA

        lock.lock();
        AW--;
        dados_nivel++;

        if (WW > 0)
            escrita_buffer_navegacao.notify_one();
        else if (WR > 0){
            leitura_buffer_navegacao.notify_all();
        }
        lock.unlock();

        log_message(
            "RECONSTRUCAO",
            "Posição escrita (nível): " + std::to_string(escrita)
        );

        leitura_buffer_nivel.notify_one();

        log_message(
            "RECONSTRUCAO",
            "notify_one enviado para consumidor"
        );
    }

    bool encontrou_falha = true; // teste

    if(encontrou_falha){
        std::lock_guard<std::mutex> lock(mtx);
        shm->e_inspecao = true;
        shm->o_liga_camera = true;
        shm->j_sp_velocidade = 10;

        log_message(
            "RECONSTRUCAO",
            "Falha detectada -> câmera acionada e velocidade reduzida"
        );
    }

        camera.notify_one();
        devagar.notify_one();
        
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

            log_message(
                "COLETOR",
                "Consumidor retomou execução"
            );
        }
        //SEÇÃO CRÍTICA
        float leitura = BUFFER[idx];

        dados_nivel--;
        int quantidade = dados_nivel;
        //SEÇÃO CRÍTICA

        lock.unlock();

        log_message(
            "COLETOR",
            "Posição lida (nível): " + std::to_string(leitura)
        );

        log_message(
            "COLETOR",
            "Quantidade de dados no buffer: "
            + std::to_string(quantidade)
        );
        
        escrita_buffer_nivel.notify_one();

        log_message(
            "COLETOR",
            "notify_one enviado para produtor"
        );

        //std::this_thread::sleep_for (std::chrono::microseconds(50));

    }

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