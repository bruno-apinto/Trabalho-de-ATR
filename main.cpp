#include "include/includes.hpp"
#include "include/shared_memory.hpp"
#include "include/sincronizacao.hpp"
#include "include/simulacao.hpp"

int main (){

    using namespace boost::interprocess;

    simulacao();

    std::remove(SHM_FILE); // remove a memória compartilhada caso ela já exista

    {
    std::filebuf fbuf;
    fbuf.open(SHM_FILE, std::ios_base::in | std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);

    if(!fbuf.is_open()){
        std::cerr << "Erro ao criar arquivo de memória compartilhada" << std::endl;
        return 1;
    }

    fbuf.pubseekoff(sizeof(MemoriaCompartilhada)-1, std::ios_base::beg);
    fbuf.sputc(0);
    fbuf.close();
    } // cria arquivo que será mapeado na memória

    std::cout << "Arquivo de memória criado em: " << SHM_FILE << std::endl;
    std::cout << "Tamanho da struct C++: " << sizeof(MemoriaCompartilhada) << " bytes" << std::endl;

    file_mapping shm_file(SHM_FILE, read_write);
    mapped_region region(shm_file, read_write); // mapeia o arquivo no espaço de memória

    MemoriaCompartilhada* shm = static_cast<MemoriaCompartilhada*>(region.get_address());

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

    shm->c_encerrar = false;

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

        //INICIALIZAÇÃO PROCESSOS

        pid_t pid;

        pid = fork();

    if (pid == 0){

                log_message(
                "PROCESSO",
                "Processo comando_navegacao criado"
            );

            file_mapping shm_child(SHM_FILE, read_write); // abre a memória compartilhada criada

            mapped_region region_child(shm_child, read_write);
            MemoriaCompartilhada* shm_filho = static_cast<MemoriaCompartilhada*>(region_child.get_address());

            comando_navegacao (shm_filho);

            std::cout << "Comando de navegação escreveu na memória compartilhada:" << std::endl;
            std::cout << "c_automatico = " << shm_filho->c_automatico << std::endl;
            std::cout << "j_sp_velocidade = " << shm_filho->j_sp_velocidade << std::endl;

            return 0;
    }

    else if (pid < 0){
            perror ("Erro ao criar processo\n");
            exit(1);
    }

    else {

        waitpid(pid, nullptr, 0); // espera o processo filho terminar

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
        std::mutex shm_mutex;

        // Inicializar MQTT Manager para comunicação com operação remota
        MQTTManager mqtt_manager(shm, shm_mutex);
        
        log_message("MAIN", "Inicializando MQTT Manager...");
        if (mqtt_manager.initialize()) {
            log_message("MAIN", "MQTT Manager inicializado com sucesso");
        } else {
            log_message("MAIN", "Falha ao inicializar MQTT Manager - continuando sem MQTT");
        }

        std::vector <std::thread> threads_navegacao;

        log_message(
            "MAIN",
            "Criando thread distancia_percorrida"
        );

        threads_navegacao.emplace_back(
            distancia_percorrida,
            std::ref(mutex_navegacao),
            std::ref(BUFFER_NAVEGACAO)
        );

        log_message(
            "MAIN",
            "Criando thread inspecao_camera"
        );

        threads_navegacao.emplace_back(
            inspecao_camera,
            std::ref(mutex_camera),
            shm
        );

        log_message(
            "MAIN",
            "Criando thread coletor_dados"
        );

        threads_navegacao.emplace_back(
            coletor_dados,
            std::ref(mutex_nivel),
            std::ref(BUFFER_NIVEL)
        );

        log_message(
            "MAIN",
            "Criando thread reconstrucao_teto"
        );

        threads_navegacao.emplace_back(
            reconstrucao_teto,
            std::ref(mutex_navegacao),
            std::ref(mutex_nivel),
            std::ref(mutex_camera),
            std::ref(BUFFER_NAVEGACAO),
            std::ref(BUFFER_NIVEL),
            shm
        );

        log_message(
            "MAIN",
            "Criando thread controle_navegacao"
        );

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

        // Finalizar MQTT Manager
        log_message("MAIN", "Finalizando MQTT Manager...");
        mqtt_manager.shutdown();
        log_message("MAIN", "MQTT Manager finalizado");
        
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

        log_message(
            "MAIN",
            "Execução finalizada"
        );
   }

    // Desanexa memória no pai
    //std::remove(SHM_FILE); 

    return 0;
}