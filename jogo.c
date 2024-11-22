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

// Variáveis globais
int helicoptero_x = HELICOPTER_START_X;
int helicoptero_y = HELICOPTER_START_Y;
int jogo_ativo = 1; // Controla o loop principal do jogo
int num_dinos = 0; // Conta o número de dinossauros ativos

// Mutex para controlar acesso ao console
pthread_mutex_t console_mutex;

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

// Função executada pelas threads de mísseis
void* dispara_missil(void* arg) {
    int missil_x = helicoptero_x + 1;
    int missil_y = helicoptero_y;

    while (missil_x < 79) { // Limite do cenário
        pthread_mutex_lock(&console_mutex);
        gotoxy(missil_x, missil_y);
        printf("%c", MISSILE_CHAR);
        pthread_mutex_unlock(&console_mutex);

        Sleep(50); // Pausa para criar movimento suave

        pthread_mutex_lock(&console_mutex);
        gotoxy(missil_x, missil_y);
        printf(" "); // Apaga o míssil da posição anterior
        pthread_mutex_unlock(&console_mutex);

        missil_x++;

    }
    return NULL;
}

// Função executada pelas threads de dinossauros
void* movimenta_dino(void* arg) {
    int dino_x = 78;                  // Dinossauro começa no lado direito
    int dino_y = (rand() % 15) + 4;   // Posição vertical aleatória

    while (dino_x > 1 && jogo_ativo) { // Enquanto o dinossauro não sair do cenário
        pthread_mutex_lock(&console_mutex);
        gotoxy(dino_x, dino_y);
        printf("%c", DINO_CHAR);
        pthread_mutex_unlock(&console_mutex);

        Sleep(200); // Movimento lento do dinossauro (talvez usar uma variavel dps)

        pthread_mutex_lock(&console_mutex); 
        gotoxy(dino_x, dino_y);
        printf(" "); // Apaga o dinossauro da posição anterior
        pthread_mutex_unlock(&console_mutex); 

        dino_x--;
    }

    pthread_mutex_lock(&console_mutex);
    num_dinos--; // Reduz o número de dinossauros ativos
    pthread_mutex_unlock(&console_mutex); 

    return NULL;
}

// Função principal
int main() {

    // Inicializa o mutex
    pthread_mutex_init(&console_mutex, NULL);

    // Redimensiona o console para 80 colunas e 25 linhas
    setConsoleSize(80, 25);

    desenha_cenario();

    pthread_t thread_helicoptero;
    pthread_create(&thread_helicoptero, NULL, movimenta_helicoptero, NULL); // Entender melhor os parametros aqui

    srand(time(NULL)); // Inicializa o gerador de números aleatórios

    // Loop principal do jogo
    while (jogo_ativo) {

        if (num_dinos < MAX_DINOS) {
            pthread_t thread_dino;
            pthread_create(&thread_dino, NULL, movimenta_dino, NULL);
            pthread_detach(thread_dino); // Deixa o dinossauro rodar de forma independente *****

            pthread_mutex_lock(&console_mutex); // Proteger variavel global (alterada na funcao movimenta_dino tbm)
            num_dinos++;
            pthread_mutex_unlock(&console_mutex);
        }

        if (GetAsyncKeyState(VK_SPACE) & 0x8000) { // Tecla "Espaço" *****
            pthread_t thread_missil;
            pthread_create(&thread_missil, NULL, dispara_missil, NULL);
            pthread_detach(thread_missil); // Deixa o míssil rodar de forma independente *****
            Sleep(200); // Evita múltiplos disparos com um único pressionamento
        }
        Sleep(50); // Suaviza o loop principal
    }

    // Espera a thread do helicóptero finalizar ****
    pthread_join(thread_helicoptero, NULL);

    // Destroi o mutex
    pthread_mutex_destroy(&console_mutex);

    return 0;
}
