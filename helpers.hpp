#ifndef _HELPERS_HPP
#define _HELPERS_HPP

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <string>
#include <unordered_map>
#include <queue>
#include <list>
#include <cmath>
#include <algorithm>

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define BUFLEN		1551	// dimensiunea maxima a calupului de date
#define MAX_CLIENTS	5	    // numarul maxim de clienti in asteptare
// dimensiunea maxima ce poate fi primita intr-un apel de recv
#define TCP_MAXLEN (sizeof(tcp_message))
#define TOPIC_LEN 50        //lungimea campului topic din structuri
//dimensiunea maxima a unui mesaj de tip comanda
#define COMM_LEN (sizeof(command_message))
#define MAX_ID_LEN 11       //lungimea maxima a ID-ului unui client

using namespace std;

/*
Structura tip mesaj care va fi trimis de la server la subscriberi
*/
typedef struct {
    uint16_t port;
    char ip[16];
    char topic[51];
    char data_type[11];
    char payload[1501];
}__attribute__((packed)) tcp_message;

/*
Structura tip comanda care va fi trimisa de la subscriberi la server
*/
typedef struct {
	uint8_t command_type;
	char topic[51];
	uint8_t sf;
}__attribute__((packed)) command_message;

/*
Structura tip mesaj udp care este trimis de la clientul udp la server
*/
typedef struct {
	char topic[50];
	uint8_t data_type;
	char payload[1501];
}__attribute__((packed)) udp_message;

/*
Structura topic
*/
typedef struct {
	string name;
	bool sf;
}topic;

/*
Structura client care contine:
	- informatia ca este online/offline
	- file descriptorul socketului la care este conectat, daca este online
	- lista de topicuri la care e abonat
	- coada de mesaje in asteptare de la topicurile cu sf 1, cat timp clientul
a fost offline
*/
typedef struct {
	bool is_online;
	int sockfd;
	list<topic> topics;
	queue<tcp_message> queued_messages;
}client;

#endif
