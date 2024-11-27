#include <stdio.h>
#include <stdlib.h>
#include <time.h>  
#include <pthread.h>
#include <windows.h>

#define HELICOPTER_CHAR 'H'
#define MISSILE_CHAR '-'
#define DINO_CHAR 'D'
#define HELICOPTER_START_X 10
#define HELICOPTER_START_Y 10
#define MAX_DINOS 5
#define MAX_SLOTS 5
#define DEPOSITO_X 10
#define DEPOSITO_Y 18

typedef struct {
    int x, y; 
    int ativo; // 1 = ativo, 0 = inativo
    int vida;
} Dinossauro;

typedef struct {
    int* slots; // 1 = cheio, 0 = vazio)
    int misseis_disponiveis;
} Deposito;

Dinossauro dinos[MAX_DINOS]; // array dos dinos

Deposito deposito;

// Variaveis de dificuldade
int t = 5000; // tempo de spawn
int m = 2; // vida
int n = 5; // depósito


// Variáveis globais
int helicoptero_x = HELICOPTER_START_X;
int helicoptero_y = HELICOPTER_START_Y;
int jogo_ativo = 1; 
int num_dinos = 0;
int misseis_hel = 0; 

// Mutex
pthread_mutex_t console_mutex;
pthread_mutex_t dinos_mutex;
pthread_mutex_t deposito_mutex;
pthread_mutex_t misseis_mutex;

pthread_cond_t deposito_cond;   // variavel de condição


void setConsoleSize(int width, int height) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    COORD bufferSize = {width, height};
    SetConsoleScreenBufferSize(hConsole, bufferSize);

    SMALL_RECT windowSize = {0, 0, width - 1, height - 1};
    SetConsoleWindowInfo(hConsole, TRUE, &windowSize);
}

void desenha_menu() {
    system("cls");
    printf("=== SELECIONE A DIFICULDADE ===\n");
    printf("1. Facil: T = 7s, M = 1, N = 5\n");
    printf("2. Medio: T = 6s, M = 2, N = 4\n");
    printf("3. Dificil: T = 5s, M = 3, N = 3\n");
    printf("Escolha uma opcao: ");
}

void inicializar_dificuldade(int escolha) {
    switch (escolha) {
        case 1: t = 7000; m = 1; n = 5; break;
        case 2: t = 6000; m = 2; n = 4; break;
        case 3: t = 5000; m = 3; n = 3; break;
        default: t = 7000; m = 1; n = 5; break;
    }
}

