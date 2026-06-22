#include "sincronizacao.hpp"

#define ELEMENTOS_BUFFERS 10

//sincronização
std::condition_variable camera;

//teste para finalizar a camera
bool finalizar_camera = false;
int eventos_camera = 0;

//Variaveis de condição buffers
std::condition_variable leitura_buffer_navegacao;
std::condition_variable escrita_buffer_navegacao;

int AR_NAVEGACAO = 0;
int WR_NAVEGACAO = 0;
int AW_NAVEGACAO = 0;
int WW_NAVEGACAO = 0;

std::condition_variable leitura_buffer_nivel; 
std::condition_variable escrita_buffer_nivel;
int dados_nivel = 0;

//Funções auxiliares de debbug
std::mutex mutex_log;
int miss[4] = {0, 0, 0, 0};
int executado[4] = {0, 0, 0, 0};

// Tolerância máxima de atraso em microssegundos antes de considerar um "Miss de Deadline"
const long long LIMITE_ATRASO_US = 5000;

// Estimativa de velocidade compartilhada entre distancia_percorrida e controle_navegacao.
// Ambas as tarefas rodam na mesma strand (strand_navegacao), portanto sem corrida de dados.
static float s_vel_encoder_ms = 0.0f;
static auto  s_t_ultimo_encoder = std::chrono::steady_clock::now();
static bool  s_encoder_ativo = false;

void log_message(const std::string& thread, const std::string& mensagem) {
    std::lock_guard<std::mutex> lock(mutex_log);
    auto agora = std::chrono::system_clock::now();
    auto tempo = std::chrono::system_clock::to_time_t(agora);
    std::cout << "[" << std::put_time(std::localtime(&tempo), "%H:%M:%S") << "] "
              << "[" << thread << "] " << mensagem << std::endl;
}

float numero_aleatorio_debugg() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(1.0f, 100.0f);
    return dis(gen);
}

// =========================================================================
// FUNÇÃO AUXILIAR DE TEMPO REAL
// =========================================================================
void reagendar_tarefa(boost::asio::steady_timer* t, int periodo_us, const std::string& nome_tarefa) {
    auto agora = std::chrono::steady_clock::now();
    auto atraso = std::chrono::duration_cast<std::chrono::milliseconds>(agora - t->expiry()).count();

    // Comentado para não poluir o log no modo Oversampling rápido
    // if (atraso > LIMITE_ATRASO_US) {
    //     log_message(nome_tarefa, "ALERTA: Deadline violado! Atraso de " + std::to_string(atraso) + " us");
    // }

    auto proximo_ciclo = t->expiry() + boost::asio::chrono::milliseconds(periodo_us);
    
    if (proximo_ciclo < agora) {
        proximo_ciclo = agora + boost::asio::chrono::milliseconds(periodo_us);
    }
    
    t->expires_at(proximo_ciclo);
}

void handler_signal(const boost::system::error_code& error, int signal_number, 
                    boost::asio::steady_timer* t, boost::asio::io_context::strand* strand_camera, 
                    std::mutex& mtx, MemoriaCompartilhada* shm, boost::asio::signal_set* sinais) {
    
    if (signal_number == SIGUSR1) {
        eventos_camera++;

        boost::asio::post(*strand_camera, std::bind(inspecao_camera, 
                        boost::system::error_code(), t, strand_camera, std::ref(mtx), shm));        
    }

        // --- REAGENDAMENTO CRÍTICO ---

        sinais->async_wait(std::bind(handler_signal, std::placeholders::_1, std::placeholders::_2, 
                                     t, strand_camera, std::ref(mtx), shm, sinais));
}


// =========================================================================
// TAREFAS ASSÍNCRONAS DO SISTEMA
// =========================================================================

