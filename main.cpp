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
    

 // --- PROCESSO 2: COMANDO (FILHO) E CONTROLE (PAI) ---
pid_t pid_controle = fork();

if (pid_controle == 0) {
    // ---------------------------------------------------------------------
    // BLOCO DO FILHO: COMANDO DE NAVEGAÇÃO
    // ---------------------------------------------------------------------
    log_message("PROCESSO", "Processo comando_navegacao criado");

    // 1. Instanciar o Asio APENAS para o filho
    boost::asio::io_context io_filho;
    boost::asio::io_context::strand strand_filho(io_filho);

    tempo_tarefa t_comando(io_filho, microssegundos(500));
    t_comando.async_wait(boost::asio::bind_executor(strand_filho, 
        std::bind(comando_navegacao, std::placeholders::_1, &t_comando, &strand_filho, shm)));

    shm->c_automatico = true;
    shm->j_sp_velocidade = 50;

    // 2. Iniciar o laço de eventos do filho (Descomentado)
    // Se não chamar run(), as tarefas do filho nunca vão executar!
    io_filho.run(); 

    // Limpeza do filho
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

    // 3. Instanciar o Asio isolado para o Pai
    boost::asio::io_context io_pai;

    // 4. CRIANDO MÚLTIPLAS STRANDS (Segregação de Lógica)
    // Tarefas na mesma strand não concorrem. Strands diferentes rodam em paralelo.
    boost::asio::io_context::strand strand_navegacao(io_pai); // Para dist e controle
    boost::asio::io_context::strand strand_processamento(io_pai); // Para reconstrução e dados
    boost::asio::io_context::strand strand_camera(io_pai); // Para câmera isolada

    struct VarCondSinc sinc;

    // --- DEFINIÇÃO DOS TEMPORIZADORES (TIMERS) ---
    tempo_tarefa t1_dist(io_pai, microssegundos(PERIODO_DISTANCIA));
    tempo_tarefa t2_cam(io_pai, microssegundos(PERIODO_CAMERA));
    tempo_tarefa t3_col(io_pai, microssegundos(PERIODO_COLETOR));
    tempo_tarefa t4_rec(io_pai, microssegundos(PERIODO_RECONSTRUCAO));
    tempo_tarefa t5_con(io_pai, microssegundos(PERIODO_CONTROLE));

    // --- CRIAÇÃO DE SIGNAL ---
    boost::asio::signal_set sinais (io_pai);
    sinais.add(SIGUSR1);

    // --- AGENDAMENTO DAS TAREFAS (ASYNC_WAIT) ---
    log_message("MAIN", "Agendando tarefas assíncronas...");

    // Associando à Strand de Navegação (Crítica)
    t1_dist.async_wait(boost::asio::bind_executor(strand_navegacao, 
        std::bind(distancia_percorrida, std::placeholders::_1, &t1_dist, &strand_navegacao, shm, std::ref(sinc))));

    t5_con.async_wait(boost::asio::bind_executor(strand_navegacao, 
        std::bind(controle_navegacao, std::placeholders::_1, &t5_con, &strand_navegacao, shm, std::ref(sinc))));

    // Associando à Strand de Processamento (Menos Crítica)
    t4_rec.async_wait(boost::asio::bind_executor(strand_processamento, 
        std::bind(reconstrucao_teto, std::placeholders::_1, &t4_rec, &strand_processamento, shm, std::ref(sinc))));

    t3_col.async_wait(boost::asio::bind_executor(strand_processamento, 
        std::bind(coletor_dados, std::placeholders::_1, &t3_col, &strand_processamento, shm, std::ref(sinc))));

    // Associando à Strand da Câmera (Isolada)
    /*t2_cam.async_wait(boost::asio::bind_executor(strand_camera, 
        std::bind(inspecao_camera, std::placeholders::_1, &t2_cam, &strand_camera, std::ref(sinc.mutex_camera), shm)));
    */
    sinais.async_wait(
        std::bind(handler_signal, std::placeholders::_1, std::placeholders::_2, &t2_cam, &strand_camera,
            std::ref(sinc.mutex_camera), shm));

    // --- EXECUÇÃO (THREAD POOL) ---
    // 5. Reativar o Pool de Threads. Com 4 threads, as nossas 3 Strands podem rodar 
    // simultaneamente em núcleos reais do processador.

    while (!shm->c_automatico) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    int num_threads = 4; 
    std::vector<std::thread> thread_pool;
    log_message("MAIN", "Iniciando Thread Pool do Boost.Asio com " + std::to_string(num_threads) + " threads");

    for (int i = 0; i < num_threads; ++i) {
        thread_pool.emplace_back([&io_pai]() { io_pai.run(); });
    }

    std::cout << "Sistema rodando...\n";

    // Simular tempo de atividade do sistema
    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::cout << "Iniciando processo de desligamento...\n";

    // Modificar a flag faz com que os handlers das tarefas não agendem novas chamadas.
    // Assim, a fila de tarefas do io_context esvazia naturalmente e o run() encerra.
    shm->c_automatico = false;

    // --- SINCRONIZAÇÃO FINAL ---
    for (auto& t : thread_pool) {
        if (t.joinable()) {
            std::cout << "Esperando join da thread...\n";
            t.join();
        }
    }
    
    std::cout << "Thread pool encerrada.\n";
    
    // Espera o processo filho acabar
    waitpid(pid_controle, nullptr, 0);

    log_message("MAIN", "Execução finalizada. Limpando memória.");

    // Desanexa e remove segmento de memória compartilhada
    shmdt(shm);
    shmctl(shmid, IPC_RMID, nullptr);
}
}   