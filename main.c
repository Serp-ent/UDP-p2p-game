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

struct Pakiet {
    int gracz1_wynik;
    int gracz2_wynik;

    int currentNumber;
    char kogo_tura;
    char czyKoniecGry;
    char ktoZaczynal;
};

struct Gra {
    int ktory_gracz;
    struct Pakiet gra;

    char enemy_name[NAZWA_MAX];
    char moja_nazwa[NAZWA_MAX];

    struct sockaddr_in clientaddr;
    socklen_t clientlen;
};

struct UsciskDloni {
    char is_server;
    char nick[NAZWA_MAX];
};

void przygotuj_gre(struct Gra* gra) {
    gra->gra.gracz1_wynik = gra->gra.gracz2_wynik = 0;
    gra->gra.currentNumber = rand() % 10 + 1;

    gra->gra.czyKoniecGry = 0;

    gra->gra.kogo_tura = 1;
    gra->gra.ktoZaczynal = 1;
}

void ustaw_nick_przeciwnika(struct Gra* gra, char* nick) {
    if (strcmp("", nick) == 0) {
        strcpy(gra->enemy_name, inet_ntoa(gra->clientaddr.sin_addr));
    } else {
        strcpy(gra->enemy_name, nick);
    }
}

void ustal_polaczenie(int sockfd, struct UsciskDloni* ack, struct Gra* gra,
                      int wiem_ze_jestem_serwerem) {
    if (wiem_ze_jestem_serwerem) {
        recvfrom(sockfd, ack, sizeof(*ack), 0,
                 (struct sockaddr*)&gra->clientaddr, &gra->clientlen);
        ustaw_nick_przeciwnika(gra, ack->nick);

        ack->is_server = 1;
        strcpy(ack->nick, gra->moja_nazwa);
        sendto(sockfd, ack, sizeof(*ack), 0, (struct sockaddr*)&gra->clientaddr,
               sizeof(gra->clientaddr));
        przygotuj_gre(gra);
        // taka nasza konwencja ze serwer to gracz 1 i to on zaczyna nowa gre
        gra->ktory_gracz = 1;  // serwer czyli 1

    } else {
        strcpy(ack->nick, gra->moja_nazwa);
        ack->is_server = 0;  // na poczatku zalkadamy ze nie jestesmy serwerem
        sendto(sockfd, ack, sizeof(*ack), 0, (struct sockaddr*)&gra->clientaddr,
               sizeof(gra->clientaddr));
        printf("Propozycja gry wyslana.\n");

        recvfrom(sockfd, ack, sizeof(*ack), 0,
                 (struct sockaddr*)&gra->clientaddr, &gra->clientlen);
        ustaw_nick_przeciwnika(gra, ack->nick);

        if (!ack->is_server) {  // host mowi ze nie jest serwerem
            ack->is_server = 1;
            strcpy(ack->nick, gra->moja_nazwa);
            sendto(sockfd, ack, sizeof(*ack), 0,
                   (struct sockaddr*)&gra->clientaddr, sizeof(gra->clientaddr));
        } else {
            ack->is_server = 0;
        }
    }

    printf("%s dolaczyl do gry.\n", inet_ntoa(gra->clientaddr.sin_addr));
}

void gra_ustaw_na_hosta(struct Gra* gra) {
    gra->gra.gracz1_wynik = ntohl(gra->gra.gracz1_wynik);
    gra->gra.gracz2_wynik = ntohl(gra->gra.gracz2_wynik);
    gra->gra.currentNumber = ntohl(gra->gra.currentNumber);
}

void gra_ustaw_do_wyslania(struct Gra* gra) {
    gra->gra.gracz1_wynik = htonl(gra->gra.gracz1_wynik);
    gra->gra.gracz2_wynik = htonl(gra->gra.gracz2_wynik);
    gra->gra.currentNumber = htonl(gra->gra.currentNumber);
}

