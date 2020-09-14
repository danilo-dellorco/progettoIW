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
#include "./lib/config.h"
#include "./lib/tcp_sender.h"
#include "./lib/tcp_receiver.h"
#include "./lib/utility.h"

int server_folder_files(char *list_files[MAX_FILE_LIST]);
void server_setup_conn( int *, struct sockaddr_in *);
int server_reliable_conn(int , struct sockaddr_in *);
void server_reliable_close (int server_sock, struct sockaddr_in* client_addr);




int main(int argc, char **argv){
    int server_sock, child_sock;
    struct sockaddr_in server_address, client_address;
    socklen_t addr_len = sizeof(client_address);
    char *buff = calloc(PKT_SIZE, sizeof(char));
    char *path = calloc(PKT_SIZE, sizeof(char));
    char *buffToSend = calloc(PKT_SIZE, sizeof(char));
    FILE *fptr;
	pid_t pid;
    int fd;
    int control, num_files, num_client=0;
	char *list_files[MAX_FILE_LIST];
    ready_pkt send_ready_pkt;
    
    clearScreen(); 
    server_setup_conn(&server_sock, &server_address);
    printf ("%s SERVER: Waiting for client connections\n",time_stamp());

    while (1) {

        // Eseguo la fork del server per ogni nuova connessione da parte di un client
        if (server_reliable_conn(server_sock, &client_address) == 0){
        pid = fork();
        num_client++;
        
        if (pid < 0){
            printf("> SERVER: fork error\n");
            exit(-1);
        }
        if (pid == 0){
            pid = getpid();
            child_sock = create_socket();
            send_ready_pkt.clientNum = num_client;
            snprintf(send_ready_pkt.message,6,"%s",READY);

            // Invio del pacchetto di READY che comunica al client il suo ID di connessione
            control = sendto(child_sock, &send_ready_pkt, sizeof(send_ready_pkt),0, (struct sockaddr *)&client_address, addr_len);
            if (control < 0) {
                printf("> SERVER %d: port comunication failed\n", pid);
            }

request:    
            // Server in attesa di un messaggio di richiesta dal client
            printf("\n\n> Server waiting for request from clients\n");
            memset(buff, 0, sizeof(buff));

            if (recvfrom(child_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&client_address, &addr_len) < 0){
                printf("> request failed\n");
                free(buff);
                free(path);
                close(child_sock);
                return 0;
            }


            switch (*(int*) buff ){
 //****************************************************************************************************************************************
            case LIST:
                printf("> LIST request received from client: %d\n", num_client);
                num_files = server_folder_files(list_files);

                fd = open("file_list.txt", O_CREAT | O_TRUNC | O_RDWR, 0666);
                if(fd<0){
                    printf("> SERVER: error opening file_list\n");
                    close(child_sock);
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
                tcp_sender(child_sock, &client_address,fd,SERVER);
                close(fd);
                remove("file_list.txt");
                break;

//****************************************************************************************************************************************
            case GET:
                printf("> DOWNLOAD request from client: %d\n", num_client);
                memset(buff, 0, sizeof(buff));
                control = recvfrom(child_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&client_address, &addr_len); 
                if (control < 0) {
                    printf("> SERVER: file transfer failed (1)\n");
                    free(buff);
                    free(path);
                    close(child_sock);
                    return 1;
                }


                // +1 per lo /0 altrimenti lo sostituisce all' ultimo carattere
                snprintf(path, 12+strlen(buff)+1, "serverFiles/%s", buff); 
                fd = open(path, O_RDONLY);
                printf("> Sending %s to client %d\n",path, num_client);

                if(fd == -1){
                    // Il client ha rifiutato di sovrascrivere il file
                    if (strcmp(buff,NOVERW) == 0){
                        printf ("SERVER: Download canceled by client %d\n", num_client);
                        goto request;
                    }                    
                
                    // Comunico al client che il file non è presente
                    printf("> SERVER: file not found\n");
                    if (sendto(child_sock, NFOUND, strlen(NFOUND), 0, (struct sockaddr *)&client_address, addr_len) < 0) {
                        printf("> SERVER: error sendto\n");
                        goto request;
                    }

                    free(buff);
                    free(path);
                    close(child_sock);
                    return 1;
                }


                // Comunico al client che il file è presente e può essere scaricato
                if (sendto(child_sock, FOUND, strlen(FOUND), 0, (struct sockaddr *)&client_address, addr_len) < 0) {
                    printf("> SERVER: error sendto\n");
                    return 1;
                }


                // Inizio l'invio del file
                tcp_sender(child_sock, &client_address,fd,SERVER);          
                break;

//**************************************************************************************************************************************
            case PUT:
                printf("> UPLOAD request from client: %d\n", num_client);
                memset(buff, 0, sizeof(buff));


                // In attesa di ricevere il nome del file 
                control = recvfrom(child_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&client_address, &addr_len);
                if (control < 0) {
                    printf("%s SERVER: file transfer failed (1) with client %d\n",time_stamp(), num_client);
                    free(buff);
                    free(path);
                    close(child_sock);
                    return 1;
                }
                snprintf(path, 12+strlen(buff)+1, "serverFiles/%s", buff);
                printf ("> Receiving file: %s from client %d\n",buff, num_client);


                // Il file è già presente nel server, invia al client NOVERW per annullare l'upload.
                fd = open(path, O_RDONLY);
                if(fd>0){
                    printf("> SERVER: The file %s already exists on server, upload denied.\n", buff);
                    sendto(child_sock, NOVERW, strlen(NOVERW), 0, (struct sockaddr *)&client_address, addr_len);
                    close(fd);
                    goto request;
                }


                // Il file non è presente nel server, invia al client NFOUND per confermare di poter caricare il file
                sendto(child_sock, NFOUND, strlen(NFOUND), 0, (struct sockaddr *)&client_address, addr_len);
                close(fd);
                fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0666);


                // Inizio la ricezione del file
                tcp_receiver(child_sock, &client_address,fd,SERVER);

                close(fd);
                break;

//**************************************************************************************************************************************
            case CLOSE:
                server_reliable_close(child_sock, &client_address);
                free(buff);
                free(path);
                close(child_sock);
                num_client--;                               // NON FUNZIONA PERCHE' IL FIGLIO MODIFICA LE SUE VARIABILI LOCALI
                return 0;

//**************************************************************************************************************************************
            default:
                printf("Wrong request from client: %d\n\n", num_client);
                break; 
            }
            goto request;
            }

        }
    }
    return 0;
}


