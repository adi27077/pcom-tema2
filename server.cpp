#include "helpers.hpp"

/*
functie care creeaza un mesaj TCP conform tipurilor de date precizate
in enunt, din datele unui mesaj in formatul transmis de clientul UDP
    - mesajul TCP contine datele in format human-readable, pentru a fi
    gata de afisare de catre clientii TCP
*/
void create_tcp_message_from_udp(udp_message *udp_msg, tcp_message *tcp_msg) {
    strncpy(tcp_msg->topic, udp_msg->topic, TOPIC_LEN);

    switch(udp_msg->data_type) {
        //caz datatype: INT
        case 0:
            long long int_number;
            uint8_t sign;
            strcpy(tcp_msg->data_type, "INT");
            sign = udp_msg->payload[0];
            int_number = ntohl(*(uint32_t *)(udp_msg->payload + 1));
            int_number *= (sign == 1) ? -1 : 1;
            sprintf(tcp_msg->payload, "%lld", int_number);
            break;
        //caz datatype: SHORT_REAL
        case 1:
            double short_real;
            strcpy(tcp_msg->data_type, "SHORT_REAL");
            short_real = 1.0 * ntohs(*(uint16_t *)(udp_msg->payload)) / 100;
            sprintf(tcp_msg->payload, "%.2f", short_real);
            break;
        //caz datatype: FLOAT
        case 2:
            double float_number;
            strcpy(tcp_msg->data_type, "FLOAT");
            sign = udp_msg->payload[0];
            float_number = ntohl(*(uint32_t *)(udp_msg->payload + 1));
            float_number /= pow(10, udp_msg->payload[5]);
            float_number *= (sign == 1) ? -1 : 1;
            if(float_number - (int)float_number == 0) {
                sprintf(tcp_msg->payload, "%d", (int)float_number);
            } else {
                //eliminare manuala trailing zeroes
                sprintf(tcp_msg->payload, "%lf", float_number);
                for(int i = strlen(tcp_msg->payload) - 1; i >= 0; i--) {
                    if(tcp_msg->payload[i] == '0') {
                        tcp_msg->payload[i] = '\0';
                    } else {
                        break;
                    }
                }

            }
            break;
        //caz datatype: STRING
        case 3:
            strcpy(tcp_msg->data_type, "STRING");
            strcpy(tcp_msg->payload, udp_msg->payload);
            break;
    }
}

/*
functie care trimite mesajul TCP la clientii TCP astfel:
    - pentru un client online si abonat la topic -> se trimite direct mesajul
    - pentru un client offline, abonat la topic cu sf=1 -> se adauga mesajul
      in coada lui de asteptare
    - pentru celelalte cazuri nu se face nimic
*/
void send_message_to_all_clients(unordered_map<string, client> &all_clients,
                                tcp_message tcp_msg) {

    string current_topic(tcp_msg.topic);

    for(auto &entry : all_clients) {
        //caz in care clientul este online
        auto client = entry.second;
        if(client.is_online) {
            //verificam daca clientul este abonat la topicul mesajului curent
            if(find_if(client.topics.begin(), client.topics.end(), [&](const topic &t) {
                if(t.name == current_topic)
                    return true;
                return false;
            }) != client.topics.end()) {
                //trimitem mesajul catre client
                int ret = send(client.sockfd, (char *)&tcp_msg, TCP_MAXLEN, 0);
                DIE(ret < 0, "Can't send message to client\n");
            }
        } else {  //caz in care clientul este offline
            /*verificam daca clientul este abonat la topicul mesajului curent
            si daca are sf=1*/
            if(find_if(client.topics.begin(), client.topics.end(), [&](const topic &t) {
                if(t.name == current_topic && t.sf == true)
                    return true;
                return false;
            }) != client.topics.end()) {
                //adaugam mesajul in coada clientului
                entry.second.queued_messages.push(tcp_msg);
            }
        }  
    }
}

/*
functie care gaseste clientul in hashmap dupa socketul pe care acesta este conectat
*/
unordered_map<string, client>::iterator find_by_sock(unordered_map<string, client> &all_clients,
                                                    int sockfd) {
    
    auto cli_iterator = find_if(all_clients.begin(), all_clients.end(),
                        [&](const pair<string, client> &entry) {
                            if(entry.second.is_online && entry.second.sockfd == sockfd)
                                return true;
                            return false;
                        });
    return cli_iterator;
}

