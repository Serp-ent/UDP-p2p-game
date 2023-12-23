#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define REPLY_MAX 32
#define NAZWA_MAX 32

typedef struct {
    int kogo_tura;
    int gracz1_wynik;
    int gracz2_wynik;
    int currentNumber;
    int isGameEnd;
} GraPakiet;

typedef struct {
    int ktory_gracz;
    GraPakiet gra;
    char enemy_name[NAZWA_MAX];
} Gra;

typedef struct Umowa {
    char is_server;
    char nick[NAZWA_MAX];
} Umowa;

void recvGameInfo(int sockfd, Gra* gra, struct sockaddr_in* clientaddr,
                  socklen_t* clientlen, Umowa* ack, const char* moj_nick) {
    while (8) {
        recvfrom(sockfd, &gra->gra, sizeof(GraPakiet), 0,
                 (struct sockaddr*)clientaddr, clientlen);

        if (gra->gra.isGameEnd) {
            printf("\n%s zakonczyl gre, mozesz poczekac na kolejnego gracza.\n",
                   gra->enemy_name);

            // wiemy ze teraz to my bedziemy serwerem

            recvfrom(sockfd, ack, sizeof(*ack), 0, (struct sockaddr*)clientaddr,
                     clientlen);
            ack->is_server = 1;
            strcpy(gra->enemy_name, ack->nick);  // save enemy nickname

            strcpy(ack->nick, moj_nick);
            sendto(sockfd, ack, sizeof(*ack), 0, (struct sockaddr*)clientaddr,
                   sizeof(*clientaddr));

            gra->gra.gracz1_wynik = gra->gra.gracz2_wynik = 0;
            // TODO: improve randomnes
            gra->gra.currentNumber = getpid() % 10 + 1;
            gra->gra.isGameEnd = 0;
            gra->ktory_gracz = 1;
            gra->gra.kogo_tura = 1;

            printf("%s dolaczyl do gry.\n", inet_ntoa(clientaddr->sin_addr));

            printf("Losowa wartosc poczatkowa: %d, podaj kolejna wartosc\n",
                   gra->gra.currentNumber);
            continue;
        }

        gra->gra.gracz1_wynik = ntohl(gra->gra.gracz1_wynik);
        gra->gra.gracz2_wynik = ntohl(gra->gra.gracz2_wynik);
        gra->gra.currentNumber = ntohl(gra->gra.currentNumber);
        gra->gra.kogo_tura = ntohl(gra->gra.kogo_tura);
        printf("\n%s podal wartosc %d", ack->nick, gra->gra.currentNumber);

        if (gra->gra.currentNumber == 50) {
            printf("\nPrzegrana!\n");

            printf("Zaczynamy kolejna rozgrywke\n");
            gra->gra.currentNumber = getpid() % 10 + 1;
            printf("Losowa wartosc poczatkowa %d, podaj kolejna wartosc\n",
                   gra->gra.currentNumber);
            continue;  // zacznij nowa rozgrywke
        }

        printf(", podaj kolejna wartosc.\n");
    }
}

void userInteraction(int sockfd, Gra* gra, struct sockaddr_in* clientaddr,
                     socklen_t clientlen) {
    char reply[REPLY_MAX];
    while (8) {
        printf("(ktory_gracz = %d)> ", gra->ktory_gracz);
        scanf(" %31s", reply);
        while (getchar() != '\n')
            ;

        if (strcmp("koniec", reply) == 0) {
            gra->gra.isGameEnd = 1;
            if (sendto(sockfd, &gra->gra, sizeof(GraPakiet), 0,
                       (struct sockaddr*)clientaddr, clientlen) == -1) {
                perror("sendto");
            }

            break;  // przejdz do czyszczenia
        } else if (strcmp("wynik", reply) == 0) {
            if (gra->gra.isGameEnd) {
                printf("Brak gry w trakcie\n");
                continue;
            }

            // TODO: user that turn currently is have points displayed in
            // network byte order e.g. instead of 1 : 1 he have 16777216
            if (gra->ktory_gracz == 1) {
                printf("Ty %d : %d %s\n", gra->gra.gracz1_wynik,
                       gra->gra.gracz2_wynik, gra->enemy_name);
            } else {
                printf("Ty %d : %d %s\n", gra->gra.gracz2_wynik,
                       gra->gra.gracz1_wynik, gra->enemy_name);
            }
        } else {
            if (gra->gra.isGameEnd) {
                printf("Brak gry w trakcie\n");
                continue;
            }

            int nowa_wartosc = atoi(reply);
            if (gra->gra.kogo_tura != gra->ktory_gracz) {
                printf("Teraz tura gracza %s, poczekaj na swoja kolej\n",
                       gra->enemy_name);
                continue;
            }

            if ((nowa_wartosc - gra->gra.currentNumber) < 1 ||
                (nowa_wartosc - gra->gra.currentNumber) > 10 ||
                nowa_wartosc > 50) {
                printf("Takiej wartosci nie mozesz wybrac!\n");
                continue;
            }

            gra->gra.currentNumber = nowa_wartosc;
            if (gra->gra.currentNumber == 50) {
                printf("Wygrana!\n");
                if (gra->ktory_gracz == 1) {
                    ++gra->gra.gracz1_wynik;
                } else {
                    ++gra->gra.gracz2_wynik;
                }
                printf(
                    "Zaczynamy kolejna rozgrywke., poczekaj na swoja kolej\n");
            }

            gra->gra.kogo_tura = htonl((gra->gra.kogo_tura == 1) ? 2 : 1);
            gra->gra.gracz1_wynik = htonl(gra->gra.gracz1_wynik);
            gra->gra.gracz2_wynik = htonl(gra->gra.gracz2_wynik);
            gra->gra.currentNumber = htonl(gra->gra.currentNumber);

            if (sendto(sockfd, &gra->gra, sizeof(GraPakiet), 0,
                       (struct sockaddr*)clientaddr, clientlen) == -1) {
                perror("sendto");
            }

            // change local game data to host byte order for case user enters
            // 'wynik'
            gra->gra.kogo_tura = ntohl(gra->gra.kogo_tura);
            gra->gra.gracz1_wynik = ntohl(gra->gra.gracz1_wynik);
            gra->gra.gracz2_wynik = ntohl(gra->gra.gracz2_wynik);
            gra->gra.currentNumber = ntohl(gra->gra.currentNumber);
        }
    }
}