void czekaj_na_pakiet(int sockfd, struct Gra* gra, struct UsciskDloni* ack) {
    while (8) {
        recvfrom(sockfd, &gra->gra, sizeof(struct Pakiet), 0,
                 (struct sockaddr*)&gra->clientaddr, &gra->clientlen);

        if (gra->gra.czyKoniecGry) {
            printf("\n%s zakonczyl gre, mozesz poczekac na kolejnego gracza.\n",
                   gra->enemy_name);

            ustal_polaczenie(sockfd, ack, gra, 1);

            printf("Losowa wartosc poczatkowa: %d, podaj kolejna wartosc\n",
                   gra->gra.currentNumber);
            continue;  // zacznij nasluchiwac na nowe pakietu
        }

        gra_ustaw_na_hosta(gra);  // przygotuj wartosci do wyslania
        printf("\n%s podal wartosc %d", gra->enemy_name,
               gra->gra.currentNumber);

        if (gra->gra.currentNumber == 50) {
            printf("\nPrzegrana!\n");

            printf("Zaczynamy kolejna rozgrywke");

            if (gra->gra.ktoZaczynal != gra->ktory_gracz) {
                gra->gra.ktoZaczynal = gra->ktory_gracz;
                gra->gra.kogo_tura = gra->ktory_gracz;

                gra->gra.currentNumber = getpid() % 10 + 1;
                printf("\nLosowa wartosc poczatkowa %d, podaj kolejna wartosc\n",
                       gra->gra.currentNumber);
            } else {
                printf(", zaczekaj na swoja kolej\n");
            }
            continue;  // zacznij nowa rozgrywke
        }

        printf(", podaj kolejna wartosc.\n");
    }
}
void poinformuj_przecinika(int sockfd, struct Gra* gra) {
    // sprawdzenie czy wogole z kims gramy
    // jesli tak to poinformuj go o zakonczeniu
    if (!gra->gra.czyKoniecGry) {  // gra jest w trakcie
        gra->gra.czyKoniecGry = 1;
        if (sendto(sockfd, &gra->gra, sizeof(struct Pakiet), 0,
                   (struct sockaddr*)&gra->clientaddr, gra->clientlen) == -1) {
            perror("sendto");
        }
    }
}

void wyswietl_wynik(struct Gra* gra) {
    if (gra->gra.czyKoniecGry) {
        printf("Brak gry w trakcie\n");
        return;
    }

    if (gra->ktory_gracz == 1) {
        printf("Ty %d : %d %s\n", gra->gra.gracz1_wynik, gra->gra.gracz2_wynik,
               gra->enemy_name);
    } else {
        printf("Ty %d : %d %s\n", gra->gra.gracz2_wynik, gra->gra.gracz1_wynik,
               gra->enemy_name);
    }
}

void wyslij_pakiet(int sockfd, struct Gra* gra) {
    gra_ustaw_do_wyslania(gra);

    if (sendto(sockfd, &gra->gra, sizeof(struct Pakiet), 0,
               (struct sockaddr*)&gra->clientaddr, gra->clientlen) == -1) {
        perror("sendto");
    }
    gra_ustaw_na_hosta(gra);
}

void poinformuj_o_wygranej(int sockfd, struct Gra* gra) {
    wyslij_pakiet(sockfd, gra);

    if (gra->gra.ktoZaczynal != gra->ktory_gracz) {
        // teraz my zaczynamy
        gra->gra.ktoZaczynal = gra->ktory_gracz;
        gra->gra.kogo_tura = gra->ktory_gracz;
        gra->gra.currentNumber = getpid() % 10 + 1;
        printf("Losowa wartosc poczatkowa %d, podaj kolejna wartosc\n",
               gra->gra.currentNumber);
    } else {
        printf(
            "Zaczynamy kolejna rozgrywke, poczekaj na swoja "
            "kolej\n");
    }
}

void tura_nastepnego_gracza(struct Gra* gra) {
    gra->gra.kogo_tura = (gra->gra.kogo_tura == 1) ? 2 : 1;
}

