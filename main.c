#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define REPLY_MAX 32
#define NAZWA_MAX 32

#define PORT 5555

typedef struct {
    int kogo_tura;
    int gracz1_wynik;
    int gracz2_wynik;
    int currentNumber;
    int czyKoniecGry;
} Pakiet;

typedef struct {
    int ktory_gracz;
    Pakiet gra;

    char enemy_name[NAZWA_MAX];
    char moja_nazwa[NAZWA_MAX];

    struct sockaddr_in clientaddr;
    socklen_t clientlen;
} Gra;

typedef struct UsciskDloni {
    char is_server;
    char nick[NAZWA_MAX];
} UsciskDloni;

void przygotuj_gre(Gra* gra);

void ustal_polaczenie(int sockfd, UsciskDloni* ack, Gra* gra,
                      int wiem_ze_jestem_serwerem) {
    if (wiem_ze_jestem_serwerem) {
        recvfrom(sockfd, ack, sizeof(*ack), 0,
                 (struct sockaddr*)&gra->clientaddr, &gra->clientlen);
        ack->is_server = 1;
        strcpy(gra->enemy_name, ack->nick);  // save enemy nickname

        strcpy(ack->nick, gra->moja_nazwa);
        sendto(sockfd, ack, sizeof(*ack), 0, (struct sockaddr*)&gra->clientaddr,
               sizeof(gra->clientaddr));

        przygotuj_gre(gra);
        gra->ktory_gracz = 1;  // serwer czyli 1

    } else {
        strcpy(ack->nick, gra->moja_nazwa);
        ack->is_server = 0;

        sendto(sockfd, ack, sizeof(*ack), 0, (struct sockaddr*)&gra->clientaddr,
               sizeof(gra->clientaddr));
        printf("Propozycja gry wyslana.\n");

        recvfrom(sockfd, ack, sizeof(*ack), 0,
                 (struct sockaddr*)&gra->clientaddr, &gra->clientlen);

        if (!ack->is_server) {  // host mowi ze nie jest serwerem
            // wiec to ten host powinien byc serwerem
            ack->is_server = 1;
            strcpy(gra->enemy_name, ack->nick);  // save enemy nickname

            strcpy(ack->nick, gra->moja_nazwa);

            sendto(sockfd, ack, sizeof(*ack), 0,
                   (struct sockaddr*)&gra->clientaddr, sizeof(gra->clientaddr));
        } else {
            // wiec ten host powinen byc klientem
            ack->is_server = 0;
            strcpy(gra->enemy_name, ack->nick);
        }
    }

    printf("%s dolaczyl do gry.\n", inet_ntoa(gra->clientaddr.sin_addr));
}

void przygotuj_gre(Gra* gra) {
    gra->gra.gracz1_wynik = gra->gra.gracz2_wynik = 0;
    gra->gra.currentNumber = rand() % 10 + 1;
    gra->gra.czyKoniecGry = 0;
    gra->gra.kogo_tura = 1;
}

void gra_ustaw_na_hosta(Gra* gra) {
    gra->gra.gracz1_wynik = ntohl(gra->gra.gracz1_wynik);
    gra->gra.gracz2_wynik = ntohl(gra->gra.gracz2_wynik);
    gra->gra.currentNumber = ntohl(gra->gra.currentNumber);
    gra->gra.kogo_tura = ntohl(gra->gra.kogo_tura);
}

void gra_ustaw_do_wyslania(Gra* gra) {
    gra->gra.kogo_tura = htonl(gra->gra.kogo_tura);
    gra->gra.gracz1_wynik = htonl(gra->gra.gracz1_wynik);
    gra->gra.gracz2_wynik = htonl(gra->gra.gracz2_wynik);
    gra->gra.currentNumber = htonl(gra->gra.currentNumber);
}

