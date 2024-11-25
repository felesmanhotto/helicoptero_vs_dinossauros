#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>  
#include <windows.h> // Para manipular o console no Windows

#define HELICOPTER_CHAR 'H'
#define MISSILE_CHAR '-'
#define DINO_CHAR 'D'
#define HELICOPTER_START_X 10
#define HELICOPTER_START_Y 10
#define MAX_DINOS 5
#define MAX_SLOTS 5 // Número de slots no depósito

typedef struct {
    int x, y; // Coordenadas do dinossauro
    int ativo; // 1 se o dinossauro está ativo, 0 caso contrário
    int vida;  // Vida restante (número de acertos na cabeça necessários)
} Dinossauro;

typedef struct {
    int slots[MAX_SLOTS]; // Cada slot representa 1 míssil (1 = cheio, 0 = vazio)
    int misseis_disponiveis; // Contador de mísseis no depósito
} Deposito;

Dinossauro dinos[MAX_DINOS]; // Armazena os dinossauros

Deposito deposito; // Depósito global

// Variáveis globais
int helicoptero_x = HELICOPTER_START_X;
int helicoptero_y = HELICOPTER_START_Y;
int jogo_ativo = 1; // Controla o loop principal do jogo
int num_dinos = 0; // Conta o número de dinossauros ativos
int misseis_hel = 5; // Helicóptero começa com 5 mísseis

// Mutex
pthread_mutex_t console_mutex;
pthread_mutex_t dinos_mutex;
pthread_mutex_t deposito_mutex; // Sincronizar acesso ao depósito
pthread_mutex_t misseis_mutex;

pthread_cond_t deposito_cond;   // Variável de condição para o depósito

// Função para redimensionar o console
void setConsoleSize(int width, int height) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    // Ajusta o buffer de tela
    COORD bufferSize = {width, height};
    SetConsoleScreenBufferSize(hConsole, bufferSize);

    // Ajusta o tamanho da janela
    SMALL_RECT windowSize = {0, 0, width - 1, height - 1};
    SetConsoleWindowInfo(hConsole, TRUE, &windowSize);
}

// Função para posicionar o cursor no console
void gotoxy(int x, int y) {
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

// Função para desenhar o cenário inicial
void desenha_cenario() {
    system("cls"); // Limpa a tela
    printf("=== BEM-VINDO AO JOGO ===\n");
    printf("Use as setas do teclado para mover o helicoptero.\n");
    printf("Pressione ESPACO para disparar. Boa sorte!\n");

    // Linha superior do cenário
    gotoxy(0, 3);
    for (int i = 0; i < 80; i++) printf("=");

    // Bordas verticais
    for (int i = 4; i < 20; i++) {
        gotoxy(0, i);
        printf("|");
        gotoxy(79, i);
        printf("|");
    }

    // Linha inferior do cenário
    gotoxy(0, 20);
    for (int i = 0; i < 80; i++) printf("=");

    // Desenhar o depósito
    gotoxy(5, 18);
    printf("[DEP]");
}

// Função para desenhar o helicóptero
void desenha_helicoptero(int x, int y) {
    pthread_mutex_lock(&console_mutex);
    gotoxy(x, y);
    printf("%c", HELICOPTER_CHAR);
    pthread_mutex_unlock(&console_mutex);
}

// Função para apagar o helicóptero
void apaga_helicoptero(int x, int y) {
    pthread_mutex_lock(&console_mutex);
    gotoxy(x, y);
    printf(" ");
    pthread_mutex_unlock(&console_mutex);
}

// Função executada pela thread do helicóptero
void* movimenta_helicoptero(void* arg) {
    while (jogo_ativo) {
        if (GetAsyncKeyState(VK_UP) & 0x8000) { // Tecla "Seta para Cima" ****
            if (helicoptero_y > 4) { // Limite superior
                apaga_helicoptero(helicoptero_x, helicoptero_y);
                helicoptero_y--;
            }
        }
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) { // Tecla "Seta para Baixo"
            if (helicoptero_y < 19) { // Limite inferior
                apaga_helicoptero(helicoptero_x, helicoptero_y);
                helicoptero_y++;
            }
        }
        desenha_helicoptero(helicoptero_x, helicoptero_y);
        Sleep(50); // Suaviza o movimento
    }
    return NULL;
}

