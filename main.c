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
        // to oznacza ze wiem_ze_jestem_serwerem bylo != 0
        // wiedzielismy ze jestesmy serwerem
        //
        // czyli bedziemy oczekiwac po prostu az ktos sie polaczy
        // 1 argumnet przez jaki socket otrzymamy informacji
        // 2 argument dzie to zapisac 3 ile bajtow bla bla bla 
        // 4 arg to  0 flagi nawet nie mam porowniania bo pisze to kurna 4 raz do tego 0 jak hajsu ile wydam na sugar daddego z analizy
        // 5 argumnet to skad otrzymalismy wiadomosc, ip nadawcy jest wpisywane do tej struktury
        // 6 tak samo dlugosc adresu w bajtach jest zapisywane
        recvfrom(sockfd, ack, sizeof(*ack), 0,
                 (struct sockaddr*)&gra->clientaddr, &gra->clientlen);



        // w komunikacie poinformuj ze elo mordo ja juz jestem serwrem i wiem o tym
        strcpy(gra->enemy_name, ack->nick);  // zapisz otrzymany nick do naszej lokalnej gry

        // przygotuj wiadomosc do usciskuDloni zeby gosciu po drugiej stronie wiedzial z kim 
        // on wogole gra kultura wymaga sie przedstawic hehehsflkhsfdlk
        ack->is_server = 1;
        strcpy(ack->nick, gra->moja_nazwa); // zapisz nasz nick we wiadomosci

        // wyslij nasz uscisk dloni do typa o tym kim my jestesmy (nasz nick) i o tym ze wiemy ze my jestesmy serwerem
        sendto(sockfd, ack, sizeof(*ack), 0, (struct sockaddr*)&gra->clientaddr,
               sizeof(gra->clientaddr));

        // znow gra->clientaddr (mozesz to nazwac adres_klienta) 
        // to ma typ sockaddr_in (czyli przeznaczony dla IPv4) dla IPv6 by mial 6 na koncu -> sockaddr_in6
        // musimy miec uoglnienie czyli (struct sockaddr*) zauwaz ze nie ma koncowki "_in"
        // musimy to rzutowac na (struct sockaddr*) czyli wskaznik przez to musimy uzyc & przed gra->clientaddr
        // rzutujemy zeby kompilator na nas nie krzyczal 
        // ta funkcja sockaddr to jest ugolnienie zeby mozna uzywac IPv4 i IPv6 (to nowsze)
        // nie obchodzi cie IPv6 tylko IPv4 i ze przykladowy adres wyglada 188.234.30.23
        // czyli 4 liczby 0-255 rodzielone kropkami
        
        // sendto 
        // 1 argument na jakim sockecie
        // 2 argument adres tego co my wlasciwie chcemy wyslac ack to juz jest wskaznik wiec nie uzywamy &
        // 3 argument ile bajtow z tej pozycji 2
        // 4 argument na jaki adres wyslac my przed cwhila go uzyskalismy z recvfrom() jak ktos do nas cos przyslal
        // 5 argument na rozmiar 4 argumentu to jest C wiec musimy podawac adres kazdej struktury :)

        // jestemy serwerem i to my musimy przygotowac gra
        przygotuj_gre(gra);
        // taka nasza konwencja ze serwer to gracz 1 i to on zaczyna nowa gre
        gra->ktory_gracz = 1;  // serwer czyli 1  

    } else {
        // ale kiedy startujemy program
        // my nie wiemy czy ktos juz nie nasluchuje (czyli czy ktos przyjal role serwera)
        // czy my jestemy pierwszymi graczami w sieci ktorzy odpalili gre  
        // wtedy to my bedziemy musieli przyjac role serwera 

        // zapisz nasz nick zeby ktos wiedzial kim jestesmy
        strcpy(ack->nick, gra->moja_nazwa);
        ack->is_server = 0;  // na poczatku zalkadamy ze nie jestesmy serwerem

        // i jeb uzywamy datagramow czyli mowimy i nie wiemy czy ktos nas slucha haha to jak ja 
        // wyslij przez nasz socket sockfd, sizeof(*ack) bajtow z ack,
        // 4 argument to flagi czyli na 0 bo nie wiadomo co to jest za wysokie progi czy tam rudy sie nie dostal
        // 5 argument to adres na jaki wysylamy to jest wlasnie adres podany przez uzytkownika jako argument
        // 6 argumnet to dlugosc naszego adresu w bajtach w pamieci
        sendto(sockfd, ack, sizeof(*ack), 0, (struct sockaddr*)&gra->clientaddr,
               sizeof(gra->clientaddr));
        printf("Propozycja gry wyslana.\n");

        // zaczynamy nasluchiwac priawe jak informacja w kosmos jak wyyslamy sygnaly do kosmitow i 
        // nasluchujemy tak samo tutaj jezeli nic nam nie odpowiada to po prostu czekamy ale powoli 
        // zaczynamy myslec ze jestemy jedyna cywilizacja tak samo tutaj powoli myslimy ze jestesmy serwerem
    

        // 1 argumnet przez jaki socket pobierz informacje
        // 2 argument gdzie zapisac dane
        // 3 argument ile bajtow
        // 4 argument flagi to 0 jak ja z toba biala flage
        // 5 argument adres skad przyszla wiadmosci ip typa ktory nam cos przyslal
        // 6 argumnet dlugosc tego adresu w bajtach
        recvfrom(sockfd, ack, sizeof(*ack), 0,
                 (struct sockaddr*)&gra->clientaddr, &gra->clientlen);

        // jezeli typo mowi ze nie jest serwerem
        if (!ack->is_server) {  // host mowi ze nie jest serwerem
            // wiec to ten host powinien byc serwerem
            // przeciez to my czekalimsy najpierw z recvfrom() a to on pozniej przszedl z sendem
            // nasz sendto() zostal zignorowany
            // lubimy princess treatment sluchalismy() to basta my chcemy byc serwerem

            strcpy(gra->enemy_name, ack->nick);  // on przysal nam swoj nick

            ack->is_server = 1;  // mordo ja tu rzadze
            strcpy(ack->nick, gra->moja_nazwa);  // masz tu moj nick

            // wyslij mu informacje ze to my czekalimsy my jestesmy serwerem i masz tu moj nick
            // jakc bedziesz chcial zobaczyc kto ciebie jedzie w liczeniu do 50
            //
            // 1 argument przez jaki socket wysylamy
            // 2 adres naszej informacji tutaj ack to juz jest wskaznik (patrz argumnety) wiec nie uzywamy &
            // 3 liczba bajtow naszej informacji czy wiadomosci jeden pies
            // 4 argument flagi 0 czyli nic nie mamy do powiedzenia 
            // 5 argument na jaki adres wyyslamy juz pisalem duoz razy czemu & i czemu na sockaddr sory ale gdzies to bylo
            // 6 argument rozmiar adresu ip
            sendto(sockfd, ack, sizeof(*ack), 0,
                   (struct sockaddr*)&gra->clientaddr, sizeof(gra->clientaddr));
        } else {
            // oho typo mowi ze jest serwerem stule sie ladnie bo mi zabierze kanapki
            // wiec ten host powinen byc klientem
            ack->is_server = 0;
            strcpy(gra->enemy_name, ack->nick);  // zapisz nick typa z ktorym gramy
        }
    }

    printf("%s dolaczyl do gry.\n", inet_ntoa(gra->clientaddr.sin_addr));
}

