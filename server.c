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
#include <semaphore.h>

#define MAX_USERS 100 //limit zarejestrowanych użytkowników
#define MAX_CONNECTIONS 100 //limit polaczonych uzytkownikow

//struktura przechowująca dane o użytkowniku
struct userData{
	int userId;
	char userPassword[20];
	char userName[20];	
	char path[24]; //sciezka do pliku z zaleglymi wiadomosciami tego uzytkownika
	bool online;
	
}users[MAX_USERS];

//struktura przechowujace adresy, deskryptory polaczenia klientow
struct client{
	struct sockaddr_in caddr; //adres klienta
	int connfd; //deskryptor połączenia
	int userId;
};

//lista aktywnych polaczen
struct client *connections[MAX_CONNECTIONS];

//liczba zarejestrowanych użytkowników
int user_count;

//semafor na zmienna user_count;
sem_t sem_count;
//semafor na tablice connections;
sem_t sem_connections;

//zwraca id uzytkownika o danej nazwie
int search_user(char *name){
	printf("Szukany użytkownik:%s/\n", name);
	int i;
	for(i=0; i<=user_count; i++){
		if(strcmp(users[i].userName, name)==0){
			return i;
		}
	}
	return 0;
}

//wysyla wiadomosc do klienta
void send_message(char *s, int fd)
{
	write(fd, s, strlen(s));
}

//odbieranie wiadomosci
char* my_read(int socket){
	char *buffer = malloc(1024);
	char tmp;
	int buflen = 0;
	int n;

	while(1){
		n = read(socket, &tmp, 1);

		if(n < 0){
			printf("Blad przy odczycie z socketa\n");
			return "error";
		}
		else{
			if(tmp == '\n'){
				buffer[buflen] = tmp;
				break;
			}

			buffer[buflen] = tmp;
			++buflen;
		}
	}

	return buffer;

}

//dopisuje do pliku zalegla wiadomosc
void fwrite_message(int to_id, char *message){
	FILE *file;
	file = fopen(users[to_id].path, "a");
	fprintf(file, "%s", message);
	fclose(file);

	printf("Dopisano do pliku %s\n", users[to_id].path );
}

//wyslij wiadomosc szukajac po id uzytkownika
void send_message_id(char *s, int id)
{
	printf("Uruchomiono funkcje send_message_id\n");
	printf("Wiadoosc: %s\n", s);
	printf("Na id %d\n", id);
	int i;
	bool send = false;
	for(i=0; i<MAX_CONNECTIONS; i++){
		if(connections[i]){
			if(connections[i]->userId == id){
				send_message(s, connections[i]->connfd);
				send = true;
				break;
			}
		}
	}
	if (!send){
	//wiadomosci nie wyslano poniewaz, uzytkownik docelowy nie jest online
	//dopisz ja do pliku z zaleglymi wiadomosciami
	fwrite_message(id, s);
		
	}
}

//Dodaje polaczenie do tablicy connections
void add_connection(struct client *c){
	int i;

	for(i=0;i<MAX_CONNECTIONS;i++){
		if(!connections[i]){
			sem_wait(&sem_connections);

			connections[i] = c;

			sem_post(&sem_connections);
			return;
		}
	}

	printf("Dodano do tab connections\n");
}

//Usuwa polaczenie z tablicy connections
void remove_connection(int id){
	int i;

	for(i=0;i<MAX_CONNECTIONS;i++){
		if(connections[i]){
			if(connections[i]->userId == id){
				sem_wait(&sem_connections);

				connections[i] = NULL;

				sem_post(&sem_connections);
				return;
			}
		}
	}

	printf("Usunieto z tab connections\n");
}

//rejestracja użytkownika, zwraca 1 gdy zarejestrowano nowego uzytkownika, 0 - gdy uzytkownik o podanej nazwie juz istnieje
bool register_user(char name[20], char password[20]){ 

	if(search_user(name)==0){

		sem_wait(&sem_count);
		//SEKCJA KRYTYCZNA

		user_count++;

		users[user_count].userId = user_count;
		strncpy(users[user_count].userName, name, sizeof(users[user_count].userName));
		strncpy(users[user_count].userPassword, password, sizeof(users[user_count].userPassword));

		//zapisanie sciezki do pliku ktory bedzie przechowywal zalegle wiadomosci:
		char file_path[50];
		strcpy(file_path, name);
		strcat(file_path, ".txt");
		strcpy(users[user_count].path, file_path);
		//utworzenie pliku przechowywujacego zalegle wiadomosci:
		FILE *file;
		file = fopen(users[user_count].path, "a");
		fclose(file);

		//KONIEC SEKCJI KRYTYCZNEJ
		sem_post(&sem_count);

		printf("Zarejestrowano uzytkownika %s, haslo: %s\n", name, password);
		return 1;
	}
	else{
		printf("Uzytkownik o podanej nazwie jest juz w systemie\n");
		return 0;
	}
}

