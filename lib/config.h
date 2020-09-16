#include <stdint.h>

// PARAMETRI DI DEFAULT
#define SERVER_PORT 25490
#define SERVER_IP 	"127.0.0.1"

// DEFINIZIONE DEI MESSAGGI DEI PACCHETTI
#define ACK 	"ack"
#define SYN 	"syn"
#define SYNACK 	"synack"
#define READY 	"ready"
#define FIN 	"fin"
#define FINACK 	"ackfin"
#define FOUND 	"found"
#define NFOUND 	"notfound"
#define NOVERW 	"nooverwr"
#define CORRUP 	"corruptd"


// DEFINIZIONE DEI COMANDI
#define LIST 	1
#define GET 	2
#define PUT 	3
#define CLOSE 	4

// PARAMETRI SULLA TRASMISSIONE
#define LOST_PROB 	15				// Probabilità di perdita sul pacchetto -> LOST PROB ∈ [0,100]
#define TRAN_WIN 	64				// Dimensione della finestra di trasmissione
#define RECV_WIN	64				// Dimensione della finestra di ricezione
#define PKT_SIZE 	1500			// Dimensione del pacchetto
#define MAX_RTO 	300000			// Valore massimo del timeout di ritrasmissione in microsecondi -> MAX_RTO < 1000000
#define MIN_RTO 	300				// Valore minimo del timeout di ritrasmissione in microsecondi
#define CLEAN_TIME 	500000			// Tempo di attesa al termine della ricezione -> CLEAN_TIME < 1000000
#define STATIC_RTO	1				// 0 = Timeout Adattivo | 1 = Timeout Statico
#define RTO_VALUE	10000			// Valore del RTO nel caso in cui si scelga di non utilizzare quello adattivo


// PARAMETRI SULLE CARTELLE CLIENT/SERVER
#define CLIENT_FOLDER "./clientFiles/"
#define SERVER_FOLDER "./serverFiles/"


// PARAMETRI SUI FILE
#define MAX_FILE_LIST	 100		// Massimo numero di file mostrati nella lista
#define MAX_NAMEFILE_LEN 127		// Massimo numero di caratteri mostrato nel filename


// COSTANTI PER IL CALCOLO DEL TIMEOUT
#define ALPHA 0.125
#define BETA  0.250

// ALTRE COSTANTI
#define CLIENT "client"				// Specifica il client come chiamante della funzione sender_tcp/receiver_tcp
#define SERVER "server"				// Specifica il server come chiamante della funzione sender_tcp/receiver_tcp



// DEFINIZIONE DELLA STRUTTURA DI UN PACCHETTO
typedef struct packet{
	//Header
	int seq_num;
	short int pkt_dim;
	int num_pkts;					//Indica il numero totale dei pacchetti del file
	uint64_t sent_time; 			//Tempo di invio del pkt in microsecondi

	//Payload
	char data[PKT_SIZE-2*sizeof(int)-sizeof(short int)];
} packet;

typedef struct ready_pkt{
    char message[5];
    int clientNum;
}ready_pkt;