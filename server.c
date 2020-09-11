#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include "./lib/comm.h"
#include "./lib/sender.h"
#include "./lib/receiver.h"
#include "./lib/utility.h"

int server_folder_files(char *list_files[MAX_FILE_LIST]);
void server_setup_conn( int *, struct sockaddr_in *);
int server_reliable_conn(int , struct sockaddr_in *); // vedere se per tcp va bene
void server_reliable_close (int server_sock, struct sockaddr_in* client_addr);




int main(int argc, char **argv){
    int server_sock;
    struct sockaddr_in server_address, client_address;
    socklen_t addr_len = sizeof(client_address);
    char *buff = calloc(PKT_SIZE, sizeof(char));
    char *path = calloc(PKT_SIZE, sizeof(char));
    char *buffToSend = calloc(PKT_SIZE, sizeof(char));
    FILE *fptr;
		int fd;
    int control, num_files;
		char *list_files[MAX_FILE_LIST];

    server_setup_conn(&server_sock, &server_address);
    server_reliable_conn(server_sock, &client_address);

    while (1) {

    request:
        printf("\n\n=======================================\n");
        printf("> Server waiting for request....\n");
        memset(buff, 0, sizeof(buff));

        if (recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&client_address, &addr_len) < 0){
            perror("ERRORE COMANDO");
            free(buff);
            free(path);
            return 0;
        }



        switch (*(int*) buff ){

//****************************************************************************************************************************************
            case LIST:
                printf("> LIST request\n");
                printf("=======================================\n\n");
                num_files = server_folder_files(list_files);

                fd = open("file_list.txt", O_CREAT | O_TRUNC | O_RDWR, 0666);
                if(fd<0){
                    printf("SERVER: error opening file_list\n");
                    close(server_sock);
                    return 1;
                 }

                // Scrivo tutti i file in serverFiles nel file che verrà inviato al client
                int i=0;
                while(i<num_files) {
                    memset(buff, 0, sizeof(buff));
                    snprintf(buff, strlen(list_files[i])+2, "%s\n", list_files[i]); //+2 per terminatore di stringa e \n
                    write(fd, buff, strlen(buff));
                    i++;
                }

                read(fd, (void *)&buffToSend, strlen(buffToSend));

                // Inizio l'invio del file
                sender(server_sock, &client_address,fd);
                close(fd);
                remove("file_list.txt");
                break;

//****************************************************************************************************************************************
            case GET:
                printf("> DOWNLOAD request\n");
                memset(buff, 0, sizeof(buff));
                control = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&client_address, &addr_len); 
                if (control < 0) {
                    printf("SERVER: file transfer failed (1)\n");
                    perror("ERROR");
                    free(buff);
                    free(path);
                    close(server_sock);
                    return 1;
                }

                // +1 per lo /0 altrimenti lo sostituisce all' ultimo carattere
                snprintf(path, 12+strlen(buff)+1, "serverFiles/%s", buff); 
                fd = open(path, O_RDONLY);
                printf("> Sending %s\n",path);
                if(fd == -1){

                    // Il client ha rifiutato di sovrascrivere il file
                    if (strcmp(buff,NOVERW) == 0){
                        printf ("Download annullato dal client\n");
                        goto request;
                    }                    
                  
                    // Comunico al client che il file non è presente
                    printf("SERVER: file not found\n");
                    if (sendto(server_sock, NFOUND, strlen(NFOUND), 0, (struct sockaddr *)&client_address, addr_len) < 0) {
                          printf("SERVER: error sendto\n");
                          goto request;
                    }

                    free(buff);
                    free(path);
                    close(server_sock);
                    return 1;
                }

                // Comunico al client che il file è presente e può essere scaricato
                if (sendto(server_sock, FOUND, strlen(FOUND), 0, (struct sockaddr *)&client_address, addr_len) < 0) {
                    printf("SERVER: error sendto\n");
                    return 1;
                }
                printf("=======================================\n\n");

                // Inizio l'invio del file
                sender(server_sock, &client_address,fd);          
                break;

//**************************************************************************************************************************************
            case PUT:
                printf("> UPLOAD request\n");
                printf("=======================================\n\n");
                memset(buff, 0, sizeof(buff));
                control = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&client_address, &addr_len);
                if (control < 0) {
                    printf("%s SERVER: file transfer failed (1)\n",time_stamp());
                    free(buff);
                    free(path);
                    close(server_sock);
                    return 1;
                }
                snprintf(path, 12+strlen(buff)+1, "serverFiles/%s", buff);

                // Il file è già presente nel server, invia al client NOVERW per annullare l'upload.
                fd = open(path, O_RDONLY);
                if(fd>0){
                    printf("%s SERVER: The file already exists, you can not overwrite files on server.\n", time_stamp());
                    sendto(server_sock, NOVERW, strlen(NOVERW), 0, (struct sockaddr *)&client_address, addr_len);
                    close(fd);
                    goto request;
                }

                // Il file non è presente nel server, invia al client NFOUND per confermare di poter caricare il file
                sendto(server_sock, NFOUND, strlen(NFOUND), 0, (struct sockaddr *)&client_address, addr_len);
                close(fd);
                fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0666);

                // Inizio la ricezione del file
                control=receiver(server_sock, &client_address,fd);
                if(control == -1) {
                    close(fd);
                    remove(path);
                    free(buff);
                    free(path);
                    return 1;
                }
                close(fd);
                break;