void recvGameInfo(int sockfd, Gra* gra, UsciskDloni* ack) {
    while (8) {
        recvfrom(sockfd, &gra->gra, sizeof(Pakiet), 0,
                 (struct sockaddr*)&gra->clientaddr, &gra->clientlen);

        if (gra->gra.czyKoniecGry) {
            printf("\n%s zakonczyl gre, mozesz poczekac na kolejnego gracza.\n",
                   gra->enemy_name);

            // wiemy ze teraz to my bedziemy serwerem
            ustal_polaczenie(sockfd, ack, gra, 1);

            printf("Losowa wartosc poczatkowa: %d, podaj kolejna wartosc\n",
                   gra->gra.currentNumber);
            continue;
        }

        gra_ustaw_na_hosta(gra);
        printf("\n%s podal wartosc %d", gra->enemy_name,
               gra->gra.currentNumber);

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

void userInteraction(int sockfd, Gra* gra) {
    char reply[REPLY_MAX];
    while (8) {
        printf("> ");
        scanf(" %31s", reply);
        while (getchar() != '\n')
            ;

        if (strcmp("koniec", reply) == 0) {
            gra->gra.czyKoniecGry = 1;
            if (sendto(sockfd, &gra->gra, sizeof(Pakiet), 0,
                       (struct sockaddr*)&gra->clientaddr,
                       gra->clientlen) == -1) {
                perror("sendto");
            }

            break;  // przejdz do czyszczenia
        } else if (strcmp("wynik", reply) == 0) {
            if (gra->gra.czyKoniecGry) {
                printf("Brak gry w trakcie\n");
                continue;
            }

            if (gra->ktory_gracz == 1) {
                printf("Ty %d : %d %s\n", gra->gra.gracz1_wynik,
                       gra->gra.gracz2_wynik, gra->enemy_name);
            } else {
                printf("Ty %d : %d %s\n", gra->gra.gracz2_wynik,
                       gra->gra.gracz1_wynik, gra->enemy_name);
            }
        } else {
            if (gra->gra.czyKoniecGry) {
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

            gra->gra.kogo_tura = (gra->gra.kogo_tura == 1) ? 2 : 1;
            gra_ustaw_do_wyslania(gra);

            if (sendto(sockfd, &gra->gra, sizeof(Pakiet), 0,
                       (struct sockaddr*)&gra->clientaddr,
                       gra->clientlen) == -1) {
                perror("sendto");
            }

            gra_ustaw_na_hosta(gra);
        }
    }
}
int przygotuj_socket(char* domena, char* port) {
    int status;
    struct addrinfo hints;
    struct addrinfo *results, *p;
    int sockfd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    status = getaddrinfo(domena, port, &hints, &results);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }

    for (p = results; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;

        break;
    }

    if (p == NULL) {  // zaden adres nie byl dobry
        fprintf(stderr, "Nie mozna utowrzyc socketu\n");
        freeaddrinfo(results);
        return -1;
    }
    freeaddrinfo(results);

    return sockfd;
}

void fillWithMyIP(int sockfd, char* buf) {
    struct sockaddr_in addr;
    socklen_t len;

    getsockname(sockfd, (struct sockaddr*)&addr, &len);

    strcpy(buf, inet_ntoa(addr.sin_addr));
}

int shmid;

void posprzataj(int sig) {
    shmctl(shmid, IPC_RMID, NULL);
    exit(0);
}

int main(int argc, char* argv[]) {
    pid_t pid;
    struct sockaddr_in my_addr;
    int sockfd;
    key_t key;
    Gra* gra;

    struct UsciskDloni ack;

    srand(time(NULL));

    if (argc != 4 && argc != 3) {
        fprintf(stderr, "uzycie: %s: <ip> <port> [ nazwa uzytkonika ]\n",
                argv[0]);
        exit(1);
    }

    key = ftok("main.c", 'a');
    if (key == -1) {
        perror("Nie mozna wygenerowac klucza");
        exit(1);
    }

    shmid = shmget(key, sizeof(Gra), 0644 | IPC_CREAT);
    if (shmid == -1) {
        perror("Nie mozna utworzyc pamieci wspoldzielonej");
        exit(1);
    }
    signal(SIGINT, posprzataj);

    gra = shmat(shmid, NULL, 0);
    if (gra == (void*)-1) {
        perror("shmat()");
        exit(1);
    }

    sockfd = przygotuj_socket(argv[1], argv[2]);
    if (sockfd == -1) {
        shmctl(shmid, IPC_RMID, NULL);
        exit(1);
    }

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // dowolny (moj) interfejs
    if (bind(sockfd, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1) {
        shmctl(shmid, IPC_RMID, NULL);

        perror("Nie mozna powiazac socketu z portem");
        exit(1);
    }

    memset(&gra->clientaddr, 0, sizeof(gra->clientaddr));
    gra->clientaddr.sin_family = AF_INET;
    gra->clientaddr.sin_port = htons((u_short)atoi(argv[2]));
    inet_aton(argv[1], &gra->clientaddr.sin_addr);

    printf("Gra w 50, Wersja A.\n");
    printf("Rozpoczynam gre z %s. ", argv[1]);
    printf(
        "Napisz \"koniec\" by zakonczyc lub "
        "\"wynik\" by wyswietlic aktualny\n");

    if (argc == 4) {
        strcpy(gra->moja_nazwa, argv[3]);
    } else {
        fillWithMyIP(sockfd, gra->moja_nazwa);
    }

    ustal_polaczenie(sockfd, &ack, gra, 0);

    przygotuj_gre(gra);
    if (ack.is_server) {
        // to serwer ma poprawnie utworzyc gre
        printf("Losowa wartosc poczatkowa: %d, podaj kolejna wartosc\n",
               gra->gra.currentNumber);
        gra->ktory_gracz = 1;
    } else {
        gra->ktory_gracz = 2;
    }

    pid = fork();
    if (pid == 0) {
        recvGameInfo(sockfd, gra, &ack);
        close(sockfd);
        exit(0);
    } else if (pid > 0) {
        userInteraction(sockfd, gra);
        kill(pid, SIGTERM);

        waitpid(pid, NULL, 0);

        close(sockfd);
        shmdt(gra);
        shmctl(shmid, IPC_RMID, NULL);
    } else {
        perror("Nie mozna utowrzyc potomka fork()");
        exit(1);
    }
}