/*
functie care inchide toti socketii existenti
*/
void close_sockets(int fdmax, fd_set *read_fds) {
    for(int i = 3; i <= fdmax; i++) {
        if(FD_ISSET(i, read_fds)) {
            FD_CLR(i, read_fds);
            close(i);
        }
    }
}

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    
    int udp_sockfd, tcp_sockfd, newsockfd, portno;
	char buffer[BUFLEN], command_buffer[COMM_LEN];
	struct sockaddr_in udp_addr, tcp_addr, cli_addr;
	int i, ret;
	socklen_t udp_socklen = sizeof(sockaddr), tcp_socklen = sizeof(sockaddr);
    socklen_t cli_socklen = sizeof(sockaddr);

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc < 2) {
		usage(argv[0]);
	}

    portno = atoi(argv[1]);
    DIE(portno == 0, "Can't parse port\n");
    DIE(portno <= 1024, "Can't use this port. Please try another\n");

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
    
    //tipurile de mesaje trimise intre server si clienti
    udp_message udp_msg;
    tcp_message tcp_msg;
    command_message comm_msg;

    /* hashmap ce va contine toti clientii
        - cheie: ID-ul clientului
        - valoare: structura clientlui
    */
    unordered_map<string, client> all_clients;

    //initializare socketi pt clientii upd si tcp
    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_sockfd < 0, "UDP socket initialization error\n");

    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_sockfd < 0, "TCP socket initialization error\n");

    memset((char *) &udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(portno);
    udp_addr.sin_addr.s_addr = INADDR_ANY;

    memset((char *) &tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_port = htons(portno);
    tcp_addr.sin_addr.s_addr = INADDR_ANY;

    //bind socketi
    ret = bind(udp_sockfd, (sockaddr *) &udp_addr, udp_socklen);
    DIE(ret < 0, "Can't bind UDP socket\n");

    ret = bind(tcp_sockfd, (sockaddr *) &tcp_addr, tcp_socklen);
    DIE(ret < 0, "Can't bind TCP socket\n");

    //ascultare pt conexiuni TCP
    ret = listen(tcp_sockfd, MAX_CLIENTS);
    DIE(ret < 0, "Error listening for TCP connections\n");

    //adaugare socketi si stdin in multimea de descriptori pt cititre
    FD_SET(udp_sockfd, &read_fds);
    FD_SET(tcp_sockfd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    fdmax = tcp_sockfd;

    bool server_is_running = true;

    while(server_is_running) {
        tmp_fds = read_fds;

        //multiplexare intrari
        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "Error selecting inputs\n");

        //setare buffer pe 0
        memset(buffer, 0, BUFLEN);

        for(i = 0; i <= fdmax; i++) {
            if(FD_ISSET(i, &tmp_fds)) {
                //se primeste input de la tastatura
                if(i == STDIN_FILENO) {
                    //citim comanda
                    fgets(buffer, BUFLEN, stdin);

                    if(strcmp(buffer, "exit\n") == 0) {
                        server_is_running = false;
                        break;
                    } else {
                        fprintf(stderr, "Unknown command. Available commands:\nexit\n");
                    }
                } else
                //se primeste mesaj de la clientul udp
                if(i == udp_sockfd) {
                    //setare buffere pt mesajele udp si tcp pe 0
                    memset(&udp_msg, 0, BUFLEN);
                    memset(&tcp_msg, 0, TCP_MAXLEN);

                    //primire mesaj udp
                    ret = recvfrom(udp_sockfd, &udp_msg, BUFLEN + 1, 0, (sockaddr *)&cli_addr, &udp_socklen);
                    DIE(ret < 0, "Error receiving from UDP client\n");

                    //salvare ip si port ale clientului udp in mesajul tcp
                    strcpy(tcp_msg.ip, inet_ntoa(cli_addr.sin_addr));
                    tcp_msg.port = cli_addr.sin_port;

                    //creare mesaj TCP din mesajul UDP primit
                    create_tcp_message_from_udp(&udp_msg, &tcp_msg);

                    /*trimitere mesaj la clientii online si adaugare in coada de mesaje
                    pentru clientii offline cu sf=1*/
                    send_message_to_all_clients(all_clients, tcp_msg);
                } else 
                //se primeste cerere de conexiune pe socketul de ascultare TCP
                if(i == tcp_sockfd) {
                    newsockfd = accept(i, (sockaddr *) &cli_addr, &cli_socklen);
                    DIE(newsockfd < 0, "Error accepting new connection\n");

                    //dezactivare algoritmul lui Nagle
                    int optval = 1;
                    setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int));

                    //adaugare socket client nou la multimea descriptorilor pt citire
                    FD_SET(newsockfd, &read_fds);
                    if(newsockfd > fdmax) {
                        fdmax = newsockfd;
                    }

                    //citire id client nou
                    memset(buffer, 0, BUFLEN);

                    ret = recv(newsockfd, buffer, BUFLEN, 0);
                    DIE(ret < 0, "Error receiving from TCP client\n");

                    //verificam daca avem deja clientul salvat in hashmap
                    string client_id(buffer);

                    if(all_clients.find(client_id) != all_clients.end()) {
                        //daca avem deja un client cu acelasi id, inchidem conexiunea noua
                        auto &current_client = all_clients.at(client_id);
                        if(current_client.is_online) {
                            printf("Client %s already connected.\n", client_id.c_str());
                            FD_CLR(newsockfd, &read_fds);
                            close(newsockfd);
                        } else {
                            //clientul se reconecteaza, daca are mesaje in coada, i le trimitem
                            current_client.is_online = true;
                            current_client.sockfd = newsockfd;
                            printf("New client %s connected from %s:%hu.\n", client_id.c_str(),
                                  inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

                            while(!current_client.queued_messages.empty()) {
                                memset(&tcp_msg, 0, sizeof(tcp_msg));
                                tcp_msg = current_client.queued_messages.front();
                                ret = send(newsockfd, (char *)&tcp_msg, TCP_MAXLEN, 0);
                                DIE(ret < 0, "Can't send message to client\n");
                                current_client.queued_messages.pop();
                            }
                        }
                    } else {
                        //client nou nout, prima conectare
                        client new_client;
                        new_client.is_online = true;
                        new_client.sockfd = newsockfd;
                        printf("New client %s connected from %s:%hu.\n", client_id.c_str(),
                              inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

                        all_clients.insert(make_pair(client_id, new_client));
                    }
                } else {
                    //se primeste comanda de la clientii TCP
                    memset(command_buffer, 0, COMM_LEN);
                    ret = recv(i, command_buffer, COMM_LEN, 0);
                    DIE(ret < 0, "Error receiving command\n");

                    if(ret == 0) {
                        //cazul in care clientul s-a deconectat
                        auto disc_client = find_by_sock(all_clients, i);

                        string client_id(disc_client->first);
                        printf("Client %s disconnected.\n", client_id.c_str());

                        disc_client->second.is_online = false;
                        disc_client->second.sockfd = -1;

                        //inchidere socket
                        FD_CLR(i, &read_fds);
                        close(i);
                    } else {
                        //am primit comanda de la client
                        memcpy(&comm_msg, command_buffer, COMM_LEN);
                        string curr_topic(comm_msg.topic);

                        if(comm_msg.command_type == 1) {
                            //caz subscribe la topic
                            auto curr_client = find_by_sock(all_clients, i);

                            if(find_if(curr_client->second.topics.begin(),
                                      curr_client->second.topics.end(),
                                      [&curr_topic](const topic &t) {
                                          if(t.name == curr_topic)
                                            return true;
                                          return false;
                                      }) == curr_client->second.topics.end()) {
                                
                                //adaugam topicul daca acesta nu exista
                                topic new_topic;
                                new_topic.name = curr_topic;
                                new_topic.sf = (comm_msg.sf == '1') ? true : false;

                                curr_client->second.topics.push_back(new_topic);
                            }
                        } else if (comm_msg.command_type == 2) {
                            //caz unsubscribe la topic
                            auto curr_client = find_by_sock(all_clients, i);

                            auto topic_iterator = find_if(curr_client->second.topics.begin(),
                                      curr_client->second.topics.end(),
                                      [&curr_topic](const topic &t) {
                                          if(t.name == curr_topic)
                                            return true;
                                          return false;
                                      });
                            //stergem topicul daca acesta exista
                            if(topic_iterator != curr_client->second.topics.end()) {
                                curr_client->second.topics.erase(topic_iterator);
                            }
                        }
                    }
                }
            }
        }
    }

    close_sockets(fdmax, &read_fds);

    return 0;
}