//**************************************************************************************************************************************
            case CLOSE:
                server_reliable_close(server_sock, &client_address);
                free(buff);
                free(path);
                close(server_sock);
                return 0;

//**************************************************************************************************************************************
            default:
                printf("Wrong request\n\n");
                break;  
        }
        goto request;
    }
}


// Crea la socket ed effettua il bind
void server_setup_conn( int *server_sock, struct sockaddr_in *server_addr){
    
    // Creazione della socket
    if ((*server_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("SERVER: socket creation error \n");
        exit(-1);
    }

    // Configurazione della socket
    memset((void *)server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(SERVER_PORT);
    server_addr->sin_addr.s_addr = htonl(INADDR_ANY);


    /* htons = host-to-network
    converte porta da rappresentazione/binaria dell'indirizzo/numero di porta
    a valore binario da inserire nella struttura sockaddr_in*/

    /* associa al socket l'indirizzo e porta locali, serve a far sapere al SO a quale processo vanno inviati i dati ricevuti dalla rete*/
    /* sockfd = descrittore socket
        addr = puntatore a struck contentente l'indirizzo locale -> RICHIEDE struck sockadrr * addr
        len = dimensione in byte della struct sopra */


    if (bind(*server_sock, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        printf("SERVER: socket bind error\n");
        exit(-1);
    }
}


// Stabilisce la connessione con il client tramite 3-way handshake
int server_reliable_conn (int server_sock, struct sockaddr_in* client_addr) {
    int control;
    char *buff = calloc(PKT_SIZE, sizeof(char));
    socklen_t addr_len = sizeof(*client_addr);

    // In attesa di ricevere SYN
		printf("\n================= CONNECTION SETUP =================\n");
		printf("%s SERVER: attesa syn\n", time_stamp());
    control = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
    if (control < 0 || strncmp(buff, SYN, strlen(SYN)) != 0) {
        printf("SERVER: connection failed (receiving SYN)\n");
        return 1;
    }

    // Invio del SYNACK
		printf("%s SERVER: invio SYNACK\n", time_stamp());
    control = sendto(server_sock, SYNACK, strlen(SYNACK), 0, (struct sockaddr *)client_addr, addr_len);
    if (control < 0) {
        printf("SERVER: connection failed (sending SYNACK)\n");
        return 1;
    }

    // Connessione stabilita
    printf("%s SERVER: ricevuto ACK_SYNACK\n", time_stamp());
    printf("%s SERVER: connection established\n", time_stamp());
    printf("====================================================\n\n");
    return 0;
}


// Chiude la connessione con il client in modo affidabile
void server_reliable_close (int server_sock, struct sockaddr_in* client_addr) {
    int control;
    char *buff = calloc(PKT_SIZE, sizeof(char));
    socklen_t addr_len = sizeof(*client_addr);
   
    printf("\n================= CONNECTION CLOSE =================\n");


    // In attesa di ricevere FIN
    control = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
    if (control < 0 || strncmp(buff, FIN, strlen(FIN)) != 0) {
        printf("SERVER: close connection failed (receiving FIN)\n");
        exit(-1);
    }
    printf("%s SERVER: ricevuto FIN\n", time_stamp());
 	
    // Invio del FINACK
    printf("%s SERVER: invio FINACK\n", time_stamp());
    control = sendto(server_sock, FINACK, strlen(FINACK), 0, (struct sockaddr *)client_addr, addr_len);
    if (control < 0) {
        printf("SERVER: close connection failed (sending FINACK)\n");
        exit(-1);
    }

    // Invio del FIN
    printf("%s SERVER: invio FIN\n", time_stamp());
	  control = sendto(server_sock, FIN, strlen(FIN), 0, (struct sockaddr *)client_addr, addr_len);
    if (control < 0) {
        printf("SERVER: close connection failed (sending FIN)\n");
        exit(-1);
    }

    // In attesa del FINACK
    memset(buff, 0, sizeof(buff));
    control = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
    if (control < 0 || strncmp(buff, FINACK, strlen(FINACK)) != 0) {
        printf("SERVER: close connection failed (receiving FINACK)\n");
        exit(-1);
    }

    // Connessione chiusa
    printf("%s SERVER: ricevuto FINACK\n", time_stamp());
	printf("SERVER: connection closed\n");
	printf("===================================================\n\n");
    return;
}


/*Apre la cartella e prende tutti i nomi dei file presenti in essa,
inserendoli in un buffer e ritornando il numero di file presenti */
int server_folder_files(char *list_files[MAX_FILE_LIST]) {
    int i = 0;
    DIR *dp;
    struct dirent *ep;
    for(; i < MAX_FILE_LIST; ++i) {
        if ((list_files[i] = malloc(MAX_NAMEFILE_LEN * sizeof(char))) == NULL) {
            perror("malloc list_files");
            exit(EXIT_FAILURE);
        }
    }

    dp = opendir(SERVER_FOLDER);
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