void comando_navegacao(const boost::system::error_code& e, boost::asio::steady_timer* t,
                       boost::asio::io_context::strand* strand, MemoriaCompartilhada* shm)
{
    if (e) return;
    if (shm->c_encerrar) {
        log_message("COMANDO", "Encerrando comando de navegação.");
        return;
    }

    // Traduz comandos em estados — manual/automático
    if (shm->c_automatico) {
        shm->e_automatico = true;
        shm->c_automatico = false;
        log_message("COMANDO", "Modo AUTOMÁTICO ativado");
    }
    if (shm->c_man) {
        shm->e_automatico = false;
        shm->c_man = false;
        shm->o_aceleracao = 0;  // cancela qualquer desaceleração de inspeção
        log_message("COMANDO", "Modo MANUAL ativado");
    }

    reagendar_tarefa(t, PERIODO_COMANDO, "COMANDO");
    t->async_wait(boost::asio::bind_executor(*strand, 
        std::bind(comando_navegacao, std::placeholders::_1, t, strand, shm)));
}


void controle_navegacao(const boost::system::error_code& e, boost::asio::steady_timer* t,
                        boost::asio::io_context::strand* strand, MemoriaCompartilhada* shm, VarCondSinc &sinc)
{
    if (e) return;
    if (shm->c_encerrar) return;

    bool processou_algo = false;

    {
        std::lock_guard<std::mutex> lock(sinc.mtx_navegacao);
        while (sinc.disp_naveg_c > 0) {
            sinc.LER_IDX_NAVEG_C = (sinc.LER_IDX_NAVEG_C + 1) % ELEMENTOS_BUFFERS;
            sinc.disp_naveg_c--;
            processou_algo = true;
        }
    }

    if (processou_algo) {
        executado[0]++;
    } else {
        miss[0]++;
    }

    // ── Controlador PID de velocidade ────────────────────────────────────────
    // Computa P, I e D a partir do encoder. Em um sistema embarcado real a
    // saída acionaria o driver de motor; aqui a simulação Python já gerencia a
    // física da velocidade, então a saída é calculada mas não sobrescreve
    // o_aceleracao (que é exclusivo de reconstrucao_teto para inspeção).
    if (shm->e_automatico && s_encoder_ativo) {
        static float integral  = 0.0f;
        static float erro_ant  = 0.0f;
        static bool  insp_ant  = false;

        constexpr float DT = PERIODO_CONTROLE / 1000.0f;
        constexpr float Kp = 5.0f, Ki = 1.0f, Kd = 0.2f;

        // Zera integrador ao sair de inspeção para evitar windup residual
        if (insp_ant && !shm->e_inspecao) { integral = 0.0f; erro_ant = 0.0f; }
        insp_ant = shm->e_inspecao;

        float sp_ms  = shm->j_sp_velocidade * 0.04f;   // sp=50 → 2.0 m/s
        float erro   = sp_ms - s_vel_encoder_ms;

        integral = std::clamp(integral + erro * DT, -20.0f, 20.0f);
        float derivada = (erro - erro_ant) / DT;
        erro_ant = erro;

        [[maybe_unused]] float saida_pid =
            std::clamp(Kp * erro + Ki * integral + Kd * derivada, -100.0f, 100.0f);
    }
    // ─────────────────────────────────────────────────────────────────────────

    reagendar_tarefa(t, PERIODO_CONTROLE, "CONTROLE");
    t->async_wait(boost::asio::bind_executor(*strand,
        std::bind(controle_navegacao, std::placeholders::_1, t, strand, shm, std::ref(sinc))));
}


