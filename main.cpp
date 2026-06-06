#include "include/includes.hpp"
#include "include/shared_memory.hpp"
#include "include/sincronizacao.hpp"
#include "include/simulacao.hpp"

int main (){

    simulacao();

    // PARÂMETROS SINCRONIZAÇÃO
    boost::asio::io_context io; // Contexto de IO para sincronizar as tarrefas
    boost::asio::io_conetxt::strand strand(io); // Strand para garantir a execução sequencial das tarefas

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

    pid_t pid_interface = fork();

    if (pid_interface == 0) {
        log_message("PROCESSO", "Processo interface Pygame criado");

        execlp("python3.12", "python3.12", "src/interface.py", nullptr);

        perror("Erro ao executar interface.py");
        exit(1);
    }
    else if (pid_interface < 0) {
        perror("Erro ao criar processo da interface");
        exit(1);
    }

    else {


    pid_t pid = fork();

    if (pid == 0){

        // PROCESSO DE COMANDO DE NAVEGAÇÃO

            log_message("PROCESSO","Processo comando_navegacao criado");

            tempo_tarefa t(microssegundos (100));
            t.async_wait(boost::asio::bind_executor(strand, std::bind(comando_navegacao, std::placeholders::_1, &t, &strand)));

            // Exemplo: filho escreve comando remoto
            shm->c_automatico = true;
            shm->j_sp_velocidade = 50;

            std::cout << "Comando de navegação escreveu na memória compartilhada:" << std::endl;
            std::cout << "c_automatico = " << shm->c_automatico << std::endl;
            std::cout << "j_sp_velocidade = " << shm->j_sp_velocidade << std::endl;

            // Desanexa memória no filho
            shmdt(shm);

            io.run();
    }

    else if (pid < 0){
            perror ("Erro ao criar processo\n");
            exit(1);
    }

    else {

        // PROCESSO DE CONTROLE DE NAVEGAÇÃO E THREADS DE TAREFAS

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
        std::mutex mutex_camera;

        std::vector <std::thread> threads_navegacao;

        log_message("MAIN","Criando thread distancia_percorrida");

        threads_navegacao.emplace_back(
            distancia_percorrida,
            std::ref(mutex_navegacao),
            std::ref(BUFFER_NAVEGACAO)
        );

        log_message("MAIN","Criando thread inspecao_camera");

        threads_navegacao.emplace_back(
            inspecao_camera,
            std::ref(mutex_camera),
            shm
        );

        log_message("MAIN","Criando thread coletor_dados");

        threads_navegacao.emplace_back(
            coletor_dados,
            std::ref(mutex_nivel),
            std::ref(BUFFER_NIVEL)
        );

        log_message("MAIN","Criando thread reconstrucao_teto");

        threads_navegacao.emplace_back(
            reconstrucao_teto,
            std::ref(mutex_navegacao),
            std::ref(mutex_nivel),
            std::ref(mutex_camera),
            std::ref(BUFFER_NAVEGACAO),
            std::ref(BUFFER_NIVEL),
            shm
        );

        log_message("MAIN","Criando thread controle_navegacao");

        threads_navegacao.emplace_back(
            controle_navegacao,
            std::ref(mutex_navegacao),
            std::ref(BUFFER_NAVEGACAO)
        );

        for (size_t i = 0; i < threads_navegacao.size(); i++) {
            log_message("MAIN", "Esperando thread " + std::to_string(i));

            if (threads_navegacao[i].joinable()) {
                threads_navegacao[i].join();
            }

            log_message("MAIN", "Thread " + std::to_string(i) + " finalizada");
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

        log_message("MAIN","Execução finalizada");
   }

    // Desanexa memória no pai
    shmdt(shm);

    // Remove segmento de memória compartilhada
    shmctl(shmid, IPC_RMID, nullptr);
    }
    return 0;
    
}