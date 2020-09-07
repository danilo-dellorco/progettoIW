#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdbool.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/param.h>
#include "comm.h"
#include "utility.h"

void timeout_routine();
void send_window(int socket, struct sockaddr_in *client_addr, packet *pkt);
void *receive_ack(void *arg);
void update_timeout(packet to_pkt);
void end_transmission();
void retransmission(int rtx_ind, char *message);
void cumulative_ack(int received_ack);
void set_packet_sent(int index);
int send_packet(int index);

// ACK CUMULATIVO UTILIZZATO: Inviare un ACK = N indica che tutti i pacchetti fino a N-1 sono stati ricevuti e che ora aspetto il pacchetto numero N

int SendBase;		// Base della finestra di trasmissione: più piccolo numero di sequenza tra segmenti trasmessi di cui non si è ancora ricevuto ACK
int NextSeqNum;		// Sequence Number del prossimo pacchetto da inviare, quindi primo pacchetto nella finestra ma non ancora in volo
int WindowEnd;		// Fine della finestra di trasmissione
int ack_num;		// Tiene traccia dell'ultimo ACK ricevuto
int tot_acked;		// Contatore del numero totale di pacchetti che sono stati riscontrati correttamente
int tot_pkts;		// Numero dei pacchetti da inviare
int num_packet_lost;
int sock;


bool fileTransfer = true;						// Termina l'invio dei pacchetti quando posto a false
bool isTimerStarted = false;					// Impostato a true quando è in funzione il timer di un pacchetto, false altrimenti
struct timeval transferEnd, transferStart;		// Utilizzati per il calcolo del tempo di trasmissione
socklen_t addr_len;
struct sockaddr_in *client_addr;
off_t file_dim;

int64_t timeoutInterval = 500000, estimatedRTT = 1000, devRTT = 1;	// Valori di default del Retransmission Timeout

/*
Array di interi per tenere traccia dello stato di invio dei pacchetti
	0 = Da Inviare
	1 = Inviato non Acked
	2 = Acked
*/
int *check_pkt;
packet *pkt;

// Ritorna l'orario attuale in microsecondi
uint64_t time_now(){
	struct timeval current;
	gettimeofday(&current, 0);
	return current.tv_sec * 1000000 + current.tv_usec;
}

// Struttura per passare argomenti al thread per la ricezione degli ACK
pthread_t thread;
struct thread_args
 {
    struct sockaddr_in *client_addr;
    socklen_t addr_len;
	int socket;
};

// Inizializza tutti i parametri 
void initialize_send(){
	SendBase = 1;
	NextSeqNum = 1;
	ack_num = 0;
	tot_acked = 0;
	tot_pkts = 0;	
	fileTransfer = true;
	isTimerStarted = false;
	timeoutInterval = 500000;
	estimatedRTT = 1000;
	devRTT = 1;
	num_packet_lost = 0;
}

// Thread che gestisce la ricezione degli ack da parte del ricevente, compresi gli ack duplicati
void *receive_ack(void *arg){
	struct thread_args *args = arg;
	socklen_t addr_len = args->addr_len;
	struct sockaddr_in *client_addr = args->client_addr;
	int socket = args->socket;
	int duplicate_ack_count = 1;
	int old_acked;

	while(true){
		if (recvfrom(socket, &ack_num, sizeof(int), 0, (struct sockaddr *)client_addr, &addr_len) < 0){
			perror ("Errore ricezione ack");
			exit(-1);
		}	
		//printf ("%s SendBase: %d | Ricevuto ACK numero: %d\n",time_stamp(),SendBase,ack_num);

		// Ricevuto ACK non duplicato
		if (ack_num>SendBase){
			old_acked = tot_acked;
			SendBase = ack_num;
			WindowEnd = MIN(tot_pkts,SendBase+TRAN_WIN-1);
			duplicate_ack_count = 1;
			cumulative_ack(ack_num);
			update_timeout(pkt[ack_num-2]);
			print_percentage(tot_acked,tot_pkts,old_acked);
			if (tot_acked == tot_pkts){
				fileTransfer = false; //Stoppa il thread e l'invio dei pacchetti se arrivati alla fine del file
				set_timer(0);
				break;
			}
		}
		
		//Ricevuto ACK duplicato
		else {
			duplicate_ack_count++;
			if (duplicate_ack_count == 3){
				retransmission(SendBase-1, "FAST RETRANSMISSION");
				duplicate_ack_count = 1;
			}
		}
	}
}