void distancia_percorrida(const boost::system::error_code& e, boost::asio::steady_timer* t,
                          boost::asio::io_context::strand* strand, MemoriaCompartilhada* shm, VarCondSinc &sinc)
{
    if (e) return;
    if (shm->c_encerrar) return;

    // Detecta transição do encoder e acumula distância em metros
    static bool ultimo_encoder = false;
    bool encoder_atual = shm->i_encoder;
    auto agora_enc = std::chrono::steady_clock::now();

    if (encoder_atual != ultimo_encoder) {
        // Nova transição: calcula velocidade = 1 metro / tempo desde última transição
        if (s_encoder_ativo) {
            float dt = std::chrono::duration<float>(agora_enc - s_t_ultimo_encoder).count();
            if (dt > 0.01f)
                s_vel_encoder_ms = 1.0f / dt;
        }
        s_t_ultimo_encoder = agora_enc;
        s_encoder_ativo = true;
        shm->posicao_x += 1.0f;
        ultimo_encoder = encoder_atual;
    } else if (s_encoder_ativo) {
        // Entre transições: velocidade estimada decai naturalmente com o tempo
        float dt = std::chrono::duration<float>(agora_enc - s_t_ultimo_encoder).count();
        if (dt > 0.01f)
            s_vel_encoder_ms = 1.0f / dt;
    }

    {
        std::lock_guard<std::mutex> lock(sinc.mtx_navegacao);

        if (sinc.disp_naveg_c >= ELEMENTOS_BUFFERS) {
            sinc.LER_IDX_NAVEG_C = (sinc.LER_IDX_NAVEG_C + 1) % ELEMENTOS_BUFFERS;
            sinc.disp_naveg_c--;
        }
        if (sinc.disp_naveg_r >= ELEMENTOS_BUFFERS) {
            sinc.LER_IDX_NAVEG_R = (sinc.LER_IDX_NAVEG_R + 1) % ELEMENTOS_BUFFERS;
            sinc.disp_naveg_r--;
        }

        // Escreve a posição atual para controle e reconstrução
        sinc.BUFFER_NAVEGACAO[sinc.ESC_IDX_NAVEG] = shm->posicao_x;
        sinc.ESC_IDX_NAVEG = (sinc.ESC_IDX_NAVEG + 1) % ELEMENTOS_BUFFERS;

        sinc.disp_naveg_c++;
        sinc.disp_naveg_r++;
    }

    executado[1]++;

    reagendar_tarefa(t, PERIODO_DISTANCIA, "DISTANCIA");
    t->async_wait(boost::asio::bind_executor(*strand,
        std::bind(distancia_percorrida, std::placeholders::_1, t, strand, shm, std::ref(sinc))));
}