void tryb_interaktywny(int sockfd, struct Gra* gra) {
    char reply[REPLY_MAX];
    while (8) {
        printf("> ");
        scanf(" %31s", reply);
        while (getchar() != '\n')
            ;

        if (strcmp("koniec", reply) == 0) {
            poinformuj_przecinika(sockfd, gra);
            break;
        } else if (strcmp("wynik", reply) == 0) {
            wyswietl_wynik(gra);
        } else {
            if (gra->gra.czyKoniecGry) {
                printf("Brak gry w trakcie\n");
                continue;
            }

            if (gra->gra.kogo_tura != gra->ktory_gracz) {
                printf("Teraz tura gracza %s, poczekaj na swoja kolej\n",
                       gra->enemy_name);
                continue;
            }

            int nowa_wartosc = atoi(reply);
            if ((nowa_wartosc - gra->gra.currentNumber) < 1 ||
                (nowa_wartosc - gra->gra.currentNumber) > 10 ||
                nowa_wartosc > 50) {
                printf("Takiej wartosci nie mozesz wybrac!\n");
                continue;
            }

            gra->gra.currentNumber = nowa_wartosc;
            if (gra->gra.currentNumber == 50) {  // koniec gry
                printf("Wygrana!\n");
                if (gra->ktory_gracz == 1) {
                    ++gra->gra.gracz1_wynik;
                } else {
                    ++gra->gra.gracz2_wynik;
                }

                poinformuj_o_wygranej(sockfd, gra);
                // i przydziel kto teraz zaczyna
                continue;
            }

            tura_nastepnego_gracza(gra);
            wyslij_pakiet(sockfd, gra);
        }
    }
}
int przygotuj_socket(char* domena, char* port, struct Gra* gra) {
    int status;
    struct addrinfo hints;
    struct addrinfo *results, *p;
    int sockfd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    memset(&gra->clientaddr, 0, sizeof(gra->clientaddr));
    gra->clientaddr.sin_family = AF_INET;
    gra->clientaddr.sin_port = htons((u_short)atoi(port));

    status = getaddrinfo(domena, port, &hints, &results);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }

    for (p = results; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;

        gra->clientaddr = *((struct sockaddr_in*)p->ai_addr);
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Nie mozna utowrzyc socketu\n");
        freeaddrinfo(results);
        return -1;
    }
    freeaddrinfo(results);

    return sockfd;
}

int shmid;

void posprzataj(int sig) {
    shmctl(shmid, IPC_RMID, NULL);
    exit(0);
}

struct Gra* create_shm_from_key(const char* keyfile, char k) {
    key_t key;
    struct Gra* gra;

    key = ftok(keyfile, k);
    if (key == -1) {
        perror("Nie mozna wygenerowac klucza");
        exit(1);
    }

    shmid = shmget(key, sizeof(struct Gra), 0644 | IPC_CREAT);
    if (shmid == -1) {
        perror("Nie mozna utworzyc pamieci wspoldzielonej");
        exit(1);
    }

    gra = shmat(shmid, NULL, 0);
    if (gra == (void*)-1) {
        perror("nie mozna podlaczyc pamieci wspoldzielonej shmat()");

        shmctl(shmid, IPC_RMID, 0);
        exit(1);
    }

    return gra;
}

int main(int argc, char* argv[]) {
    pid_t pid;
    int sockfd;

    struct sockaddr_in my_addr;
    struct Gra* gra;
    struct UsciskDloni ack;

    srand(time(NULL));

    if (argc != 4 && argc != 3) {
        fprintf(stderr, "uzycie: %s: <ip> <port> [ nazwa uzytkonika ]\n",
                argv[0]);
        exit(1);
    }

    gra = create_shm_from_key("main.c", 'a');
    signal(SIGINT, posprzataj);

    sockfd = przygotuj_socket(argv[1], argv[2], gra);
    if (sockfd == -1) {
        shmctl(shmid, IPC_RMID, NULL);
        exit(1);
    }

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1) {
        shmctl(shmid, IPC_RMID, NULL);

        perror("Nie mozna powiazac socketu z portem");
        exit(1);
    }

    printf("Gra w 50, Wersja A.\n");
    printf("Rozpoczynam gre z %s. ", inet_ntoa(gra->clientaddr.sin_addr));
    printf(
        "Napisz \"koniec\" by zakonczyc lub "
        "\"wynik\" by wyswietlic aktualny\n");

    if (argc == 4) {
        strcpy(gra->moja_nazwa, argv[3]);
    } else {
        gra->moja_nazwa[0] = '\0';
    }

    ustal_polaczenie(sockfd, &ack, gra, 0);
    // klient tak samo musi przygotowac lokalna gre
    // bo wie ze to serwer zaczyna i wie ze nie jest serwerem
    przygotuj_gre(gra);
    if (ack.is_server) {
        printf("Losowa wartosc poczatkowa: %d, podaj kolejna wartosc\n",
               gra->gra.currentNumber);
        gra->ktory_gracz = 1;
    } else {
        gra->ktory_gracz = 2;
    }

    pid = fork();
    if (pid == 0) {
        czekaj_na_pakiet(sockfd, gra, &ack);
        close(sockfd);
        exit(0);
    } else if (pid > 0) {
        tryb_interaktywny(sockfd, gra);
        kill(pid, SIGTERM);

        waitpid(pid, NULL, 0);

        close(sockfd);
        shmdt(gra);
        shmctl(shmid, IPC_RMID, NULL);
    } else {
        perror("Nie mozna utowrzyc potomka fork()");
        exit(1);
    }

    return 0;
}
