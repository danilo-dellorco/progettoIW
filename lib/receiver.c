#include "comm.h"
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
#include <signal.h>
#include <fcntl.h>
#include <sys/param.h>
#include "utility.h"

int ReceiveBase, WindowEnd; // Tengono traccia della base e della fine della finestra di ricezione
int tot_pkts;				// Numero totale di pacchetti da ricevere
int tot_received;			// Contatore del numero di pacchetti bufferizzati correttamente
//int num_packet_lost;		// Conta il numero di pacchetti persi (utile per l'analisi delle prestazioni)
int *check_pkt_received;	// Array di interi che mantiene lo stato di ricezione dei pacchetti
bool allocated;				// Permette di allocare le risorse soltanto alla prima passata nel ciclo while

packet new_pkt, *pkt;
socklen_t addr_len = sizeof(struct sockaddr_in);
struct sockaddr_in *client_addr;

void checkSegment( struct sockaddr_in *, int socket);
void send_cumulative_ack(int ack_number, int sock);
void mark_recvd(int seq);
int is_received(int seq);
void move_window();
void initialize_recv();




// Gestisce la ricezioni di pacchetti secondo il protocollo TCP
int receiver(int socket, struct sockaddr_in *sender_addr, int fd){
	srand (time(NULL)); // Randomizza la scelta dei numeri per la simulazione della perdita dei pacchetti
	initialize_recv();
	socklen_t addr_len=sizeof(struct sockaddr_in);
	client_addr = sender_addr;
	long i = 0;

	printf("=========================================\n\n");
	printf("> File transfer started\n> Wait...\n");

	while(tot_received != tot_pkts){							
		memset(&new_pkt, 0, sizeof(packet));

recv_start:
		if((recvfrom(socket, &new_pkt, PKT_SIZE, 0, (struct sockaddr *)sender_addr, &addr_len)<0)) {
			perror("error receive pkt ");
			continue;
		}

		// Alloca le risorse per i pacchetti in ricezione e per l'array di interi che tiene traccia dei pacchetti ricevuti
		if (!allocated){
			// Scarto eventuali pacchetti restati in volo dalle precedenti ritrasmissioni 
			if (new_pkt.seq_num != 1){
				//printf ("Scartato Pacchetto spazzatura: %d\n",new_pkt.seq_num); 
				//goto recv_start;
			}
			tot_pkts = new_pkt.num_pkts;
			pkt=calloc(tot_pkts, sizeof(packet));
			check_pkt_received=calloc(tot_pkts, sizeof(int));
			allocated = true;
		}

		// Il pacchetto è arrivato correttamente e viene gestito da TCP

			//printf ("%s Ricevuto:%d | Atteso:%d | Finestra [%d:%d]\n",time_stamp(), new_pkt.seq_num, ReceiveBase, ReceiveBase, WindowEnd);
			
			// Arrivo di un nuovo pacchetto non in ordine
			if (ReceiveBase < new_pkt.seq_num && new_pkt.seq_num <= WindowEnd && !is_received(new_pkt.seq_num)){
				mark_recvd(new_pkt.seq_num);
				print_percentage(tot_received,tot_pkts,tot_received-1);
				memset(pkt+new_pkt.seq_num-1, 0, sizeof(packet));
				pkt[new_pkt.seq_num-1] = new_pkt;
			}

			// Arrivo ordinato di segmento con numero di sequenza atteso
			else if (new_pkt.seq_num == ReceiveBase){
				mark_recvd(new_pkt.seq_num);
				print_percentage(tot_received,tot_pkts,tot_received-1);
				move_window();
				memset(pkt+new_pkt.seq_num-1, 0, sizeof(packet));
				pkt[new_pkt.seq_num-1] = new_pkt;
			}

			send_cumulative_ack(ReceiveBase, socket);

	}

	// Ricevuti tutti i pacchetti termino la trasmissione
	printf("\n\n================ Transmission end =================\n");
	//printf("Pacchetti da scrivere: %d\nPacchetti persi: %d\n", tot_pkts,num_packet_lost);

	// Scrivo un pacchetto per volta in ordine sul file
	for(i=0; i<tot_pkts; i++){
		write(fd, pkt[i].data, pkt[i].pkt_dim);
	}
	//printf("===================================================\n");

}

// Invia un ACK cumulativo al mittente
void send_cumulative_ack(int ack_number, int sock){
	if(sendto(sock, &ack_number, sizeof(int), 0, (struct sockaddr *)client_addr, addr_len) < 0) {
		perror("Error send ack\n");
		return;
	}
	//printf ("%s INVIO ACK: %d\n",time_stamp(),ack_number);
}

// Segna come ricevuto il pacchetto con il numero di sequenza passato come parametro e aumenta il contatore del totale di pacchetti ricevuti
void mark_recvd(int seq){
	check_pkt_received[seq-1] = 1;
	tot_received++;
}

// Ritorna 1 se il pacchetto con il numero di sequenza passato come parametro è stato ricevuto, 0 altrimenti
int is_received(int seq){
	if (check_pkt_received[seq-1] == 1){
		return 1;
	}
	return 0;
 }

// Trasla in avanti la finestra di ricezione fino al primo pacchetto non ancora ricevuto.
void move_window(){
	int j = ReceiveBase;
	for (j = ReceiveBase;j<=WindowEnd;j++){
		if (is_received(j)){
			ReceiveBase++;
			WindowEnd = MIN(ReceiveBase + RECV_WIN,tot_pkts);
		}
		else{
			break;
		}
	}
}

// Inizializza le variabili utilizzate per la ricezione di pacchetti
void initialize_recv(){
	ReceiveBase = 1;
	WindowEnd = RECV_WIN;
	tot_pkts = 1;
	allocated = false;
	tot_received = 0;
	//num_packet_lost = 0;
}