// posicionar o cursor no console
void gotoxy(int x, int y) {
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

void atualizar_contador() {

    pthread_mutex_lock(&console_mutex); 
    gotoxy(1, 22); 
    printf("                                      "); 
    gotoxy(1, 22); 

    pthread_mutex_lock(&misseis_mutex); 
    printf("Misseis restantes: %d     ", misseis_hel); 
    pthread_mutex_unlock(&misseis_mutex);
    pthread_mutex_unlock(&console_mutex);   // mantem em lock ate finalizar os prints
}

void atualizar_contador_deposito() {
    pthread_mutex_lock(&console_mutex);
    gotoxy(3, 19); 
    printf("[");

    pthread_mutex_lock(&deposito_mutex);
    for (int i = 0; i < n; i++) {
        if (deposito.slots[i] == 1) {
            printf("#"); // cheio
        } else {
            printf("."); // vazio
        }
    }

    printf("]");
    pthread_mutex_unlock(&deposito_mutex);
    pthread_mutex_unlock(&console_mutex);   // mantem em lock ate finalizar os prints
}

void desenha_cenario() {
    system("cls");
    printf("=== HELICÓPTERO VS DINOSSAUROS ===\n");
    printf("Use as setas do teclado para mover o helicoptero.\n");
    printf("Pressione ESPACO para disparar.\n");

    gotoxy(0, 3);
    for (int i = 0; i < 80; i++) printf("=");

    for (int i = 4; i < 20; i++) {
        gotoxy(0, i);
        printf("|");
        gotoxy(79, i);
        printf("|");
    }

    gotoxy(0, 20);
    for (int i = 0; i < 80; i++) printf("=");

    gotoxy(5, 18);
    printf("[DEP]");
}

void verificar_game_over() {
    pthread_mutex_lock(&dinos_mutex);
    if (num_dinos >= MAX_DINOS) {
        jogo_ativo = 0;
        pthread_mutex_unlock(&dinos_mutex);

        pthread_mutex_lock(&console_mutex);
        system("cls");
        gotoxy(10, 10);
        printf("Game Over! O numero maximo de dinossauros foi atingido.\n");
        pthread_mutex_unlock(&console_mutex);

        return;
    }
    pthread_mutex_unlock(&dinos_mutex);
}

void desenha_helicoptero(int x, int y) {
    pthread_mutex_lock(&console_mutex);
    gotoxy(x, y);
    printf("%c", HELICOPTER_CHAR);
    pthread_mutex_unlock(&console_mutex);
}

void apaga_helicoptero(int x, int y) {
    pthread_mutex_lock(&console_mutex);
    gotoxy(x, y);
    printf(" ");
    pthread_mutex_unlock(&console_mutex);
}

void* movimenta_helicoptero(void* arg) {    // * para referenciar ponteiro na criaçao da thread
    while (jogo_ativo) {
        if (GetAsyncKeyState(VK_UP) & 0x8000) {  // retorna o estado em 16bits. 0x8000 verifica o mais significativo
            if (helicoptero_y > 4) { // limite superior
                apaga_helicoptero(helicoptero_x, helicoptero_y);
                helicoptero_y--;
            }
        }
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
            if (helicoptero_y < 19) { // limite inferior
                apaga_helicoptero(helicoptero_x, helicoptero_y);
                helicoptero_y++;
            }
        }
        desenha_helicoptero(helicoptero_x, helicoptero_y);
        Sleep(50); // melhora o movimento
    }
    return NULL;
}

void recarregar_helicoptero() {
    if (helicoptero_y < DEPOSITO_Y) {
        return;
    }
    pthread_mutex_lock(&deposito_mutex);
    
    if (deposito.misseis_disponiveis == 0) {
        pthread_mutex_unlock(&deposito_mutex);
        return;
    }

    for (int i = 0; i < n; i++) {
        if (deposito.slots[i] == 1) {
            deposito.slots[i] = 0;
            deposito.misseis_disponiveis--;

            pthread_mutex_lock(&misseis_mutex);
            misseis_hel++;
            pthread_mutex_unlock(&misseis_mutex);

            pthread_cond_signal(&deposito_cond); // notificar a thread caminhão
            atualizar_contador();
            break;
        }
    }

    pthread_mutex_unlock(&deposito_mutex);
    atualizar_contador_deposito();
}

void* dispara_missil(void* arg) {
    int missil_x = helicoptero_x + 1; 
    int missil_y = helicoptero_y;    

    while (missil_x < 79 && jogo_ativo) { 
        pthread_mutex_lock(&console_mutex);
        gotoxy(missil_x, missil_y);
        printf("%c", MISSILE_CHAR); 
        pthread_mutex_unlock(&console_mutex);

        Sleep(50); 

        pthread_mutex_lock(&console_mutex);
        gotoxy(missil_x, missil_y);
        printf(" ");
        pthread_mutex_unlock(&console_mutex);

        missil_x++;

        // colisão com dinossauros
        pthread_mutex_lock(&dinos_mutex);
        for (int i = 0; i < MAX_DINOS; i++) {
            if (dinos[i].ativo && missil_x == dinos[i].x && missil_y == dinos[i].y - 1) {
                dinos[i].vida--; 
                
                if (dinos[i].vida <= 0) {
                    dinos[i].ativo = 0;

                    pthread_mutex_unlock(&dinos_mutex);

                    pthread_mutex_lock(&console_mutex);
                    gotoxy(dinos[i].x, dinos[i].y);
                    printf(" ");
                    gotoxy(dinos[i].x, dinos[i].y - 1);
                    printf(" ");
                    pthread_mutex_unlock(&console_mutex);

                    return NULL;
                }

                pthread_mutex_unlock(&dinos_mutex);
                return NULL;
            }
        }
        pthread_mutex_unlock(&dinos_mutex);
    }

    return NULL;
}