//wyswietla dane z tablicy users
void print_table(){
	printf("Tablica:\n");
	int i;
	for(i=0; i<=user_count; i++){
		printf("%d: %s, %s\n", users[i].userId, users[i].userName, users[i].userPassword);
		if(users[i].online==true) printf("ONLINE\n\n");
		else printf("OFFLINE\n\n");
	}
}

//sprawdza login i hasło i zwraca pozycje użytkownika w tablicy users
int check_login(char name[20], char password[20]){
	printf("Login: %s\npass: %s\n", name, password );
	int i;
	for(i=0; i<=user_count; i++){
		if(strcmp(users[i].userName, name)==0)
		{
			if(strcmp(users[i].userPassword, password)==0)
				return i; 
		}
	}
	return 0;
}

//usuniece znaku nowej linii
void strip_newline(char *s){
	while(*s != '\0'){
		if(*s == '\r' || *s == '\n'){
			*s = '\0';
		}
		s++;
	}
}

//sprawdza statusy użytkowników podanych w tablicy friends i wysyla id znajomych uzytkownikow ktorzy sa online
void check_friend_status(int *friends, int n, int fd){
	int i;
	int j;
	char message[1024];
	char tmp[1024];
	strcpy(message, "#STATUS");

	for (i=0; i<n; i++){//po friends
		for(j=1; j<=user_count; j++){ //po users
			if(friends[i]==users[j].userId){
				if(users[j].online==true){
					//dodaj do wiadomosci
					sprintf(tmp, "%d", users[j].userId);
					strcat(message, " ");
					strcat(message, tmp);
				}
			}
		}
	}
	strcat(message, "\n");
	send_message(message, fd);
}

//wysyla zalegle wiadomosci odczytane z pliku
void send_old_messages(int id){
	FILE *file;
	char line[1024];
	file = fopen(users[id].path, "r");
	while (1) {
		if (fgets(line,1024, file) == NULL) break;
		//strcat(line, "\n");
		send_message_id(line, id);
	}
	freopen(users[id].path, "w", file); //usuniecie zawartosci pliku
	fclose(file);
}

