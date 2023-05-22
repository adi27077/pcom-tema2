#include "helpers.hpp"

void usage(char *file) {
	fprintf(stderr, "Usage: %s client_id server_address server_port\n", file);
	exit(0);
}

void id_too_long(char *id) {
    fprintf(stderr, "ID %s is too long. 10 characters max\n", id);
    exit(0);
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int sockfd, ret;
    struct sockaddr_in serv_addr;
    char buffer_comm[COMM_LEN], buffer_tcp[TCP_MAXLEN];
    fd_set read_fds, tmp_fds;

    if(argc < 4) {
        usage(argv[0]);
    }

    if(strlen(argv[1]) > 10) {
        id_too_long(argv[1]);
    }

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    tcp_message tcp_msg;
    command_message comm_msg;

    //initializare socket pt conexiunea la server 
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "TCP socket initialization error\n");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    ret = inet_aton(argv[2], &serv_addr.sin_addr);
    DIE(ret == 0, "Error setting server IP address\n");

    //conectare la server
    ret = connect(sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "Error connecting to server\n");

    //dezactivare algoritmul lui Nagle
    int optval = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int));

    //trimitere id catre server
    ret = send(sockfd, argv[1], strlen(argv[1]) + 1, 0);
    DIE(ret < 0, "Error sending ID to server\n");

    //adaugare socket si stdin in multimea descriptorilor pt citire
    FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);

    while(1) {
        tmp_fds = read_fds;

        //multiplexare intrari
        ret = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "Error selecting inputs\n");

        //cazul in care se citeste de la tastatura
        if(FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            memset(buffer_comm, 0, BUFLEN);
            fgets(buffer_comm, BUFLEN - 1, stdin);

            //comanda exit
            if(strncmp(buffer_comm, "exit", 4) == 0) {
                break;
            }

            //comanda subscribe/unsubscribe si creare structura mesaj comanda
            buffer_comm[strlen(buffer_comm) - 1] = '\0';
            char *p = strtok(buffer_comm, " ");

            if(strncmp(p, "subscribe", 9) == 0) {
                comm_msg.command_type = 1;
            } else if(strncmp(p, "unsubscribe", 11) == 0) {
                comm_msg.command_type = 2;
            } else {
                fprintf(stderr, "Unknown command. Available commands\n\
                        subscribe <topic> <sf>\nunsubscribe <topic>\nexit\n");
            }

            p = strtok(NULL, " ");
            strcpy(comm_msg.topic, p);

            if(comm_msg.command_type == 1) {
                p = strtok(NULL, " ");
                comm_msg.sf = p[0];
            }

            //trimitere comanda la server
            ret = send(sockfd, (char *)&comm_msg, COMM_LEN, 0);
            DIE(ret < 0, "Error sending command to server\n");

            //afisare efect comanda dupa ce aceasta a fost trimisa la server
            if(comm_msg.command_type == 1) {
                printf("Subscribed to topic.\n");
            } else {
                printf("Unsubscribed from topic.\n");
            }
        }

        //cazul in care se primeste mesaj TCP de la server
        if(FD_ISSET(sockfd, &tmp_fds)) {
            memset(buffer_tcp, 0, TCP_MAXLEN);
            ret = recv(sockfd, buffer_comm, TCP_MAXLEN, 0);
            DIE(ret < 0, "Error receiving message from server\n");
            if(ret == 0)
                break; //serverul a inchis conexiunea

            memcpy(&tcp_msg, buffer_comm, TCP_MAXLEN);
            printf("%s:%hu - %s - %s - %s\n", tcp_msg.ip, tcp_msg.port, tcp_msg.topic,
                   tcp_msg.data_type, tcp_msg.payload);
        }
    }

    close(sockfd);

    return 0;
}