void sender(int socket, struct sockaddr_in *receiver_addr, int fd) {
	sock = socket;
	initialize_send();
	client_addr = receiver_addr;
	addr_len = sizeof(struct sockaddr_in);
	char *buff = calloc(PKT_SIZE, sizeof(char));
	int i;

	struct thread_args t_args;
	t_args.addr_len = addr_len;
	t_args.client_addr = receiver_addr;
	t_args.socket = socket;

	int ret = pthread_create(&thread,NULL,receive_ack,(void*)&t_args); //Creazione del thread per la ricezione degli ACK	
	srand(time(NULL));
	
	// Calcolo del numero totale di pacchetti da inviare
	file_dim = lseek(fd, 0, SEEK_END);
	int pkt_data_size = PKT_SIZE-2*sizeof(int)-sizeof(short int);
	if (file_dim%pkt_data_size==0) {
		tot_pkts = file_dim/pkt_data_size;
	}
	else {
		tot_pkts = (file_dim/pkt_data_size)+1;
	}

	printf("\n====== INIZIO TRASMISSIONE PACCHETTI | PKTS: %d ======\n\n",tot_pkts);
	pkt=calloc(tot_pkts, sizeof(packet));
	check_pkt=calloc(tot_pkts, sizeof(int));
	lseek(fd, 0, SEEK_SET);
	gettimeofday(&transferStart, NULL);

	// Assegnazione dei numeri di sequenza ai pacchetti da inviare
	for(i=0; i<tot_pkts; i++){
		pkt[i].seq_num = i+1;
		pkt[i].num_pkts = tot_pkts;
		pkt[i].pkt_dim=read(fd, pkt[i].data, pkt_data_size);
		if(pkt[i].pkt_dim==-1){
			pkt[i].pkt_dim=0;
		}
	}

	// Trasmissione dei pacchetti
	while(fileTransfer){ //while ho pachetti da inviare
		send_window(socket, receiver_addr, pkt);
	}
	end_transmission();
}

//Invia tutti i pacchetti nella finestra
void send_window(int socket, struct sockaddr_in *client_addr, packet *pkt){
	WindowEnd = MIN(tot_pkts,SendBase+TRAN_WIN-1);
	signal(SIGALRM, timeout_routine);
	int i, j;
	socklen_t addr_len = sizeof(struct sockaddr_in);

	// Caso in cui la finestra non è ancora piena di pkt in volo	
	for(i=NextSeqNum-1; i<WindowEnd; i++){
		WindowEnd = MIN(tot_pkts,SendBase+TRAN_WIN-1);

		if (!isTimerStarted){
			set_timer(timeoutInterval);
			isTimerStarted = true;
		}

		if (send_packet(i)>0){
			set_packet_sent(i);
			NextSeqNum++;
			check_pkt[i] = 1;
		}
	}
}

void timeout_routine(){
	isTimerStarted = false;
	retransmission(SendBase-1, "TIMEOUT EXPIRED    ");
	return;
}

void set_packet_sent(int index){
	if (check_pkt[index] == 0){
		pkt[index].sent_time = time_now();
		check_pkt[index] = 1;
	}
}

// Aggiorna l'attuale valore del Retransmission Timeout
void update_timeout(packet to_pkt) {	
	uint64_t recvTime = time_now();
	uint64_t sentTime = to_pkt.sent_time;
	uint64_t sampleRTT = recvTime - sentTime;
	estimatedRTT = (1-ALPHA) * estimatedRTT + ALPHA * sampleRTT;
	devRTT = (1-BETA)*devRTT + BETA * abs(sampleRTT - estimatedRTT);
	timeoutInterval = (estimatedRTT + 4 * devRTT);
}

// Ritrasmette immediatamente il pacchetto passato come parametro
void retransmission(int rtx_ind, char *message){
	if (!fileTransfer){
		return;
	}
	set_packet_sent(rtx_ind);
	send_packet(rtx_ind);
	set_timer(timeoutInterval);
	isTimerStarted = true;
}

// Imposta come acked tutti i pacchetti con numero di sequenza inferiore a quello ricevuto
void cumulative_ack(int received_ack){
	tot_acked = received_ack-1;
	for (int k = SendBase-1; k<received_ack-1; k++){
		check_pkt[k] = 2;
	}	
}

// Stoppa il timer e stampa il tempo impiegato per l'invio del file
void end_transmission(){
	printf("\n\n================ Transmission end =================\n");
	set_timer(0);
	printf("File transfer finished\n");
	gettimeofday(&transferEnd, NULL);
	double tm=transferEnd.tv_sec-transferStart.tv_sec+(double)(transferEnd.tv_usec-transferStart.tv_usec)/1000000;
	double tp=file_dim/tm;
	printf("Transfer time: %f sec [%f KB/s]\n", tm, tp/1024);
	printf("Packets total: %d\n",tot_pkts);
	printf("Packets lost : %d\n", num_packet_lost);
	printf("===================================================\n");
}

int send_packet(int index){
	if (is_packet_lost(LOST_PROB)){
		set_packet_sent(index);
		num_packet_lost++;
		return -1;
	}
	if (sendto(sock, pkt+index, PKT_SIZE, 0, (struct sockaddr *)client_addr, addr_len)<0){
		perror ("Sendto Error");
		num_packet_lost++;
		return -1;
	}
	return 1;
}

