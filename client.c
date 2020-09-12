#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include "./lib/comm.h"
#include "./lib/tcp_receiver.h"
#include "./lib/tcp_sender.h"
#include "./lib/utility.h"
#define fflush(stdin) while ((getchar()) != '\n')

void client_setup_conn (int*, struct sockaddr_in*);
void client_reliable_conn (int, struct sockaddr_in*);
void client_reliable_close (int client_sock, struct sockaddr_in *server_addr);
int client_folder_files(char *list_files[MAX_FILE_LIST]);
void* alarm_routine();




int main (int argc, char** argv) {
	int control, answer, num_files;
	int client_sock;
	int list = LIST, get = GET, put = PUT, close_conn = CLOSE;
	struct sockaddr_in server_address;
	char *buff = calloc(PKT_SIZE, sizeof(char));
	char *path = calloc(PKT_SIZE, sizeof(char));
	socklen_t addr_len = sizeof(server_address);
	int fd, clientNum;
	off_t end_file, file_control;
	char *list_files[MAX_FILE_LIST];
	ready_pkt recv_ready_pkt;

	clearScreen();
	client_setup_conn(&client_sock , &server_address);
	client_reliable_conn(client_sock, &server_address);
  	memset(&recv_ready_pkt, 0, sizeof(ready_pkt));
	
	
	control = recvfrom(client_sock, &recv_ready_pkt, sizeof(ready_pkt), 0, (struct sockaddr *)&server_address, &addr_len);

	clientNum = recv_ready_pkt.clientNum;  
	printf("clientNum: %d\n", clientNum);
	printf("message  : %s\n",recv_ready_pkt.message);

	if (control < 0 || strcmp(READY,recv_ready_pkt.message) != 0) {
		printf("CLIENT: server dispatching failed\n");
		exit(-1);
	}
	
	

menu:
	printf("\n\n________________________ COMMAND LIST ________________________\n\n");
	printf("1) List available files on the server\n");
	printf("2) Download a file from the server\n");
	printf("3) Upload a file to the server\n");
	printf("4) Close connection\n");
	printf("______________________________________________________________\n\n");
	printf("> Choose an operation: ");
	scanf ("%d",&answer);
	clearScreen();

  	switch (answer) {
//****************************************************************************************************************************************
    	case LIST:
			printf("==============================================================\n");
			printf("\t\t\t LIST REQUEST\n");
			printf("==============================================================\n");


			control = sendto(client_sock, (void*)&list, sizeof(int), 0, (struct sockaddr *)&server_address, addr_len);
			if (control < 0) {
				printf("CLIENT: request failed (sending)\n");
				exit(-1);
			}
			fd = open("clientFiles/file_list.txt", O_CREAT | O_TRUNC | O_RDWR, 0666); //Apro il file con la lista dei file del server
			
			// Inizio la ricezione del file
			tcp_receiver(client_sock, &server_address,fd,CLIENT);
			clearScreen();

			// Lettura del file contenente la lista di file del server
			end_file = lseek(fd, 0, SEEK_END);
			if (end_file >0){
				lseek(fd, 0, SEEK_SET);
				read(fd, buff, end_file);
				printf("_________________________ FILE LIST __________________________\n\n");
				printf("%s", buff);
				printf("______________________________________________________________\n\n\n\n");

				close(fd);
				remove("clientFiles/file_list.txt");
				break;
			}

//****************************************************************************************************************************************
		case GET:
			printf("==============================================================\n");
			printf("\t\t\t  GET REQUEST\n");
			printf("==============================================================\n");


			control = sendto(client_sock, (void *)&get, sizeof(int), 0, (struct sockaddr *)&server_address, addr_len);
			if (control < 0) {
				printf("CLIENT: request failed (sending)\n");
				exit(-1);
			}
			
			printf("> Type the file name to download: ");
			memset(buff, 0, sizeof(buff));
			scanf("%s", buff);
			char *aux = calloc(PKT_SIZE, sizeof(char));
			snprintf(aux, 12+strlen(buff)+1, "clientFiles/%s", buff);
			fd = open(aux, O_RDONLY);


			// Il file è già presente nella cartella del client. Viene chiesto all'utente se vuole sovrascriverlo o meno. 
			if(fd>0){
				char overwrite;
				printf("> File already exists in local directory. Do you want to overwrite the file? [Y/N]: ");
				scanf(" %c", &overwrite);
				if (overwrite == 'Y' || overwrite == 'y'){
					//continue & overwrite
				}
				else {
					// Invia al server un pacchetto NOVERW per comunicare l'annullamento del download
					sendto(client_sock, NOVERW, strlen(NOVERW), 0, (struct sockaddr *)&server_address, addr_len);
					goto menu;
				}
				remove(aux);
			}
			close(fd);


			// Invia al server il nome del file da scaricare
			control = sendto(client_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&server_address, addr_len);
			if (control < 0) {
				printf("CLIENT: request failed (sending)\n");
				exit(-1);
			}
			snprintf(path, 12+strlen(buff)+1, "clientFiles/%s", buff);
			fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0666);


			// Ricevo la risposta dal server se il file è presente o meno.
			if (recvfrom(client_sock, buff, strlen(NFOUND), 0, (struct sockaddr *)&server_address, &addr_len) < 0) {
				printf("CLIENT: error recvfrom\n");
			}


			// Ricevo NFOUND, il file non è presente sul server
			if (strncmp(buff, NFOUND, strlen(NFOUND)) == 0) {
				printf("CLIENT: file not found on server\n");
				close(fd);
				remove(path);
				exit(-1);
			}
			
			// Inizia la ricezione del file
			tcp_receiver(client_sock, &server_address,fd,CLIENT);
			close(fd);
			break;

//****************************************************************************************************************************************
		case PUT:
			printf("==============================================================\n");
			printf("\t\t\t  PUT REQUEST\n");
			printf("==============================================================\n");


			// Mostro a schermo la lista di file presenti nella cartella del client
			printf("__________________ File available to upload __________________\n\n");
			num_files = client_folder_files(list_files);
			int i;
			for (i = 0; i < num_files; i++) {
				printf("%s\n", list_files[i]);	
			}


			// Chiedo il nome del file da caricare sul server
			printf("______________________________________________________________\n\n");
			printf("\n> Type the file name to upload: ");
			memset(buff, 0, sizeof(buff));
			scanf("%s", buff);
			snprintf(path, 12+strlen(buff)+1, "clientFiles/%s", buff);
			fd = open(path, O_RDONLY);


			// Se il file non è presente nel client non inoltro la richiesta di PUT al server
			if(fd == -1){
				printf("> CLIENT: file not found\n");
				goto menu;
			}


			// Invio la richiesta di PUT al server
			control = sendto(client_sock, (void *)&put, sizeof(int), 0, (struct sockaddr *)&server_address, sizeof(server_address));
			if (control < 0) {
				printf("> CLIENT: request failed (sending)\n");
				exit(-1);
			}
			

			// Comunico al server il nome del file che voglio caricare
			printf ("%s CLIENT: Sending File: %s\n", time_stamp(),buff);
			control = sendto(client_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&server_address, addr_len);
			if (control < 0) {
				printf("CLIENT: request failed (sending)\n");
				exit(-1);
			}
			memset(buff, 0, sizeof(NOVERW));


			// Il client resta in attesa di conferma dal server prima di caricare il file
			printf ("%s CLIENT: Attesa permesso upload\n", time_stamp());
			control = recvfrom(client_sock, buff, strlen(NOVERW), 0, (struct sockaddr *)&server_address, &addr_len);


			// Se ricevo NOVERW il file che voglio caricare è già presente sul server. Viene annullato l'upload.
			if (strcmp(buff,NOVERW) == 0){
				printf ("%s CLIENT: File already exists on server, upload canceled\n", time_stamp());
				break;
			}
			printf ("%s Upload authorization granted\n", time_stamp());


			// Inizio l'invio del file
			tcp_sender(client_sock, &server_address,fd,CLIENT);
			break;

//****************************************************************************************************************************************
	case CLOSE:
		control = sendto(client_sock, (void *)&close_conn, sizeof(int), 0, (struct sockaddr *)&server_address, sizeof(server_address));
		if (control < 0) {
			printf("CLIENT: request failed (sending)\n");
			exit(-1);
		}
		client_reliable_close(client_sock, &server_address);
		return 0;

//**************************************************************************************************************************************
	default: 
		printf ("Wrong request\n");
		fflush(stdin);
		break;
  	}
  goto menu;
}