//obsluga klienta w osobnym watku
void* cthread(void* arg){
	char *buff_in;
	char message[1024];

	int tmp=0;
	int user_number=0; //nr id/index w tablicy users uzytkownika ktory sie zalogowal w tym polaczeniu

	struct client* c = (struct client*)arg;

	printf("Nawiązano polaczenie z klientem\n");
	//przyjmuj dane od klienta
	while(1){

		buff_in = my_read(c->connfd); //odczyt wiadomosci od klienta

		strip_newline(buff_in);	//usuniecie znaku nowej linii

		strcpy(message, "");

		printf("wiadomosc: %sx\n", buff_in); //wyswietlenie wiadomosci nadanej przez klienta

		//wszystkie wiadomosci powinny zaczynac sie od znaku '#'
		if(buff_in[0]== '#'){
			char *command, *param;
			command = strtok(buff_in, " "); //ignoruje znaki po spacji
			
			char name[20];
			char password[20];

			//ZALOGUJ SIE
			if(strcmp(command, "#LOGIN")==0){ // #LOGIN <userName> <userPassword>
				//logowanie
				param = strtok(NULL, " "); //rozpoczyna przeszukiwanie znaków od miejsca ostatniego zakończenia
				if(param){
					strcpy(name, param);
					param = strtok(NULL, " ");
					if(param){
						strcpy(password, param);
					}
				}
				//printf("%s, %s\n", name, password);
				if ((tmp = check_login(name, password)) > 0){
					//zalogowano uzytkownika
					user_number = tmp;
					users[user_number].online = true;

					c->userId = users[user_number].userId; //przypisanie id tego uzytkownika do polaczenia
					add_connection(c); //dodanie polaczenia do tablicy connections

					char tmp_message[1024];
					char s_id[1024];
					sprintf(s_id, "%d", user_number);

					strcpy(tmp_message, "#OK ");
					strcat(tmp_message, s_id);
					strcat(tmp_message, "\n");

					printf("Zalogowano uzytkownika\n");
					send_message(tmp_message, c->connfd); //odeslanie wiadomosci potwierdzajacej zalogowanie sie wraz z nr id uzytkownika
					printf("Przy lgoowaniu wyslano: %s\n", tmp_message );

					//wysylanie zaleglych wiadomosci z tym miejscu
					send_old_messages(user_number);
				}
				else{
					//nie udalo sie zalogowac uzytkownika
					printf("Nie udalo sie zalogowac uzytkownika\n");
					send_message("#ERR\n", c->connfd);
				}
				printf("tmp: %d\n",tmp);
				printf("user_number: %d\n",user_number);
			}

			//ZAREJESTRUJ
			else if(strcmp(command, "#REGISTER")==0){	//#REGISTER <userName> <userPassword>		
				param = strtok(NULL, " ");
				if(param){
					strcpy(name, param);
					param = strtok(NULL, "");
					if(param){
						strcpy(password, param);

						//printf("Name:%s\nPass:%s\n",name, password);

						if(register_user(name, password)){
							//zarejestrowano uzytkownika
							print_table();
							send_message("#OK\n", c->connfd);
						}
						else{
							//nie zarejestrowano uzytkownika
							send_message("#ERR\n", c->connfd);
						}						
					}
				}
				
			}

			//SPRAWDZ STATUSY ZNAJOMYCH
			else if(strcmp(command, "#STATUS")==0){ //#STATUS <id1> <id2> ... <idN>
				printf("Zapytanie o statusy\n");
				//send_message("#STATUS\n", c->connfd);

				int friends[100]; //tablica przechowujaca id uzytkownikow, ktorych statusy maja zostac sprawdzone
				int n=0;

				param = strtok(NULL, " ");
				if(param){
					while(param!=NULL){
						friends[n]=atoi(param);
						param = strtok(NULL, " ");
						n++;
					}
				}
				check_friend_status(friends, n, c->connfd); //sprawdzenie statusow i odeslanie do klienta id uzytkownikow, ktorzy sa online

			}

			//WYLOGUJ SIE
			else if(strcmp(command, "#LOGOUT")==0){
				printf("Wylogowanie\n");
				users[user_number].online = false;
				remove_connection(users[user_number].userId); //usuniece polaczenia z tablicy connections
				break;
			}

			//WYSZUKIWANIE UŻYTKOWNIKA O PODANEJ NAZWIE
			else if(strcmp(command, "#SEARCH")==0){ //#SEARCH <userName>
				int searched_id=0;
				char tmp[1024];
				char s_id[1024];

				param = strtok(NULL, " ");

				if(param){
					if ((searched_id=search_user(param)) > 0){
						//znaleziono uzytkownika o podanej nazwie
						sprintf(s_id, "%d", searched_id);

						strcpy(tmp, "#OK ");
						strcat(tmp, s_id);
						strcat(tmp, "\n");

						send_message(tmp, c->connfd); //#OK <userId>
					}
					else{
						//nie znaleziono uzytkownika
						send_message("#ERR\n", c->connfd);
					}
				}
			}

			//WIADOMOSC
			else if(strcmp(command, "#MESSAGE")==0){ //#MESSAGE <do_kogo_userId> <od_kogo_userId> <wiadomosc>
				printf("MESSAGE\n");
				
				param = strtok(NULL, " ");
				if (param){
					int to_id = atoi(param); //userId adresata

						param = strtok(NULL, " ");
						if(param){

							sprintf(message, "%s", "#MESSAGE "); 
						strcat(message, param);	//dodanie do wiadomosci userId nadawcy
						param = strtok(NULL, " ");
						
						if (param){
							while(param!=NULL){
								strcat(message, " ");
								strcat(message, param);
								param = strtok(NULL, " ");
							}
							strcat(message, "\r\n");
							//printf("message: %s\n", message);
							send_message_id(message, to_id); //#MESSAGE <od_kogo> <wiadomosc>
						}
					}
				
			}
			printf("%s\n", message);
		}

		else{
			printf("Blad. Niepoprawna komenda!\n");
			send_message("#ERR\n", c->connfd);
		}		

	}

}

close(c->connfd);
free(c);	

return 0;
}


int main(int argc, char **argv)
{
	user_count = 0;

	sem_init(&sem_count, 0, 1); //semafor binarny
	sem_init(&sem_connections, 0, 1); 

	pthread_t tid;
	socklen_t slt;

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