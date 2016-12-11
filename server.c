#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <stdbool.h>

#define MAX_USERS 100 //limit zarejestrowanych użytkowników

//struktura przechowująca dane o użytkowniku
struct userData{
	int userId;
	char userPassword[20];
	char userName[20];	
	char message[1024]; //zalegle wiadomosci/ wiadomosci ktorych uzytkownik nie odczytal
	bool online;
	int friend_list[50]; //lista userId znajomych
};

//struktura przechowujace adresy, deskryptory polaczenia klientow
struct client{
	struct sockaddr_in caddr; //adres klienta
	int connfd; //deskryptor połączenia
	int from_userId; //zeby wiedziec jaki uzytkownik wykonal polaczenie do serwera
	int to_userID; //do kogo wysylac wiadomosci
};

//tablica przechowująca wszystkich użytkowników
struct userData users[MAX_USERS];
//liczba zarejestrowanych użytkowników
static int user_count;

void register_user(int fd){ //trzeba dodać: ogarnianie pustych wiadomosci, write/read osobne funkcje z petlami
	//struct client* c = (struct client*)client;
	char tmp[20];

	write(fd, "Podaj login", 20);
	read(fd, tmp, 20);

	strcpy(users[user_count].userName, tmp);

	write(fd, "Podaj hasło", 20);
	read(fd, tmp, 20);

	strcpy(users[user_count].userName, tmp);

	write(fd, "Dzięki", 20);

	user_count++;
}

void* cthread(void* arg){
	//obsluga klienta w osobnym watku
	struct client* c = (struct client*)arg;
	printf("Nawiązano polaczenie z klientem\n");

	char signal[10] = ""; //1 - logowanie, 2 - rejestracja
	int size = read(c->connfd, signal, 1);	
	printf("signal: %s\n",signal );
	if(strcmp(signal, "1")==0){
		//logowanie
	}
	else if (strcmp(signal, "2")==0){
		//rejestracja
		register_user(c->connfd);
	}
	else{
		//błąd
		printf("blad przy logowaniu/rejestracji\n");
	}

	close(c->connfd);
	free(c);
	return 0;
}

//sprawdza czy dana wartosc val zawiera sie w tablicy arr
bool in_array(int val, int *arr, int size){
    int i;
    for (i=0; i < size; i++) {
        if (arr[i] == val)
            return true;
    }
    return false;
}

//sprawdza statusy znajomych użytkowników i wysyla id znajomych uzytkownikow ktorzy sa online
void check_friend_status(int connfd, struct userData user){
	int i;
	char tmp[20];
	char online_friends[1024] = "";
	for(i=0;i<MAX_USERS;i++){
		
		if(in_array(users[i].userId, user.friend_list, 50))
		{
			sprintf(tmp, "%d", users[i].userId);
			strcat(online_friends, tmp);
		}
	}
	write(connfd, online_friends, strlen(online_friends));
}

int main(int argc, char **argv)
{
	user_count = 0;

	pthread_t tid;
	socklen_t slt;
	int fd;  //deskryptor gniazda od klienta
	struct sockaddr_in saddr;

	int listenfd = socket(AF_INET, SOCK_STREAM, 0);

	int on = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));

	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons(5001); 

	//bind łączy adres z gniazdem
	if(bind(listenfd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0){
		perror("Socket binding failed");
		return 1;
	}

	//listen oznacza, że socket będzie gniazdem pasywnym, czyli bedzie akceptowal przychodzace polaczenia 
	if(listen(listenfd, 10) < 0){
		perror("Socket listening failed");
		return 1;
	}

	//przyjmowanie polaczen od klientow:
	while(1)
	{
		struct client* c = malloc(sizeof(struct client));
		slt = sizeof(c->caddr);
		c->connfd = accept(listenfd, (struct sockaddr*)&c->caddr, &slt); 
		pthread_create(&tid, NULL, cthread, c);
		pthread_detach(tid);
	}

	return 0;
}

