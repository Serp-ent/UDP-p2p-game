#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define REPLY_MAX 32

typedef struct {
    int czyMojaTura;
    int gracz1_wynik;
    int gracz2_wynik;
    int currentNumber;
} Game;

void recvGameInfo(Game* gra) {
    while (8) {
        // TODO: kiedy user wpisze koniec zakoncz ten process
        // TODO: recvfrom messages and update our game
        // WARNING: user moze wyslac informacje o zakonczeniu gry
        if (gra->czyMojaTura == -1) {
            // to zacznij czeakc na innego usera
        }
        printf("\nOtrzymano jakas wiadomosc\n");
        sleep(3);
    }
}

void userInteraction(Game* gra) {
    char reply[REPLY_MAX];
    while (8) {
        printf("> ");
        scanf(" %31s", reply);
        while (getchar() != '\n')
            ;

        if (strcmp("koniec", reply) == 0) {
            // TODO: inform second about disconnecting
            break;
        } else if (strcmp("wynik", reply) == 0) {
            printf("Ty %d : %d %s\n", gra->gracz1_wynik, gra->gracz2_wynik,
                   "<nick przeciwnika>");
        } else {
            int nowa_wartosc = atoi(reply);
            if (!gra->czyMojaTura) {
                printf("Teraz tura gracza %s, poczekaj na swoja kolej\n",
                       "<nick przeciwnika>");
                continue;
            }

            if ((nowa_wartosc - gra->currentNumber) < 1 ||
                (nowa_wartosc - gra->currentNumber) > 10) {
                printf("Takiej wartosci nie mozesz wybrac!\n");
                continue;
            }

            gra->currentNumber = nowa_wartosc;
            gra->czyMojaTura = 0;

            //
            // wypisz info wygrana czy przegrana jezeli mozna
        }
    }
}

int main(int argc, char* argv[]) {
    pid_t pid;
    if (argc != 3) {
        fprintf(stderr, "Prosze podac dwa argumenty\n");
        exit(1);
    }

    // acknowledge connection
    // // wyslij propozycje gry
    // czekaj az ktos sie polaczy i rozpocznij z nim gre gra sz
    Game* gra = calloc(1, sizeof(*gra));
    gra->gracz1_wynik = gra->gracz2_wynik = 0;
    gra->czyMojaTura = 1;
    gra->currentNumber = getpid() % 10 + 1;  // TODO: improve randomnes

    // TODO: watchout for zombie process
    pid = fork();
    if (pid == 0) {
        recvGameInfo(gra);
        exit(0);
    } else if (pid > 0) {
        userInteraction(gra);
        wait(NULL);
    } else {
        perror("Nie mozna utowrzyc potomka fork()");
        exit(1);
    }
    // argv[1] host to zaproszenia
    // argv[2] port
    // argv[3] opcjonalny nick
}