void recarregar_helicoptero() {
    pthread_mutex_lock(&deposito_mutex);

    while (deposito.misseis_disponiveis == 0) {
        gotoxy(0, 22); // Abaixo do cenário **sem proteçao mutex
        printf("\nO helicóptero está esperando mísseis no depósito...\n");
        pthread_cond_wait(&deposito_cond, &deposito_mutex);
    }

    // Recarrega mísseis do depósito
    for (int i = 0; i < MAX_SLOTS && misseis_hel < 5; i++) {
        if (deposito.slots[i] == 1) {
            deposito.slots[i] = 0; // Remove do slot
            deposito.misseis_disponiveis--;

            pthread_mutex_lock(&misseis_mutex);
            misseis_hel++;
            pthread_mutex_unlock(&misseis_mutex);
        }
    }

    gotoxy(0, 21); // Proteger com mutex?
    printf("\nO helicoptero recarregou e tem %d misseis agora.\n", misseis_hel);
    pthread_mutex_unlock(&deposito_mutex);
}

// Função executada pelas threads de mísseis
void* dispara_missil(void* arg) {
    int missil_x = helicoptero_x + 1; // Começa à direita do helicóptero
    int missil_y = helicoptero_y;    // Mesma altura do helicóptero

    while (missil_x < 79 && jogo_ativo) { // Enquanto o míssil está no cenário
        pthread_mutex_lock(&console_mutex);
        gotoxy(missil_x, missil_y);
        printf("%c", MISSILE_CHAR); // Desenha o míssil
        pthread_mutex_unlock(&console_mutex);

        Sleep(50); // Pausa para suavizar o movimento

        pthread_mutex_lock(&console_mutex);
        gotoxy(missil_x, missil_y);
        printf(" "); // Apaga o míssil da posição anterior
        pthread_mutex_unlock(&console_mutex);

        missil_x++; // Move o míssil para a direita

        // Verifica colisão com dinossauros
        pthread_mutex_lock(&dinos_mutex);
        for (int i = 0; i < MAX_DINOS; i++) {
            if (dinos[i].ativo && missil_x == dinos[i].x && missil_y == dinos[i].y - 1) { // Cabeça é "y - 1"
                dinos[i].vida--; // Reduz a vida do dinossauro
                
                if (dinos[i].vida <= 0) { // Se a vida chega a 0, destrua o dinossauro
                    dinos[i].ativo = 0;

                    pthread_mutex_unlock(&dinos_mutex);

                    pthread_mutex_lock(&console_mutex);
                    gotoxy(dinos[i].x, dinos[i].y);
                    printf(" "); // Apaga o corpo
                    gotoxy(dinos[i].x, dinos[i].y - 1);
                    printf(" "); // Apaga a cabeça
                    pthread_mutex_unlock(&console_mutex);

                    return NULL; // O míssil desaparece após a colisão
                }

                pthread_mutex_unlock(&dinos_mutex);
                return NULL; // O míssil desaparece mesmo sem destruir completamente o dinossauro
            }
        }
        pthread_mutex_unlock(&dinos_mutex);
    }

    return NULL; // Finaliza o míssil ao sair do cenário
}

void* abastece_deposito(void* arg) {
    while (jogo_ativo) {
        pthread_mutex_lock(&deposito_mutex);

        // Abastece slots vazios
        for (int i = 0; i < MAX_SLOTS; i++) {
            if (deposito.slots[i] == 0) {
                deposito.slots[i] = 1; // Preenche o slot
                deposito.misseis_disponiveis++;
            }
        }

        printf("\nO caminhao abasteceu o deposito com misseis.\n");
        pthread_cond_broadcast(&deposito_cond); // Notifica o helicóptero
        pthread_mutex_unlock(&deposito_mutex);

        Sleep(5000); // Abastece a cada 5 segundos
    }
    return NULL;
}



// Função executada pelas threads de dinossauros
void* movimenta_dino(void* arg) {
    int dino_id = *(int*)arg; // Recebe o índice do dinossauro ***
    free(arg);

    while (dinos[dino_id].x > 1 && jogo_ativo && dinos[dino_id].ativo) {
        pthread_mutex_lock(&console_mutex);
        gotoxy(dinos[dino_id].x, dinos[dino_id].y);
        printf("%c", DINO_CHAR); // Corpo
        gotoxy(dinos[dino_id].x, dinos[dino_id].y - 1);
        printf("O"); // Cabeça
        pthread_mutex_unlock(&console_mutex);

        Sleep(500);

        pthread_mutex_lock(&console_mutex);
        gotoxy(dinos[dino_id].x, dinos[dino_id].y);
        printf(" ");
        gotoxy(dinos[dino_id].x, dinos[dino_id].y - 1);
        printf(" ");
        pthread_mutex_unlock(&console_mutex);

        dinos[dino_id].x--;

        // Verifica colisão com o helicóptero
        if (dinos[dino_id].x == helicoptero_x && dinos[dino_id].y == helicoptero_y) {
            jogo_ativo = 0; // Finaliza o jogo
            printf("\nGame Over! O helicoptero foi destruido.\n");
            break;
        }
    }

    // Marca o dinossauro como inativo e apaga da tela, caso ainda não tenha sido
    pthread_mutex_lock(&console_mutex);
    gotoxy(dinos[dino_id].x, dinos[dino_id].y);
    printf(" ");
    gotoxy(dinos[dino_id].x, dinos[dino_id].y - 1);
    printf(" "); // Apaga a cabeça também
    pthread_mutex_unlock(&console_mutex);

    pthread_mutex_lock(&dinos_mutex);
    dinos[dino_id].ativo = 0;
    num_dinos--; // Reduz o contador global
    pthread_mutex_unlock(&dinos_mutex);



    return NULL;
}