// Crea la socket ed effettua il bind
void server_setup_conn( int *server_sock, struct sockaddr_in *server_addr){
    
    // Creazione della socket
    if ((*server_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("> SERVER: socket creation error \n");
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
        printf("> SERVER: socket bind error\n");
        exit(-1);
    }
}


// Stabilisce la connessione con il client tramite 3-way handshake
int server_reliable_conn (int server_sock, struct sockaddr_in* client_addr) {
    int control;
    char *buff = calloc(PKT_SIZE, sizeof(char));
    socklen_t addr_len = sizeof(*client_addr);

    // In attesa di ricevere SYN
    control = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
    if (control < 0 || strncmp(buff, SYN, strlen(SYN)) != 0) {
        printf("SERVER: connection failed (receiving SYN)\n");
        return 1;
    }
    printf("\n===================== CONNECTION SETUP =======================\n");
    printf("%s SERVER: Received syn\n", time_stamp());

    // Invio del SYNACK
	printf("%s SERVER: sending SYNACK\n", time_stamp());
    control = sendto(server_sock, SYNACK, strlen(SYNACK), 0, (struct sockaddr *)client_addr, addr_len);
    if (control < 0) {
        printf("SERVER: connection failed (sending SYNACK)\n");
        return 1;
    }

    // In attesa di ricevere ACK
	printf("%s SERVER: waiting ACK\n", time_stamp());
    control = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
    if (control < 0 || strncmp(buff, ACK, strlen(ACK)) != 0) {
        printf("SERVER: connection failed (receiving ACK)\n");
        return 1;
    }

    // Connessione stabilita
    printf("%s SERVER: connection established\n", time_stamp());
    printf("==============================================================\n");
    return 0;
}


// Chiude la connessione con il client in modo affidabile
void server_reliable_close (int server_sock, struct sockaddr_in* client_addr) {
    int control;
    char *buff = calloc(PKT_SIZE, sizeof(char));
    socklen_t addr_len = sizeof(*client_addr);
   
    printf("\n===================== CONNECTION CLOSE =======================\n");


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
	printf("==============================================================\n\n");
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