void* abastece_deposito(void* arg) {
    while (jogo_ativo) {
        pthread_mutex_lock(&deposito_mutex);

        while (deposito.misseis_disponiveis == n) {
            pthread_cond_wait(&deposito_cond, &deposito_mutex);
        }

        for (int i = 0; i < n; i++) {
            if (deposito.slots[i] == 0) {
                deposito.slots[i] = 1;
                deposito.misseis_disponiveis++;

                pthread_mutex_unlock(&deposito_mutex);
                atualizar_contador_deposito();
                
                Sleep(2000);
                pthread_mutex_lock(&deposito_mutex);
                i = -1; // preenche os slots do inicio caso algum tenha sido utilizado
            }
        }

        pthread_mutex_unlock(&deposito_mutex);

        Sleep(5000);
    }
    return NULL;
}

void* movimenta_dino(void* arg) {
    int dino_id = *(int*)arg; // Recebe o índice do dinossauro ***
    free(arg);

    while (dinos[dino_id].x > 1 && jogo_ativo && dinos[dino_id].ativo) {
        pthread_mutex_lock(&console_mutex);
        gotoxy(dinos[dino_id].x, dinos[dino_id].y);
        printf("%c", DINO_CHAR);
        gotoxy(dinos[dino_id].x, dinos[dino_id].y - 1);
        printf("O");
        pthread_mutex_unlock(&console_mutex);

        Sleep(500);

        pthread_mutex_lock(&console_mutex);
        gotoxy(dinos[dino_id].x, dinos[dino_id].y);
        printf(" ");
        gotoxy(dinos[dino_id].x, dinos[dino_id].y - 1);
        printf(" ");
        pthread_mutex_unlock(&console_mutex);

        dinos[dino_id].x--;

        if (dinos[dino_id].x == helicoptero_x && dinos[dino_id].y == helicoptero_y) {
            jogo_ativo = 0;

            pthread_mutex_lock(&console_mutex);
            system("cls");
            gotoxy(10, 10);
            printf("Game Over! O helicoptero foi destruido.\n");
            pthread_mutex_unlock(&console_mutex);

            break;
        }
    }

    pthread_mutex_lock(&console_mutex);
    gotoxy(dinos[dino_id].x, dinos[dino_id].y);
    printf(" ");
    gotoxy(dinos[dino_id].x, dinos[dino_id].y - 1);
    printf(" ");
    pthread_mutex_unlock(&console_mutex);

    pthread_mutex_lock(&dinos_mutex);
    dinos[dino_id].ativo = 0;
    num_dinos--;
    pthread_mutex_unlock(&dinos_mutex);



    return NULL;
}

// thread para gerenciar dinos separada do loop do jogo para ter sleep diferente
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
                do {
                    dinos[slot].y = (rand() % 15) + 4; // Gera posição inicial
                } while (dinos[slot].y >= DEPOSITO_Y);
                dinos[slot].vida = m;


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

                verificar_game_over();
            }
        } else {
            pthread_mutex_unlock(&dinos_mutex);
        }

        Sleep(t); // Controle do intervalo de criação
    }
    return NULL;
}

// Função principal
int main() {

    desenha_menu();
    int escolha;
    scanf("%d", &escolha);
    inicializar_dificuldade(escolha);

    deposito.slots = (int*)malloc(n * sizeof(int));
    if (!deposito.slots) {
        fprintf(stderr, "Erro ao alocar memória para os slots do depósito\n");
        exit(EXIT_FAILURE);
    }

    // Inicializa o depósito
    for (int i = 0; i < n; i++) {
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
    atualizar_contador(); 

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
            if (misseis_hel > 0 && helicoptero_y < DEPOSITO_Y) {
                misseis_hel--;
                pthread_mutex_unlock(&misseis_mutex);
                atualizar_contador(); 

                pthread_t thread_missil;
                pthread_create(&thread_missil, NULL, dispara_missil, NULL);
                pthread_detach(thread_missil);
            } else {
                pthread_mutex_unlock(&misseis_mutex);
                recarregar_helicoptero();
            }
            Sleep(500);
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

    free(deposito.slots);
    return 0;
}