int shmid;

int main(int argc, char* argv[]) {
    pid_t pid;
    struct sockaddr_in my_addr;
    int sockfd;
    int port;

    struct sockaddr_in clientaddr;
    socklen_t clientlen;

    struct Umowa ack;

    // TODO: BUG: error handling
    if (argc != 5) {
        fprintf(stderr, "Prosze podac cztery argumenty\n");
        exit(1);
    }

    key_t key;
    Gra* gra;

    // TODO: getaddrinfo():
    if (strcmp(argv[4], "-s") == 0) {
        key = ftok("main.c", 'a');
        port = 30000 + 1;
    } else {
        key = ftok("main.c", 'b');
        port = 30000;
    }

    shmid = shmget(key, sizeof(Gra), 0644 | IPC_CREAT);
    gra = shmat(shmid, NULL, 0);
    if (gra == (void*)-1) {
        perror("shmat()");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // dowolny (moj) interfejs
    bind(sockfd, (struct sockaddr*)&my_addr, sizeof(my_addr));

    memset(&clientaddr, 0, sizeof(clientaddr));
    clientaddr.sin_family = AF_INET;
    clientaddr.sin_port = htons((u_short)atoi(argv[2]));
    inet_aton(argv[1], &clientaddr.sin_addr);

    printf("Gra w 50, Wersja A. (PORT: %d)\n", port);
    // TODO: dns look up
    printf("Rozpoczynam gre z %s. ", argv[1]);
    printf(
        "Napisz \"koniec\" by zakonczyc lub "
        "\"wynik\" by wyswietlic aktualny\n");

    // begin acknowledge connection
    strcpy(ack.nick, argv[3]);
    ack.is_server = 0;

    sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr*)&clientaddr,
           sizeof(clientaddr));
    printf("Propozycja gry wyslana.\n");

    recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr*)&clientaddr,
             &clientlen);

    if (!ack.is_server) {  // host mowi ze nie jest serwerem
        // wiec to ten host powinien byc serwerem
        ack.is_server = 1;
        strcpy(gra->enemy_name, ack.nick);  // save enemy nickname

        strcpy(ack.nick, argv[3]);

        sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr*)&clientaddr,
               sizeof(clientaddr));
    } else {
        // wiec ten host powinen byc klientem
        ack.is_server = 0;
        strcpy(gra->enemy_name, ack.nick);
    }

    printf("%s dolaczyl do gry.\n", inet_ntoa(clientaddr.sin_addr));
    // end acknowledge

    gra->gra.kogo_tura = 1;
    gra->gra.isGameEnd = 0;
    if (ack.is_server) {
        // to serwer ma poprawnie utworzyc gre
        gra->gra.gracz1_wynik = gra->gra.gracz2_wynik = 0;
        gra->gra.currentNumber = getpid() % 10 + 1;  // TODO: improve randomness
        gra->ktory_gracz = 1;

        printf("Losowa wartosc poczatkowa: %d, podaj kolejna wartosc\n",
               gra->gra.currentNumber);

    } else {
        gra->ktory_gracz = 2;
    }

    pid = fork();
    if (pid == 0) {
        recvGameInfo(sockfd, gra, &clientaddr, &clientlen, &ack, argv[3]);
        exit(0);
    } else if (pid > 0) {
        userInteraction(sockfd, gra, &clientaddr, clientlen);
        kill(pid, SIGTERM);

        waitpid(pid, NULL, 0);

        shmdt(gra);
        shmctl(shmid, IPC_RMID, NULL);
    } else {
        perror("Nie mozna utowrzyc potomka fork()");
        exit(1);
    }
}