void reconstrucao_teto(const boost::system::error_code& e, boost::asio::steady_timer* t,
                       boost::asio::io_context::strand* strand, MemoriaCompartilhada* shm, VarCondSinc &sinc)
{
    if (e) return;
    if (shm->c_encerrar) return;

    // Consome atualizações de posição do buffer de navegação
    bool houve_atualizacao = false;
    {
        std::lock_guard<std::mutex> lock_nav(sinc.mtx_navegacao);
        while (sinc.disp_naveg_r > 0) {
            sinc.LER_IDX_NAVEG_R = (sinc.LER_IDX_NAVEG_R + 1) % ELEMENTOS_BUFFERS;
            sinc.disp_naveg_r--;
            houve_atualizacao = true;
        }
    }

    if (houve_atualizacao) {
        // Filtro de média móvel (janela de 2 amostras) sobre o LIDAR
        static float janela[2] = {};
        static int idx_janela = 0;
        static bool janela_cheia = false;

        janela[idx_janela % 2] = static_cast<float>(shm->i_lidar);
        idx_janela++;
        if (idx_janela >= 2) janela_cheia = true;

        int n = janela_cheia ? 2 : idx_janela;
        float soma = 0.0f;
        for (int i = 0; i < n; ++i) soma += janela[i];
        float media = soma / n;

        // Escreve valor filtrado no buffer de nível
        {
            std::lock_guard<std::mutex> lock_nivel(sinc.mtx_nivel);

            // Estabelece baseline a partir das primeiras 15 leituras (antes de qualquer anomalia)
            // Depois compara cada leitura contra esse baseline para detectar variação severa
            static float baseline = 0.0f;
            static float baseline_soma = 0.0f;
            static int   baseline_n = 0;
            const  int   BASELINE_AMOSTRAS = 15;

            if (baseline_n < BASELINE_AMOSTRAS) {
                // Aguarda janela cheia e média estabilizada (> 100) para excluir zeros de startup
                // e valores de transição como (0+0+0+0+145)/5 = 29 que corrompem o baseline
                if (janela_cheia && media > 100.0f) {
                    baseline_soma += media;
                    baseline_n++;
                    if (baseline_n == BASELINE_AMOSTRAS) {
                        baseline = baseline_soma / BASELINE_AMOSTRAS;
                        log_message("RECONSTRUCAO", "Baseline estabelecido: " + std::to_string(baseline));
                    }
                }
            } else {
                float variacao = std::abs(media - baseline);
                if (variacao > shm->variacao_severa) {
                    if (!shm->e_inspecao) {   // dispara apenas uma vez por anomalia
                        shm->e_inspecao = true;
                        shm->o_liga_camera = true;
                        if (shm->e_automatico)    // desacelera apenas em modo automático
                            shm->o_aceleracao = -30;
                        kill(getpid(), SIGUSR1);
                        log_message("RECONSTRUCAO", "Variacao severa detectada: " + std::to_string(variacao));
                    }
                } else if (shm->e_inspecao) { // anomalia superada: restaura estado normal
                    shm->e_inspecao = false;
                    shm->o_liga_camera = false;
                    shm->o_aceleracao = 0;
                    log_message("RECONSTRUCAO", "Anomalia superada, retomando velocidade normal");
                }
            }

            if (sinc.disp_nivel >= ELEMENTOS_BUFFERS) {
                sinc.LER_IDX_NIVEL = (sinc.LER_IDX_NIVEL + 1) % ELEMENTOS_BUFFERS;
                sinc.disp_nivel--;
            }
            sinc.BUFFER_NIVEL[sinc.ESC_IDX_NIVEL] = media;
            sinc.ESC_IDX_NIVEL = (sinc.ESC_IDX_NIVEL + 1) % ELEMENTOS_BUFFERS;
            sinc.disp_nivel++;
        }

        // Sinaliza para o publisher MQTT que há novo ponto de perfil
        shm->perfil_y = media;
        shm->perfil_novo = true;

        executado[2]++;
    } else {
        miss[2]++;
    }

    reagendar_tarefa(t, PERIODO_RECONSTRUCAO, "RECONSTRUCAO");
    t->async_wait(boost::asio::bind_executor(*strand,
        std::bind(reconstrucao_teto, std::placeholders::_1, t, strand, shm, std::ref(sinc))));
}


