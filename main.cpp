#include "sincronizacao.hpp"

int main (){

    log_message(
        "MAIN",
        "Inicializando sistema"
    );

    //INICIALIZAÇÃO PROCESSOS

    pid_t pid;

    pid = fork();

   if (pid == 0){

        log_message(
            "PROCESSO",
            "Processo comando_navegacao criado"
        );

        comando_navegacao ();
   }

   else if (pid < 0){
        perror ("Erro ao criar processo\n");
        exit(1);
   }

   else {

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

        log_message(
            "MAIN",
            "Criando thread distancia_percorrida"
        );

        threads_navegacao.emplace_back(distancia_percorrida, std::ref (mutex_navegacao), std::ref(BUFFER_NAVEGACAO));

        log_message(
            "MAIN",
            "Criando thread inspecao_camera"
        );

        threads_navegacao.emplace_back(inspecao_camera);

        log_message(
            "MAIN",
            "Criando thread coletor_dados"
        );

        threads_navegacao.emplace_back(coletor_dados, std::ref (mutex_nivel), std::ref(BUFFER_NIVEL));

        log_message(
            "MAIN",
            "Criando thread reconstrucao_teto"
        );

        threads_navegacao.emplace_back(reconstrucao_teto, std::ref (mutex_nivel), std::ref(BUFFER_NIVEL));

        log_message(
            "MAIN",
            "Iniciando controle_navegacao"
        );

        controle_navegacao(std::ref (mutex_navegacao), std::ref(BUFFER_NAVEGACAO));

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

    return 0;
}