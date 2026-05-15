#include "includes.hpp"
#include "shared_memory.hpp"

#define ELEMENTOS_BUFFERS 10
const int SHM_SIZE = 1024; // Size of the shared memory segment

//sensores e atuadores disponíveis no carrinho:
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
 */
void controle_navegacao(std::mutex& mtx, std::vector <float>& BUFFER){

    std::unique_lock<std::mutex> lock (mtx);
    lock.unlock();

}

void distancia_percorrida(std::mutex& mtx, std::vector<float>& BUFFER){

    std::unique_lock<std::mutex> lock (mtx);
    lock.unlock();

}

void reconstrucao_teto(std::mutex& mtx, std::vector<float>& BUFFER){
    
    bool encontrou_falha = true; // teste

    if(encontrou_falha){
        std::lock_guard<std::mutex> lock(mtx);
        e_inspecao = true;
        o_liga_camera = true;
        j_sp_velocidade = 10;
    }

        std::cout << "Falha detectada. Câmera acionada." << std::endl;

        camera.notify_one();
        devagar.notify_one();
}


void inspecao_camera(std::mutex& mtx){

    std::unique_lock<std::mutex> lock(mtx); // Protocolo de entrada
        while(!o_liga_camera){
            camera.wait(lock); // Espera até que haja uma falha para inspecionar
        }

        //inspeciona...

        o_liga_camera = false;
        lock.unlock();
}

void coletor_dados(std::mutex& mtx, std::vector<float>& BUFFER){

    std::unique_lock<std::mutex> lock (mtx);

    lock.unlock();
}

void operacao_remota(std::mutex& mtx, std::vector<float>& BUFFER){
    std::unique_lock<std::mutex> lock(mtx); // Protocolo de entrada
    lock.unlock(); // Protocolo de saída
}

void simulacao(){

}


int main (){

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

        // Desanexa memória no pai
        shmdt(shm);

        // Remove segmento de memória compartilhada
        shmctl(shmid, IPC_RMID, nullptr);

        //INICIALIZAÇÃO DE BUFFERS
        std::vector <float> BUFFER_NAVEGACAO (ELEMENTOS_BUFFERS); //posição do carrinho
        std::vector <float> BUFFER_NIVEL (ELEMENTOS_BUFFERS); //leitura de nivel

        //INICIALIZAÇÃO AS THREADS

        std::mutex mutex_navegacao;
        std::mutex mutex_nivel;

        std::vector <std::thread> threads_navegacao;
        threads_navegacao.emplace_back(distancia_percorrida, std::ref(mutex_navegacao), std::ref(BUFFER_NAVEGACAO));
        threads_navegacao.emplace_back(inspecao_camera, std::ref(mutex_navegacao));
        threads_navegacao.emplace_back(coletor_dados, std::ref(mutex_nivel), std::ref(BUFFER_NIVEL));
        threads_navegacao.emplace_back(reconstrucao_teto, std::ref(mutex_nivel), std::ref(BUFFER_NIVEL));
        
        controle_navegacao(std::ref(mutex_navegacao), std::ref(BUFFER_NAVEGACAO));

        for (auto& t : threads_navegacao) {
            if (t.joinable()) {
                t.join();
            }
}
   }

    return 0;
}