void przygotuj_gre(Gra* gra) {
    gra->gra.gracz1_wynik = gra->gra.gracz2_wynik = 0;
    // na poczatku obaj gracze nie maja zadnych wygranych wiec po 0


    // WAGA wyklad o liczbach pseudolosowych w komputerach
    // nie musisz tego zrozumiec
    // dla losowosci uzywac tylko
    // srand(timmme(NULL)) -> ustaw generator
    // rand() -> wylosuj liczbe tylko ta lizba bedzie duza i nakladamy takie ogranizenie za pomoca modulo %

    // srand(ziarno): ziarno to jakas liczba i np to moze byc 1
    // rand() zwroci ziarno i je pomnozy np * 1000 przez co 
    // rand() zwroci ziarno 1000 je pomnozy np * 1000 przez co 
    // rand() zwroci ziarno 10000000 je pomnozy np * 1000 .... itd
    // przez to wydaje sie ze to jest losowe
    //
    // ale nastepne wywowalnie programu mamy znow ziarno 1 czyil otrzymamy 
    // takie same losowe liczby
    //
    //
    // time(NULL) : zwraca liczbe sekund od 1970 roku
    // przez to ze sekundy sie zwiekszaja caly czas (NO BA) to wydaje sie ze pomnozone wartosic
    // sa losowe i tylko tak dzialaja losowe liczby w komputerze
    // nie musisz tego rozumiec nie martw sie 
    

    // ale to sa ogromne liczby a my mamy miec lizba [1..10]
    // no to myk modulo wrzucamy ograniczenie [0..9] i zeby bylo [1..10] to dodajemy 1
    gra->gra.currentNumber = rand() % 10 + 1;

    // wiadomo nowa gra wiec to jest poczatek a nie koniec
    gra->gra.czyKoniecGry = 0;

    // tura gracza 1 czyli serwera (tak ustalilismy) 
    // my jestemy serwerem czyli ustawiamy "na siebie" jak Artur
    gra->gra.kogo_tura = 1;
}

