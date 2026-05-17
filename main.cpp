#include "include/includes.hpp"
#include "include/shared_memory.hpp"
#include "src/simulacao.cpp"

#define ELEMENTOS_BUFFERS 10
const int SHM_SIZE = 1024; // Size of the shared memory segment

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

int main (){

    simulacao();

    // Generate a unique key for the shared memory segment 
    key_t key = IPC_PRIVATE; // Use IPC_PRIVATE for a unique key 
    
    // Create a shared memory segment 
    int shmid = shmget(key, sizeof(MemoriaCompartilhada), 0666 | IPC_CREAT);

    if (shmid < 0) {
        perror("Erro ao criar memória compartilhada");
        return 1;
    }

    // Attach to the shared memory
    MemoriaCompartilhada* shm = (MemoriaCompartilhada*) shmat(shmid, nullptr, 0);

    if (shm == (void*) -1) {
        perror("Erro ao anexar memória compartilhada");
        return 1;
    }

    // Inicializa os valores da memória compartilhada
    shm->i_encoder = false;
    shm->i_lidar = 0;

    shm->o_liga_camera = false;
    shm->o_aceleracao = 0;

    shm->e_inspecao = false;
    shm->e_automatico = false;

    shm->c_automatico = false;
    shm->c_man = false;
    shm->j_sp_velocidade = 0;

    //INICIALIZAÇÃO PROCESSOS

    pid_t pid;

    pid = fork();

   if (pid == 0){

        comando_navegacao ();

        // Exemplo: filho escreve comando remoto
        shm->c_automatico = true;
        shm->j_sp_velocidade = 50;

        std::cout << "Comando de navegação escreveu na memória compartilhada:" << std::endl;
        std::cout << "c_automatico = " << shm->c_automatico << std::endl;
        std::cout << "j_sp_velocidade = " << shm->j_sp_velocidade << std::endl;

        // Desanexa memória no filho
        shmdt(shm);
   }

   else if (pid < 0){
        perror ("Erro ao criar processo\n");
        exit(1);
   }

   else {

        wait(nullptr);

        std::cout << "Controle de navegação lendo memória compartilhada:" << std::endl;
        std::cout << "c_automatico = " << shm->c_automatico << std::endl;
        std::cout << "j_sp_velocidade = " << shm->j_sp_velocidade << std::endl;

        //INICIALIZAÇÃO DE BUFFERS
        std::vector <float> BUFFER_NAVEGACAO (ELEMENTOS_BUFFERS); //posição do carrinho
        std::vector <float> BUFFER_NIVEL (ELEMENTOS_BUFFERS); //leitura de nivel

        log_message(
            "MAIN",
            "Buffers inicializados"
        );

        //INICIALIZAÇÃO AS THREADS

        std::mutex mutex_navegacao;
        std::mutex mutex_nivel;

        std::vector <std::thread> threads_navegacao;
        threads_navegacao.emplace_back(distancia_percorrida, std::ref(mutex_navegacao), std::ref(BUFFER_NAVEGACAO));
        threads_navegacao.emplace_back(inspecao_camera, std::ref(mutex_navegacao), shm);
        threads_navegacao.emplace_back(coletor_dados, std::ref(mutex_nivel), std::ref(BUFFER_NIVEL));
        threads_navegacao.emplace_back(reconstrucao_teto, std::ref(mutex_nivel), std::ref(BUFFER_NIVEL), shm);
        
        controle_navegacao(std::ref(mutex_navegacao), std::ref(BUFFER_NAVEGACAO));

        for (int i = 0; i < threads_navegacao.size(); i++){
            threads_navegacao[i].detach();
        }
        
        std::cout << "Buffer navegação: ";

        for (auto i : BUFFER_NAVEGACAO){
            std::cout << i << " ";
        }

        std::cout << std::endl;

        std::cout << "Buffer nível: ";

        for (auto i : BUFFER_NIVEL){
            std::cout << i << " ";
        }

        std::cout << std::endl;
   }

    // Desanexa memória no pai
    shmdt(shm);

    // Remove segmento de memória compartilhada
    shmctl(shmid, IPC_RMID, nullptr);


    return 0;
}