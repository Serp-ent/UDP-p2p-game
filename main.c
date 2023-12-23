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
} Game;

typedef struct Umowa {
    char is_server;
    char nick[NAZWA_MAX];
} Umowa;

void recvGameInfo(int sockfd, Game* gra, struct sockaddr_in* clientaddr,
                  socklen_t* clientlen, const char* enemy_name,
                  int ktory_gracz) {
    while (8) {
        recvfrom(sockfd, gra, sizeof(Game), 0, (struct sockaddr*)clientaddr,
                 clientlen);

        gra->gracz1_wynik = ntohl(gra->gracz1_wynik);
        gra->gracz2_wynik = ntohl(gra->gracz2_wynik);
        gra->currentNumber = ntohl(gra->currentNumber);
        gra->kogo_tura = ntohl(gra->kogo_tura);
        printf("\n%s podal wartosc %d", enemy_name, gra->currentNumber);

        if (gra->currentNumber == 50) {
            printf("\nPrzegrana!\n");

            printf("Zaczynamy kolejna rozgrywke\n");
            gra->currentNumber = getpid() % 10 + 1;
            printf("Losowa wartosc poczatkowa %d, podaj kolejna wartosc\n",
                   gra->currentNumber);
            continue;  // zacznij nowa rozgrywke
        }

        printf(", podaj kolejna wartosc.\n");
    }
}

void userInteraction(int sockfd, Game* gra, struct sockaddr_in* clientaddr,
                     socklen_t clientlen, const char* enemy_name,
                     int ktory_gracz) {
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
            if (ktory_gracz == 1) {
                printf("Ty %d : %d %s\n", gra->gracz1_wynik, gra->gracz2_wynik,
                       enemy_name);
            } else {
                printf("Ty %d : %d %s\n", gra->gracz2_wynik, gra->gracz1_wynik,
                       enemy_name);
            }
        } else {
            int nowa_wartosc = atoi(reply);
            if (gra->kogo_tura != ktory_gracz) {
                printf("Teraz tura gracza %s, poczekaj na swoja kolej\n",
                       enemy_name);
                continue;
            }

            if ((nowa_wartosc - gra->currentNumber) < 1 ||
                (nowa_wartosc - gra->currentNumber) > 10) {
                printf("Takiej wartosci nie mozesz wybrac!\n");
                continue;
            }

            gra->currentNumber = nowa_wartosc;
            if (gra->currentNumber == 50) {
                printf("Wygrana!\n");
                if (ktory_gracz == 1) {
                    ++gra->gracz1_wynik;
                } else {
                    ++gra->gracz2_wynik;
                }
                printf(
                    "Zaczynamy kolejna rozgrywke., poczekaj na swoja kolej\n");
            }

            gra->kogo_tura = htonl((gra->kogo_tura == 1) ? 2 : 1);
            gra->gracz1_wynik = htonl(gra->gracz1_wynik);
            gra->gracz2_wynik = htonl(gra->gracz2_wynik);
            gra->currentNumber = htonl(gra->currentNumber);

            if (sendto(sockfd, gra, sizeof(Game), 0,
                       (struct sockaddr*)clientaddr, clientlen) == -1) {
                perror("sendto");
            }
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

    int ktory_gracz;

    // TODO: BUG: error handling
    if (argc != 5) {
        fprintf(stderr, "Prosze podac cztery argumenty\n");
        exit(1);
    }

    key_t key;
    key = ftok("main.c", 'a');
    shmid = shmget(key, sizeof(Game), 0644 | IPC_CREAT);
    // TODO: shmdt and cleanup
    Game* gra = shmat(shmid, NULL, 0);
    if (gra == (void*)-1) {
        perror("shmat()");
        exit(1);
    }

    // TODO: getaddrinfo():
    if (strcmp(argv[4], "-s") == 0) {
        port = 30000 + getpid() % 10000 + 1;
    } else {
        port = 30000;
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
    // begin acknowledge connection
    strcpy(ack.nick, argv[3]);
    ack.is_server = 0;

    sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr*)&clientaddr,
           sizeof(clientaddr));
    recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr*)&clientaddr,
             &clientlen);

    if (!ack.is_server) {  // host mowi ze nie jest serwerem
        // wiec to ten host powinien byc serwerem
        ack.is_server = 1;
        strcpy(ack.nick, argv[3]);

        sendto(sockfd, argv[3], sizeof(ack), 0, (struct sockaddr*)&clientaddr,
               sizeof(clientaddr));
    } else {
        // wiec ten host powinen byc klientem
        ack.is_server = 0;
    }

    printf("Gra rozpoczeta z %s:%d\n", inet_ntoa(clientaddr.sin_addr),
           ntohs(clientaddr.sin_port));
    // end acknowledge

    gra->kogo_tura = 1;
    if (ack.is_server) {
        // to serwer ma poprawnie utworzyc gre
        gra->gracz1_wynik = gra->gracz2_wynik = 0;
        gra->currentNumber = getpid() % 10 + 1;  // TODO: improve randomnes
        ktory_gracz = 1;

        printf("Losowa wartosc poczatkowa: %d, podaj kolejna wartosc\n",
               gra->currentNumber);

    } else {
        ktory_gracz = 2;
        printf("Czekam zeby dostac wartosci od serwera\n");
    }

    pid = fork();
    if (pid == 0) {
        recvGameInfo(sockfd, gra, &clientaddr, &clientlen, ack.nick,
                     ktory_gracz);
        exit(0);
    } else if (pid > 0) {
        userInteraction(sockfd, gra, &clientaddr, clientlen, ack.nick,
                        ktory_gracz);
        kill(pid, SIGTERM);

        waitpid(pid, NULL, 0);

        shmdt(gra);
        shmctl(shmid, IPC_RMID, NULL);
    } else {
        perror("Nie mozna utowrzyc potomka fork()");
        exit(1);
    }
}