void gra_ustaw_na_hosta(Gra* gra) {
    // pewnie odczytalismy pakiet z sieci czyli musimy przetlumaczyc 
    //  zamieniamy tylko wartosci wiecej niz 1 bajtow
    //  cyzli nie char
    //  a np short to 2 bajty i  255 = FF to raz by bylo 0xFF00 a raz 0x00FF
    //  bo doskonale pewnie wiesz ze 0x tzn szesnastkowo 2 znaki to jeden bajt
    //  a jak nie to teraz wiesz GRATULACJE
    gra->gra.gracz1_wynik = ntohl(gra->gra.gracz1_wynik);
    gra->gra.gracz2_wynik = ntohl(gra->gra.gracz2_wynik);
    gra->gra.currentNumber = ntohl(gra->gra.currentNumber);
    gra->gra.kogo_tura = ntohl(gra->gra.kogo_tura);
}

void gra_ustaw_do_wyslania(Gra* gra) {
    // po prosut zamien kazda wartosc ktora ma wiecej bajtow niz 1 
    // na poruszanie przez siec
    gra->gra.kogo_tura = htonl(gra->gra.kogo_tura);
    gra->gra.gracz1_wynik = htonl(gra->gra.gracz1_wynik);
    gra->gra.gracz2_wynik = htonl(gra->gra.gracz2_wynik);
    gra->gra.currentNumber = htonl(gra->gra.currentNumber);
}