void coletor_dados(const boost::system::error_code& e, boost::asio::steady_timer* t,
                   boost::asio::io_context::strand* strand, MemoriaCompartilhada* shm, VarCondSinc &sinc)
{
    static FILE* arquivo_coletor = nullptr;
    static bool arquivo_aberto = false;
    static std::size_t contador_coletor = 0;
    static const auto inicio_coletor = std::chrono::steady_clock::now();

    // Variáveis para cálculo online de confiança
    // Mais medições numa mesma zona de posição → maior confiança
    static float zona_x_anterior = -999.0f;
    static int contagem_zona = 0;

    if (e || shm->c_encerrar) {
        if (arquivo_aberto) {
            std::fflush(arquivo_coletor);
            std::fclose(arquivo_coletor);
            arquivo_aberto = false;
            arquivo_coletor = nullptr;
            log_message("COLETOR", "Arquivo de coleta fechado.");
        }
        return;
    }

    if (!arquivo_aberto) {
        arquivo_coletor = std::fopen("dados_coletados.csv", "a+");
        arquivo_aberto = true;
        if (arquivo_coletor == nullptr) {
            log_message("COLETOR", "Erro ao abrir arquivo de coleta!");
        } else {
            log_message("COLETOR", "Arquivo de coleta aberto para escrita.");
            std::fseek(arquivo_coletor, 0, SEEK_END);
            if (std::ftell(arquivo_coletor) == 0) {
                std::fprintf(arquivo_coletor,
                    "timestamp_ms;posicao_x;teto_filtrado_y;lidar_raw;"
                    "encoder;aceleracao;velocidade;e_automatico;e_inspecao;confianca\n");
                std::fflush(arquivo_coletor);
            }
        }
    }

    std::vector<float> dados_coletados;
    {
        std::lock_guard<std::mutex> lock(sinc.mtx_nivel);
        while (sinc.disp_nivel > 0) {
            dados_coletados.push_back(sinc.BUFFER_NIVEL[sinc.LER_IDX_NIVEL]);
            sinc.LER_IDX_NIVEL = (sinc.LER_IDX_NIVEL + 1) % ELEMENTOS_BUFFERS;
            sinc.disp_nivel--;
        }
    }

    if (!dados_coletados.empty()) {
        executado[3]++;

        // Cálculo online de confiança: mais medições na mesma zona → maior confiança
        if (std::abs(shm->posicao_x - zona_x_anterior) < 1.0f) {
            contagem_zona = std::min(contagem_zona + 1, 10);
        } else {
            zona_x_anterior = shm->posicao_x;
            contagem_zona = 1;
        }
        float confianca = contagem_zona / 10.0f;
        shm->perfil_confianca = confianca; // disponibiliza para o publisher MQTT

        // Média de todas as leituras acumuladas no buffer — evita duplicatas e não perde passos
        float dado = 0.0f;
        for (float v : dados_coletados) dado += v;
        dado /= static_cast<float>(dados_coletados.size());
        const auto agora = std::chrono::steady_clock::now();
        const long long timestamp_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(agora - inicio_coletor).count();

        if (arquivo_coletor) {
            const int velocidade_efetiva = shm->j_sp_velocidade + shm->o_aceleracao;
            std::fprintf(arquivo_coletor, "%lld;%.2f;%.2f;%d;%d;%d;%d;%d;%d;%.2f\n",
                timestamp_ms,
                shm->posicao_x,
                dado,
                static_cast<int>(shm->i_lidar),
                static_cast<int>(shm->i_encoder),
                static_cast<int>(shm->o_aceleracao),
                velocidade_efetiva,
                static_cast<int>(shm->e_automatico),
                static_cast<int>(shm->e_inspecao),
                confianca);

            if (++contador_coletor >= 10) {
                contador_coletor = 0;
                std::fflush(arquivo_coletor);
            }
        }
    } else {
        miss[3]++;
    }

    if ((executado[3] + miss[3]) % 100 == 0) {
        std::cout << "[STATUS] Miss: ";
        for (auto i : miss) std::cout << i << " ";
        std::cout << "| Exec: ";
        for (auto i : executado) std::cout << i << " ";
        std::cout << "\n";
    }

    reagendar_tarefa(t, PERIODO_COLETOR, "COLETOR");
    t->async_wait(boost::asio::bind_executor(*strand,
        std::bind(coletor_dados, std::placeholders::_1, t, strand, shm, std::ref(sinc))));
}


void inspecao_camera(const boost::system::error_code& e, boost::asio::steady_timer* t,
                     boost::asio::io_context::strand* strand, std::mutex& mtx, MemoriaCompartilhada* shm) {
    if (e) return;
    if (shm->c_encerrar) return;

    {
        std::lock_guard<std::mutex> lock(mtx);
        if (eventos_camera > 0) {
            eventos_camera--;

            log_message("CAMERA", "Inspeção iniciada — processamento de imagem em curso");

            // Simula carga de CPU real: análise de imagem por visão computacional embarcada
            // (substitui sleep — usa o processador de fato, como exigido pelo enunciado)
            volatile float acumulador = 0.0f;
            for (int i = 1; i <= 800000; ++i) {
                acumulador += std::sqrt(static_cast<float>(i)) *
                              std::sin(static_cast<float>(i) * 0.0001f);
            }
            (void)acumulador; // impede otimização do compilador

            log_message("CAMERA", "Inspeção finalizada");
        }
    }

    reagendar_tarefa(t, PERIODO_CAMERA, "CAMERA");
    t->async_wait(boost::asio::bind_executor(*strand,
        std::bind(inspecao_camera, std::placeholders::_1, t, strand, std::ref(mtx), shm)));
}