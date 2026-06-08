#include "include/includes.hpp"
#include "include/shared_memory.hpp"
#include "include/sincronizacao.hpp"
#include "include/simulacao.hpp"

int main() {
    
    simulacao();

    // =========================================================================
    // 1. DIRETIVAS DE COMUNICAÇÃO (MEMÓRIA COMPARTILHADA / IPC)
    // =========================================================================
    key_t key = IPC_PRIVATE; 
    int shmid = shmget(key, sizeof(MemoriaCompartilhada), 0666 | IPC_CREAT);
    
    if (shmid < 0) {
        perror("Erro ao criar memória compartilhada");
        return 1;
    }

    MemoriaCompartilhada* shm = (MemoriaCompartilhada*) shmat(shmid, nullptr, 0);
    if (shm == (void*) -1) {
        perror("Erro ao anexar memória compartilhada");
        return 1;
    }

    // Inicialização da Estrutura de Dados
    shm->i_encoder      = false;
    shm->i_lidar        = 0;
    shm->o_liga_camera  = false;
    shm->o_aceleracao   = 0;
    shm->e_inspecao     = false;
    shm->e_automatico   = false;
    shm->c_automatico   = false;
    shm->c_man          = false;
    shm->j_sp_velocidade= 0;

    // =========================================================================
    // 2. CRIAÇÃO DE PROCESSOS (FORK)
    // =========================================================================
    
    /*
    // --- PROCESSO 1: INTERFACE PYGAME ---
    pid_t pid_interface = fork();
    if (pid_interface == 0) {
        log_message("PROCESSO", "Processo interface Pygame criado");
        execlp("python3.12", "python3.12", "src/interface.py", nullptr);
        perror("Erro ao executar interface.py");
        exit(1);
    } else if (pid_interface < 0) {
        perror("Erro ao criar processo da interface");
        exit(1);
    }
    */

    // --- PROCESSO 2: COMANDO (FILHO) E CONTROLE (PAI) ---
    pid_t pid_controle = fork();

    // Diretivas de Sincronização Isoladas para o Filho
    boost::asio::io_context io_geral;
    boost::asio::io_context::strand strand_geral(io_geral);

    if (pid_controle == 0) {
        // ---------------------------------------------------------------------
        // BLOCO DO FILHO: COMANDO DE NAVEGAÇÃO
        // ---------------------------------------------------------------------
        log_message("PROCESSO", "Processo comando_navegacao criado");

        tempo_tarefa t_comando(io_geral, microssegundos(500));
        t_comando.async_wait(boost::asio::bind_executor(strand_geral, 
            std::bind(comando_navegacao, std::placeholders::_1, &t_comando, &strand_geral, shm)));

        shm->c_automatico = true;
        shm->j_sp_velocidade = 50;

        // Inicia o processamento da fila de tarefas do filho
        //io_geral.run(); 

        // Limpeza do filho (só chega aqui se o loop do io.run() for interrompido)
        shmdt(shm);
        exit(0); 
    } 
    else if (pid_controle < 0) {
        perror("Erro ao criar processo de comando\n");
        exit(1);
    } 
    else {
        // ---------------------------------------------------------------------
        // BLOCO DO PAI: CONTROLE DE NAVEGAÇÃO E SENSORIAMENTO
        // ---------------------------------------------------------------------
        log_message("PROCESSO", "Processo de controle de navegação iniciado");

        // Inicialização de Buffers e Mutexes
        std::vector<float> BUFFER_NAVEGACAO(ELEMENTOS_BUFFERS);
        std::vector<float> BUFFER_NIVEL(ELEMENTOS_BUFFERS);
        
        std::mutex mutex_navegacao;
        std::mutex mutex_nivel;
        std::mutex mutex_camera;

        // --- DEFINIÇÃO DOS TEMPORIZADORES (TIMERS) ---
        tempo_tarefa t1_dist(io_geral, microssegundos(PERIODO_DISTANCIA));
        tempo_tarefa t2_cam(io_geral, microssegundos(PERIODO_CAMERA));
        tempo_tarefa t3_col(io_geral, microssegundos(PERIODO_COLETOR));
        tempo_tarefa t4_rec(io_geral, microssegundos(PERIODO_RECONSTRUCAO));
        tempo_tarefa t5_con(io_geral, microssegundos(PERIODO_CONTROLE));

        // --- AGENDAMENTO DAS TAREFAS (ASYNC_WAIT) ---
        log_message("MAIN", "Agendando tarefas assíncronas...");

        t1_dist.async_wait(boost::asio::bind_executor(strand_geral, 
            std::bind(distancia_percorrida, std::placeholders::_1, &t1_dist, &strand_geral, 
                      std::ref(mutex_navegacao), std::ref(BUFFER_NAVEGACAO), shm)));

        t2_cam.async_wait(boost::asio::bind_executor(strand_geral, 
            std::bind(inspecao_camera, std::placeholders::_1, &t2_cam, &strand_geral, 
                      std::ref(mutex_camera), shm)));

        t3_col.async_wait(boost::asio::bind_executor(strand_geral, 
            std::bind(coletor_dados, std::placeholders::_1, &t3_col, &strand_geral, 
                      std::ref(mutex_nivel), std::ref(BUFFER_NIVEL), shm)));

        t4_rec.async_wait(boost::asio::bind_executor(strand_geral, 
            std::bind(reconstrucao_teto, std::placeholders::_1, &t4_rec, &strand_geral, 
                      std::ref(mutex_navegacao), std::ref(mutex_nivel), std::ref(mutex_camera), 
                      std::ref(BUFFER_NAVEGACAO), std::ref(BUFFER_NIVEL), shm)));

        t5_con.async_wait(boost::asio::bind_executor(strand_geral, 
            std::bind(controle_navegacao, std::placeholders::_1, &t5_con, &strand_geral, 
                      std::ref(mutex_navegacao), std::ref(BUFFER_NAVEGACAO), shm)));

        // --- EXECUÇÃO (THREAD POOL) ---
        // Cria um grupo de operários para processar o io_context em paralelo
        std::vector<std::thread> thread_pool;
        int num_threads = 4; // Ajuste conforme os núcleos disponíveis no seu sistema
        
        log_message("MAIN", "Iniciando Thread Pool do Boost.Asio");
        for (int i = 0; i < num_threads; ++i) {
            thread_pool.emplace_back([&io_geral]() {
                io_geral.run();
            });
        }

        //Simular desligamento
        std::this_thread::sleep_for(std::chrono::seconds(1));
        shm->c_automatico = false;

        // --- SINCRONIZAÇÃO FINAL (Aguardar encerramento) ---
        for (auto& t : thread_pool) {
            if (t.joinable()) {
                t.join();
                std::cout << "Esperando join...\n";
            }
        }
        
        // Só espera os processos filhos acabarem DEPOIS que as tarefas terminarem
        waitpid(pid_controle, nullptr, 0);
        //waitpid(pid_interface, nullptr, 0);

        log_message("MAIN", "Execução finalizada. Limpando memória.");

        // Desanexa e remove segmento de memória compartilhada
        shmdt(shm);
        shmctl(shmid, IPC_RMID, nullptr);
    }
    
    return 0;
}