void recvGameInfo(int sockfd, Gra* gra, UsciskDloni* ack) {
    while (8) {  // nieskonczona petal 
        // ale to rodzic nam wysle sygnal zeby zakonczyc wiec sie nie boimy

        // tu juz mamy rozpoczeta gre
        // wiec czekaj za danymi o grze
        // 1 arg czekaj za gra na sockecie sockfd
        // 2 arg wczytaj argument do naszej lokalnej gry i wczytaj 3 sizeof(Pakiet)
        // 4 argumne tto flagi 0 ktorym ty nie jestes dasz rade wierze w ciebie naprawde
        // wiem ze to sie moze wydawac trudne ale trzymam za ciebie kciuki
        // 5 argument to adres ip z ktorego przyszla nasza wiadomosci
        // 6 dlugosc tego adresu 
        // tak jak przyjdzie do ciebie paczka zebys wiedziala kto jest nadawca
        recvfrom(sockfd, &gra->gra, sizeof(Pakiet), 0,
                 (struct sockaddr*)&gra->clientaddr, &gra->clientlen);

        // jak gosciu informuje ze koniec to trudno splakal sie bo przegrywal
        if (gra->gra.czyKoniecGry) {
            printf("\n%s zakonczyl gre, mozesz poczekac na kolejnego gracza.\n",
                   gra->enemy_name);

            // wiemy ze teraz to my bedziemy serwerem, bo przed chilwa enemy sie poplakalo
            // czyli to my bedziemy czekac az ktos nowy dolaczy 
            //
            // ustal polaczenie ale my juz wiemy ze jestesmy serwerem 
            // czyli 4 argumnet ustawiamy na 1 albo byle co roznego od 0

            ustal_polaczenie(sockfd, ack, gra, 1);

            // info o nowej wygenerowanej wartosci
            printf("Losowa wartosc poczatkowa: %d, podaj kolejna wartosc\n",
                   gra->gra.currentNumber);
            continue; // zacznij nasluchiwac na nowe pakietu
        }

        gra_ustaw_na_hosta(gra);  // przygotuj wartosci do wyslania 
        printf("\n%s podal wartosc %d", gra->enemy_name,
               gra->gra.currentNumber);

        if (gra->gra.currentNumber == 50) {
            printf("\nPrzegrana!\n");

            // tu nw chuj zostaw tak ale wychodzi na to ze przegrany zaczyna zawsze nowe gra
            // ale dobra nikt sie nie przczepi
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
    char reply[REPLY_MAX];  // musimy miec jakis bufor bo nie wiemy czy uzytkownik poda 
    // string czyli nowa komende czy nowa liczbe
    // wgle string to jedne stringi jakie programista widzi  NIE ZALUJE 
    while (8) {  // wiadomo nieskonczona petelka jak moja na szyje
        printf("> ");
        scanf(" %31s", reply);  // %31 oznacza wczytaj maksymlanie 31 znakow
        while (getchar() != '\n')  // zignoruj do konca lini ktora wpisal uzytkwnik np  
            // "kotek\n" zostawilo by '\n' nieodczytany i nastepne wczytanie byloby zle 
            ;

        if (strcmp("koniec", reply) == 0) {  // jezelk wejscie to "koniec"
            gra->gra.czyKoniecGry = 1;  // ustaw flage na koniec gry
            // wyslij do uzytwknoika milion raz yjuz tuttaj pisalem co sie dizeje 
            // wiec napisze moja wersje inwokacji
            // Kingo ojczyzno moja    // nie moge znalezc zamiennika ojczyzno 
            // ty jest jak zdrowie    // ktore by nie bylo za mocnym slowem
            // ile cie trzeba cenic 
            // ten tylko sie dowie
            // kto cie stracil

            if (sendto(sockfd, &gra->gra, sizeof(Pakiet), 0,
                       (struct sockaddr*)&gra->clientaddr,
                       gra->clientlen) == -1) {
                perror("sendto");  // sendto to systemowa funkcja bo istneijee
                // man -s 2 sendto czyli uzywamy perror
            }

            break;  // przejdz do czyszczenia
        } else if (strcmp("wynik", reply) == 0) {
            // sprawdz czy wgle cos sie rozgrywa
            if (gra->gra.czyKoniecGry) {
                printf("Brak gry w trakcie\n");
                continue;
            }

            if (gra->ktory_gracz == 1) {  // tu po prostu perspektywa jak wyswietlamy
                printf("Ty %d : %d %s\n", gra->gra.gracz1_wynik,
                       gra->gra.gracz2_wynik, gra->enemy_name);
            } else {
                printf("Ty %d : %d %s\n", gra->gra.gracz2_wynik,
                       gra->gra.gracz1_wynik, gra->enemy_name);
            }
        } else {
            // sprawdz czy wgle cos sie rozgrywa
            if (gra->gra.czyKoniecGry) {
                printf("Brak gry w trakcie\n");
                continue;
            }

            // zamien na inta
            int nowa_wartosc = atoi(reply);
            // sprawdz czy wogole jest nasza tura
            if (gra->gra.kogo_tura != gra->ktory_gracz) {
                printf("Teraz tura gracza %s, poczekaj na swoja kolej\n",
                       gra->enemy_name);
                continue;  // jezeli nie to czekaj chamie dalej
            }

            // nie podajemy o ile zwiekszyc tylko nowa warotsc ktora moze byc o 10 maks wieksza
            // sprawdzamy tez czy nie wyskoczymy poza te 50 ktore trzeba osiagnac
            // nie ja wymyslalem zasady glupie 
            if ((nowa_wartosc - gra->gra.currentNumber) < 1 ||
                (nowa_wartosc - gra->gra.currentNumber) > 10 ||
                nowa_wartosc > 50) {
                printf("Takiej wartosci nie mozesz wybrac!\n");
                continue;
            }

            gra->gra.currentNumber = nowa_wartosc;  // ustaw wartsoc
            // sprawdz wygrana
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

            // zmien ture
            // jezeli gracz 1 byl teraz to ustaw gracza 2 jezeli nie to tura gracza 1
            gra->gra.kogo_tura = (gra->gra.kogo_tura == 1) ? 2 : 1;
            gra_ustaw_do_wyslania(gra);  // przygotuj do wyslania w internet

            // wyslij beng beng risky text i mozna isc sie uczyc  hhfadshfhahfdah
            // znow 4000 razy pisalem co sie dizjee czemu rzutuje 
            // to napisze ci DASZ RADE nie poddawaj wiem ze jest ciezko
            // ale kto jak nie ty
            if (sendto(sockfd, &gra->gra, sizeof(Pakiet), 0,
                       (struct sockaddr*)&gra->clientaddr,
                       gra->clientlen) == -1) {
                perror("sendto");
            }

            gra_ustaw_na_hosta(gra);  // ustaw spowrotem na hosta zeby uzytkownik
            // mial swoja wersje gry ktora moze odczytywac w swoim ramie
        }
    }
}
int przygotuj_socket(char* domena, char* port) {
    int status;  // zapisz wynik getaddrinfo czy sie udalo
    struct addrinfo hints;  // wskazowki dla getaddrinfo jakich adresow ma szukac
    struct addrinfo *results, *p;  // wynik i taka nasza jakby zmienna przechodzaca po liscie
    int sockfd;

    // WAZNE: ustaw to na 0 zeby nie miec gdzies w strukturze 
    // smieciowych wartosci ze nagle np
    // hints.ai_protocol = 1321312312  // bo to zaburzy nasze filtrowanie
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;  // IPv4 
    // TCP -> segmenty
    // UDP -> datagram <- to jest nasze
    //
    // ogolnie chodzi o to ze TCP jest niezawodny co wyslesz musi dojsc
    // UDP czyli nasze datagramy po prostu wysylamy i zyjemy dalej nie obchodzi nas co sie z tym stalo
    //
    // np tcp to mowi ktos do ciebie
    // ej zaraz do ciebie bede mowil
    // dobra to mow czekam
    // dobra to mowie
    //      bla bla bla
    //      bla bla bla
    // skonczylem mowic
    // dobra uslyszalem co mowiles
    // to bylo bla bla bla bla
    // tak to bylo dokladnie to fajnie ze nie musze sie powtarzac
    //
    // a udp czyli datagramy
    //      bla bla bla
    //      bla bla bla
    // jo slysze ze cos gadasz
    // jakies bla bla bla
    //
    // no jest to po prostu zawodne i nie musi dojsc 
    // no to po co to jest jak to jest zawodne
    // np filmy gdzie masz 60 klatek to nie zauwazysz jak w jednej klatce
    // jeden piksel bedzie mial inny odcien bo bajt sie zgubil
    // albo pozycja gracza jest wysylana 64 razy na sekunde 
    // jak bedzie wyslana 63 to najwyzej ktos sie zdenerwuje ze lag i tyle

    // wskazowki dla getaddrinfo czego szukac
    // takie jakby filtry dla adaresow jakie znajdziemy
    hints.ai_socktype = SOCK_DGRAM; // szukamy socketu z datagramami
    // po prosut nas to nie obchodzi
    // to i tak jest zbedne bo ustawialismy cala strukture hints na 0
    hints.ai_flags = 0;  // zadnych flag
    hints.ai_protocol = 0;  // zadnych konkretnych protokolow

    // ta funkcja zwraca tak jakby 2 razy
    // * 1 raz do status i jak zwykle int != 0 czyli jakis blad
    // * 2 raz zapisuje wynik do results
    //
    // 1 argument to domena albo adres int to np aleks-2 albo ip 
    // 2 argument to port 5555 to tak okreslilismy nasz program
    // 3 argument to wskazowki takie jakby filtry
    //
    // 4 argument to gdzie zapisujemy znalezione wyniki
    status = getaddrinfo(domena, port, &hints, &results);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        // perror() uzywamy do wywolan systemowych czyli tych z sekcji 2
        // gai_strerror() obsluguje bledy getaddrinfo() ktora ktos napisal i nam dal czyli sekcja 3
        // to ma tyle bledow bo to duza funkcja mega pomocniza dlatego potrzebuje
        // osobnej funkcji zeby zamienic kod bledu na string
        // np 2 -> "nie ma takiego adresu"
        return -1;
    }


    // results to zwykla linked list
    // czyli w pamieci to wygladad jakos tak
    // adres1 -> adres2 -> adres3 -> NULL
    //
    // czyli null to oznacza koniec naszej listy
    // po liscie sie poruszamy tylko w jedna strone za pomoca wskaznika na nastepny wpis
    // teraz ktory adres uzyc
    // no nie jestesmy zadnymi specami uzywamy pierwszy lepszy ktory dziala
    for (p = results; p != NULL; p = p->ai_next) {
        // utworz socket dla tego wpisu z adresem
        // socket to tak jakby ip : port
        // ip rozrozna komputer w sieci
        // port rozroznia aplikacje na komputerze do ktorej przekazac wiadomosc

        // mysl o tym ze ip to adres twojego budynku(komputer)
        // a w tym budynku port to nr twojego mieszkania


        // getaddrinfo() to taka fajna funkcja ze nie musimy znac zadnych wartosci
        // prezciez juz je filtrowalismy za pomoca hints

        // pierwszy argument czy to jest IPv4 czy IPv6
        // drugia argument to czy TCP czy UDP my uzywamy datagramow czyli SOCK_DGRAM  - ale to juz podalismy w hints
        // 3 argument to jaki protokow to tez podalismy ze 0 czyli obojetnie
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;

        break;  // jezeli sie udalo utworzyc socket przerwij
    }

    // jezeli p == NULL to dotralismy na koniec listy
    if (p == NULL) {  // zaden adres nie byl dobry
        fprintf(stderr, "Nie mozna utowrzyc socketu\n");
        freeaddrinfo(results);
        return -1;
    }
    freeaddrinfo(results);  // getaddrinfo() alokuje pamiec i musimy ja zwolnic ta funkcja pomocnicza
    // ktora juz ktos za nas dostarcza 
    // to pewnie przechodzi 
    // zwolnij addr1
    // zwolnij addr2
    // NULL -> koniec

    return sockfd;  // zworc nasz utowrzony socket 
    // socket podobnie jak deskryptor pliku oznaczamy intem (liczba calokowita)
}

void fillWithMyIP(int sockfd, char* buf) {
    // nw czy to dzilaa ale chodiz tylko o to zeby dostac ip naszej maszyny
    struct sockaddr_in addr;
    socklen_t len;

    // pobierz ip do addr hosta polaczonego na sockfd
    // ale sockfd polaczylismy do siebie za pomoca bind()
    getsockname(sockfd, (struct sockaddr*)&addr, &len);

    // zapisz do bufora 
    // inet_ntoa zamienia reprezentacja np 0010101.1001.1000110.101001 na "123.23.23.3" 
    // to nie musi sie zgadzac chodzi tylko ze zamienia z bitow w pamieci na string
    strcpy(buf, inet_ntoa(addr.sin_addr));
}

int shmid;

void posprzataj(int sig) {
    // pobierz id pamieci wspoldzielonej 
    // IPC_RMID -> inter process communication remove za pomoca id
    // 3 argument jest ignorowany przy IPC_RMID wiec moze byc to nul albo "<3"
    shmctl(shmid, IPC_RMID, NULL);
    exit(0);
}

int main(int argc, char* argv[]) {
    pid_t pid;
    struct sockaddr_in my_addr;  // informacje o adresie IPv4 czyli np 192.168.0.1
    int sockfd;  // socket podobnie jak file descriptor rozpoznajemy jako int
    // przez to ze socket dziala jak plik mozemy uzywac do niego write() i read() 
    // and sendto() i recvfrom() daja wiecej mozliwosci 

    // dzielimy pamiec miedzy 2 procesami 
    // * jeden proces czeka na komunikaty i wyswietla jak cos przyszlo
    // * drugi proces wczytje od uzytkownika
    key_t key;  //  shared memory uzyskujemy za pomoca klucza
    Gra* gra;  // wskaznik na shared memory

    struct UsciskDloni ack;  // narazie nie wazne
    // uzywamy tego pozniej zeby ustalic polaczenie

    srand(time(NULL));  // do generowania losowych liczb (patrz przygotuj_gre()) tam masz wyklad co to

    // 3 albo 4 argumenty bo nazwa uzytkownika jest opcjonalna
    if (argc != 4 && argc != 3) {
        fprintf(stderr, "uzycie: %s: <ip> <port> [ nazwa uzytkonika ]\n",
                argv[0]);
        exit(1);
    }

    key = ftok("main.c", 'a');  // wygeneruj klucz z pliku plik musi istniec
    if (key == -1) {
        perror("Nie mozna wygenerowac klucza");
        exit(1);
    }

    shmid = shmget(key, sizeof(Gra), 0644 | IPC_CREAT);  // utworz shared memory na nasza gre
    // nasza gra jest narazie lokalna
    if (shmid == -1) {
        perror("Nie mozna utworzyc pamieci wspoldzielonej");
        exit(1);
    }
    signal(SIGINT, posprzataj);  // ustaw sygnal zeby usunac pamiec dzielona dla CTRL-C

    gra = shmat(shmid, NULL, 0);  // podlacz pamiec do naszego wskaznika (naszej gry)
    if (gra == (void*)-1) {
        perror("shmat()");
        exit(1);
    }

    // funkcja pomocnicza 
    sockfd = przygotuj_socket(argv[1], argv[2]);
    if (sockfd == -1) {
        shmctl(shmid, IPC_RMID, NULL);
        exit(1);
    }


    // zawsze zerujemy struktury ktore bedziemy uzwac do wysylania itp
    // zeby nie miec smieciowych wartosci ktore moga wywolac bugi

    // & czyli przekaz obiekt funkcji zeby go za nas wypelnila
    // 0 czyli ustaw kazdy bajt na 0
    // ustaw sizoef(my_addr) bajtow na 0
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;  // AF_INET -> adres family IPv4 czyli np 192.168.0.1
    // htons() zeby bajty byly little endian 
    // jezeli wysylasz jakies liczby przez siec robisz 
    // host to netowrks short (2 bajty bo sizeof(short) == 2)
    // np 255 to 0x00FF dla little endian
    // a 0xFF00 dla big endian, bo najmniej znaczacy bajt jest pierwszy
    my_addr.sin_port = htons(PORT);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // dowolny (moj) interfejs
    
    // polacz z naszym socketem nasz adres ktory podajemy jako 2 argument z jego dlugoscia
    // czyli 3 argument
    // my_addr ma typ sockaddr_in  czyli socket dla IPV4
    // funkcja wymaga ogolnego typu sockaddr* bo to takie uogolnienie i pozwala to tez
    // na uczywanie IPv6 czyli tych nowszych ale sie nie martw tego nie uzywamy
    // czyli zeby rzutowac na wskaznik (struct sockaddr*)
    // musimy najpierw miec wskaznik dlatego dodajemy & (zeby pobrac adres tej zmiennej)
    //
    // bind wywolujemy jezeli bedziemy nasluchiwac i to my chcemy okreslic na jakim porcie
    if (bind(sockfd, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1) {
        shmctl(shmid, IPC_RMID, NULL);  // posprzataj shared memory

        perror("Nie mozna powiazac socketu z portem");
        exit(1);
    }

    // wyzeruj wszystkie pola w naszej grze
    // bo ta gra to taki jakby pakiet ktory bedziemy przesylac
    memset(&gra->clientaddr, 0, sizeof(gra->clientaddr));
    gra->clientaddr.sin_family = AF_INET;  // IPv4 czyli 4 oktety oddzielone kropkami 192.168.0.1
    // oktet to po prostu 8bitow bo 192 dziesietnie to 1100 0000
    gra->clientaddr.sin_port = htons((u_short)atoi(argv[2])); // ustaw port klienta na ten podany jako argument
    inet_aton(argv[1], &gra->clientaddr.sin_addr);  // ustaw adres klienta na ten podany jako argument

    // tutaj wiadomo informacje
    printf("Gra w 50, Wersja A.\n");
    printf("Rozpoczynam gre z %s. ", argv[1]);
    printf(
        "Napisz \"koniec\" by zakonczyc lub "
        "\"wynik\" by wyswietlic aktualny\n");

    if (argc == 4) {  // jezeli podal klient nick jaki chce miec to poslugujemy sie tym nickiem
        strcpy(gra->moja_nazwa, argv[3]);
    } else {
        // a jak nie to jako jego nick traktujemy jego ip
        // nw szczezre czy to dziala ta funkcja 
        fillWithMyIP(sockfd, gra->moja_nazwa);
    }

    // tu uzywamy tego wlasciwego ack usciskuDloni
    ustal_polaczenie(sockfd, &ack, gra, 0);

    przygotuj_gre(gra);
    if (ack.is_server) {  // jestesmy serwerem
        // to serwer ma poprawnie utworzyc gre
        // pokaz uzytkownikowi jaka wartosc wylosowalismy 
        // i pokaz zachete zeby wpisal jakas wartosc
        printf("Losowa wartosc poczatkowa: %d, podaj kolejna wartosc\n",
               gra->gra.currentNumber);
        // wiadomo 1 to jest serewre tak sobie ustalilismy 
        // mozesz gdzies sobie wpisac #define SERVER 1
        // mozesz gdzies sobie wpisac #define CLIENT 2
        // mi sie nie chce wszedzie poprawiwac tej 1 
        gra->ktory_gracz = 1;  // ustal zeby uzytkownik wiedzial ktorym jest graczem
    } else {
        gra->ktory_gracz = 2;  // ustal zeby uzytkownik wiedzial ktorym jest graczem
    }


    pid = fork(); // widelec
    //
    //                  |
    //       ___________|___________
    //      |                       |
    //     czekaj                wejscie 
    //   na wiadomosci          naszego uzytkownika
    //  i wyswitlaj je
    //
    //        
    if (pid == 0) {  // to dziecko jest od wyswieltania czyli jest pasywne ( ͡° ͜ʖ ͡°)  jeny nie moglem sie powstrzymac
        recvGameInfo(sockfd, gra, &ack);
        close(sockfd);  // zamknij nie uzywany socket po skonczeniu nasluchiwania 
        exit(0);  // zakoncz dziecko po co tak drastycznie
    } else if (pid > 0) {
        userInteraction(sockfd, gra);
        kill(pid, SIGTERM);  // nwm jak to inaczej zrobic
        // jak uzytkwnik wyjdzie z userInteraction to wyslij sygnal do dziecka ze mordo 
        // zwijamy sie do domu pozegnaj sie i leci SIGTERM z bomby 
        // bo mamy schabowe w domu
        // schabowe w domu = SIGTERM sory ale to jest moj humor jak to pisze w nocy
        // SIGTERM = SIGnal TERMinate 

        waitpid(pid, NULL, 0);  // czekaj za dzieckem az zglosic ze pakuje waliki

        close(sockfd);  // zamknij nie bedziemy juz nic sluchac po sieci
        shmdt(gra); // odlacz pamiec
        shmctl(shmid, IPC_RMID, NULL);  // usun pamiec
    } else {
        perror("Nie mozna utowrzyc potomka fork()");
        exit(1);
    }
}