// Função gerenciadora de dinossauros. Thread separada para ter frequencia (sleep) diferente
void* gerencia_dinos(void* arg) {
    while (jogo_ativo) {
        pthread_mutex_lock(&dinos_mutex);
        if (num_dinos < MAX_DINOS) {
            pthread_mutex_unlock(&dinos_mutex);

            pthread_t thread_dino;

            // Encontra um slot livre na estrutura de dinossauros
            int slot = -1;
            for (int i = 0; i < MAX_DINOS; i++) {
                if (dinos[i].ativo == 0) {
                    slot = i;
                    break;
                }
            }

            if (slot != -1) {
                dinos[slot].ativo = 1; // Marca o dinossauro como ativo
                dinos[slot].x = 78;
                dinos[slot].y = (rand() % 15) + 4;
                dinos[slot].vida = 3;

                // Passa o índice do dinossauro para a thread
                int* dino_id = malloc(sizeof(int));
                if (!dino_id) {
                    fprintf(stderr, "Erro ao alocar memória para dino_id\n");
                    exit(EXIT_FAILURE);
                }
                *dino_id = slot;

                pthread_create(&thread_dino, NULL, movimenta_dino, dino_id);    // ***
                pthread_detach(thread_dino);

                pthread_mutex_lock(&dinos_mutex);
                num_dinos++;
                pthread_mutex_unlock(&dinos_mutex);
            }
        } else {
            pthread_mutex_unlock(&dinos_mutex);
        }

        Sleep(5000); // Controle do intervalo de criação
    }
    return NULL;
}

// Função principal
int main() {

    // Inicializa o depósito
    for (int i = 0; i < MAX_SLOTS; i++) {
        deposito.slots[i] = 0; // Todos os slots começam vazios
    }
    deposito.misseis_disponiveis = 0;

    // Inicializa os mutex
    pthread_mutex_init(&console_mutex, NULL);
    pthread_mutex_init(&dinos_mutex, NULL);
    pthread_mutex_init(&deposito_mutex, NULL);
    pthread_cond_init(&deposito_cond, NULL);
    pthread_mutex_init(&misseis_mutex, NULL);


    // Redimensiona o console para 80 colunas e 25 linhas
    setConsoleSize(80, 25);

    desenha_cenario();

    pthread_t thread_helicoptero;
    pthread_create(&thread_helicoptero, NULL, movimenta_helicoptero, NULL); // Entender melhor os parametros aqui

    pthread_t thread_gerencia_dinos;
    pthread_create(&thread_gerencia_dinos, NULL, gerencia_dinos, NULL);

    pthread_t thread_caminhao;
    pthread_create(&thread_caminhao, NULL, abastece_deposito, NULL);


    // Loop principal para entradas do jogador
    while (jogo_ativo) {
        if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
            pthread_mutex_lock(&misseis_mutex);
            if (misseis_hel > 0) {
                misseis_hel--;
                pthread_mutex_unlock(&misseis_mutex);

                pthread_t thread_missil;
                pthread_create(&thread_missil, NULL, dispara_missil, NULL);
                pthread_detach(thread_missil);
                Sleep(200);

            } else {
                pthread_mutex_unlock(&misseis_mutex);
                printf("\nO helicoptero esta sem misseis! Precisa recarregar.\n");
                recarregar_helicoptero();
            }
        }
        Sleep(50); // Suaviza o loop principal
    }
    // Espera a thread do helicóptero finalizar ****
    pthread_join(thread_helicoptero, NULL);
    pthread_join(thread_gerencia_dinos, NULL);
    pthread_join(thread_caminhao, NULL);

    // Destroi o mutex
    pthread_mutex_destroy(&console_mutex);
    pthread_mutex_destroy(&dinos_mutex);
    pthread_mutex_destroy(&deposito_mutex);
    pthread_cond_destroy(&deposito_cond);
    pthread_mutex_destroy(&misseis_mutex);


    return 0;
}