// Crea la socket ed effettua il bind
void client_setup_conn (int *client_sock, struct sockaddr_in *server_addr) {

	// Creazione della socket
	if ((*client_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("CLIENT: socket creation error\n");
		exit(-1);
	}

	// Configurazione della socket
	memset((void *)server_addr, 0, sizeof(*server_addr));
	server_addr->sin_family = AF_INET;
	server_addr->sin_port = htons(SERVER_PORT);

	if (inet_aton(SERVER_IP, &server_addr->sin_addr) == 0) {
		printf("CLIENT: ip conversion error\n");
		exit(-1);
	}
}


// Stabilisce la connessione con il server tramite 3-way handshake
void client_reliable_conn (int client_sock, struct sockaddr_in *server_addr) {
	int control;
	char *buff = calloc(PKT_SIZE, sizeof(char));
	socklen_t addr_len = sizeof(*server_addr);


	// Invio del SYN
	printf("\n===================== CONNECTION SETUP =======================\n");
    printf("%s CLIENT: sending SYN\n", time_stamp());
	control = sendto(client_sock, SYN, strlen(SYN), 0, (struct sockaddr *)server_addr, addr_len);
	if (control < 0) {
		printf("CLIENT: connection failed (sending SYN)\n");
		exit(-1);
	}


	// In attesa del SYNACK
  	printf("%s CLIENT: waiting SYNACK\n", time_stamp());
	memset(buff, 0, sizeof(buff));
	control = recvfrom(client_sock, buff, strlen(SYNACK), 0, (struct sockaddr *)server_addr, &addr_len);
	if (control < 0 || strncmp(buff, SYNACK, strlen(SYNACK)) != 0) {
		printf("CLIENT: connection failed (receiving SYNACK)\n");
		exit(-1);
	}


	// Invio del ACK
    printf("%s CLIENT: sending ACK\n", time_stamp());
	control = sendto(client_sock, ACK, strlen(ACK), 0, (struct sockaddr *)server_addr, addr_len);
	if (control < 0) {
		printf("CLIENT: connection failed (sending ACK)\n");
		exit(-1);
	}


	// Connessione stabilita
	printf("%s CLIENT: connection established\n", time_stamp());
	printf("==============================================================\n\n");
}


// Chiude la connessione con il server in modo affidabile
void client_reliable_close (int client_sock, struct sockaddr_in *server_addr) {
	int control;
	char *buff = calloc(PKT_SIZE, sizeof(char));
	socklen_t addr_len = sizeof(*server_addr);
	
	// Invio del FIN
	printf("\n===================== CONNECTION CLOSE =======================\n");
	printf("%s CLIENT: invio FIN\n", time_stamp());
	control = sendto(client_sock, FIN, strlen(FIN), 0, (struct sockaddr *)server_addr, addr_len);
	if (control < 0) {
		printf("CLIENT: close failed (sending FIN)\n");
		exit(-1);
	}//Se un client non riesce a ben connettersi, il server non forka


	// In attesa del FINACK
	memset(buff, 0, sizeof(buff));
	control = recvfrom(client_sock, buff, strlen(FINACK), 0, (struct sockaddr *)server_addr, &addr_len);
	if (control < 0 || strncmp(buff, FINACK, strlen(FINACK)) != 0) {
		printf("CLIENT: close connection failed (receiving FINACK)\n");
		exit(-1);
	}
	printf("%s CLIENT: ricevuto FINACK\n", time_stamp());


	// In attesa del FIN
	memset(buff, 0, sizeof(buff));
	control = recvfrom(client_sock, buff, strlen(FIN), 0, (struct sockaddr *)server_addr, &addr_len);
	if (control < 0 || strncmp(buff, FIN, strlen(FIN)) != 0) {
		printf("CLIENT: close connection failed (receiving FIN)\n");
		exit(-1);
	}
	printf("%s CLIENT: ricevuto FIN\n", time_stamp());
	

	// Invio del FINACK
	printf("%s CLIENT: invio FINACK\n", time_stamp());
	control = sendto(client_sock, FINACK, strlen(FINACK), 0, (struct sockaddr *)server_addr, addr_len);
	if (control < 0) {
		printf("CLIENT: close connection failed (sending FINACK)\n");
		exit(-1);
	}		
	

	// Connessione chiusa
	printf("CLIENT: connection closed\n");
	printf("==============================================================\n\n");
}


/*Apre la cartella e prende tutti i nomi dei file presenti in essa,
inserendoli in un buffer e ritornando il numero di file presenti */
int client_folder_files(char *list_files[MAX_FILE_LIST]) {
	int i = 0;
	DIR *dp;
	struct dirent *ep;
	for(; i < MAX_FILE_LIST; ++i) {
		if ((list_files[i] = malloc(MAX_NAMEFILE_LEN * sizeof(char))) == NULL) {
			perror("malloc list_files");
			exit(EXIT_FAILURE);
		}
	}

	dp = opendir(CLIENT_FOLDER);
	if(dp != NULL){
		i = 0;
		while((ep = readdir(dp))) {
		if(strncmp(ep->d_name, ".", 1) != 0 && strncmp(ep->d_name, "..", 2) != 0){
			strncpy(list_files[i], ep->d_name, MAX_NAMEFILE_LEN);
			++i;
		}
		}
		closedir(dp);
	}else{
		perror ("Couldn't open the directory");
	}
	return i;
}