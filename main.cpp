#include "include/includes.hpp"
#include "include/shared_memory.hpp"
#include "src/simulacao.cpp"

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

        log_message("MAIN","Buffers inicializados");

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