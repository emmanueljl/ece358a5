/**
 * @brief: ECE358 RCS API interface dummy implementation 
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <netinet/udp.h>   //Provides declarations for udp header
#include <netinet/ip.h>    //Provides declarations for ip header
#include "rcs.h"
#include "ucp.h"
#define LISTEN_BACKLOG 10
#define SHA_DIGEST_LENGTH 40
#define UDP_DATAGRAM_SIZE 65500
#define SEQUENCE_NUMBER_SIZE 2

struct RCSSOC {
	int inUse;
	int max_buffer;
	int ucpfd;
	int seq;
	int isBinded;
	int isListening;
	struct sockaddr_in *src;
	struct sockaddr_in *dest;
	char* rcvbuffer[1280];
};

struct RCSSOC *rcssoc_array[100] = {0}; 

int rcsSocket() 
{
	int ucpfd = ucpSocket();
	// fprintf(stdout,"ucpfd in init is %d \n", ucpfd);
	int rcsfd = 0;
	int i;
	for(i = 0; i < sizeof(rcssoc_array) / sizeof(struct RCSSOC); i ++){
		if(rcssoc_array[i] == 0){
			
			rcsfd = i;
			struct RCSSOC *r = (struct RCSSOC*)malloc(sizeof(struct RCSSOC));
			r->ucpfd = ucpfd;
			r->inUse = 1;
			rcssoc_array[i] = r;
			// fprintf(stderr, "Current used index is %d\n", i);
			break;
		}
	}

	return rcsfd;
}

int rcsBind(int sockfd, struct sockaddr_in *addr) 
{
	int ucpfd;
	int status_code;
	if(sockfd > 99 || sockfd < 0) {
		fprintf(stderr,"Invalid sockfd! \n");
		return -1;
	}
	ucpfd = rcssoc_array[sockfd]->ucpfd;
	// fprintf(stdout,"ucpfd in bind is %d \n", ucpfd);
	status_code = ucpBind(ucpfd, addr);

	if(status_code == 0){
		rcssoc_array[sockfd] -> isBinded = 1;
		return 0;
	} else {
		return -1;
	}
}

int rcsGetSockName(int sockfd, struct sockaddr_in *addr) 
{
	int ucpfd;
	int status_code;
	if(sockfd > 99 || sockfd < 0) {
		fprintf(stderr,"Invalid sockfd! \n");
		return -1;
	}
	ucpfd = rcssoc_array[sockfd]->ucpfd;
	status_code = ucpGetSockName(ucpfd, addr);
	rcssoc_array[sockfd]->src = addr;
	return status_code;
}

int rcsListen(int sockfd)
{
	if(sockfd > 99 || sockfd < 0) {
		fprintf(stderr,"Invalid sockfd! \n");
		return -1;
	}
	rcssoc_array[sockfd]->isListening = 1;
	// fprintf(stderr,"Socket %d is listening -- %d! \n", sockfd, rcssoc_array[sockfd]->isListening);
	return 0;
}

char* get_checksum(int seq, void* buf){
	int i;
	int payload_size = UDP_DATAGRAM_SIZE - SHA_DIGEST_LENGTH;
    unsigned char hash[SHA_DIGEST_LENGTH];
    char *payload;
    char *checksum;
    size_t length;
    payload = malloc(payload_size);
    memset(payload, 0, payload_size);
    checksum = malloc(SHA_DIGEST_LENGTH);
    memset(checksum, 0, SHA_DIGEST_LENGTH);
    memcpy(payload, &seq, SEQUENCE_NUMBER_SIZE);
    //strncpy(payload, seq, SEQUENCE_NUMBER_SIZE);
    strncpy(&payload[2], buf, payload_size - SEQUENCE_NUMBER_SIZE);
    length = sizeof(payload);
    SHA1(payload, length, hash);

    for (i = 0; i < SHA_DIGEST_LENGTH; i++) {
        checksum[i] = hash[i];
    }
    return checksum;
}

int verify_checksum(char *buf) {
    int payload_size = UDP_DATAGRAM_SIZE - SHA_DIGEST_LENGTH;
    char *payload[payload_size];
    strncpy(payload, &buf[40], payload_size);
    unsigned char hash_from_buf[SHA_DIGEST_LENGTH];
    strncpy(hash_from_buf, buf, SHA_DIGEST_LENGTH);
    size_t length = sizeof(payload);
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(payload, length, hash);
    return strcmp(hash,hash_from_buf) == 0 ? 1 : 0;
}

const char* make_pct(int seq, void *buf, int checksum){
	// char* datagram[UDP_DATAGRAM_SIZE]; // check buf size fistr, not necessary max size
	// // How to copy ???????!?!!!!
	// datagram[0] = checksum;
	// datagram[40] = seq;
	// datagram[42] = buf;
	return "datagram";
}

int rcsAccept(int sockfd, struct sockaddr_in *addr)
{
	char *buffer;
	char *msg = "syn-ack";
	struct sockaddr_in *sender_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	struct RCSSOC *origin = rcssoc_array[sockfd];
	int new_rcsfd ;
	int i;
	int blocked = 0;
	int v;

	if(sockfd > 99 || sockfd < 0) {
		fprintf(stderr,"Invalid sockfd! \n");
		return -1;
	}
	if(origin->isListening == 0){
		fprintf(stderr,"Socket %d is not listening! \n", sockfd);
		return -1;
	}

	// fprintf(stderr, "Accept before block ------------  receive from %d \n", origin -> ucpfd);
	
	
	blocked = ucpRecvFrom(origin -> ucpfd, buffer, 10, sender_addr);
	
	// fprintf(stderr, "Accept after block ------------ %s\n", sender_addr);

	// for(i = 0; i < 3; i++){
	// 	fprintf(stderr,"Before ADD!!! Socket %d has value ------- %s! \n", i, rcssoc_array[i]);
	// }
	for(i = 0; i < sizeof(rcssoc_array) / sizeof(struct RCSSOC); i ++){
		if(rcssoc_array[i] == 0){
			struct RCSSOC *r = (struct RCSSOC*)malloc(sizeof(struct RCSSOC));
			new_rcsfd = i;
			r -> ucpfd = ucpSocket();
			r -> inUse = 1;
			r -> max_buffer = 10;
			r -> src = origin -> src;
			r -> dest = sender_addr;
			rcssoc_array[i] = r;
			break;
		}
	}
	// for(i = 0; i < 3; i++){
	// 	fprintf(stderr,"After ADD!!! Socket %d has value ------- %s! \n", i, rcssoc_array[i]);
	// }

	fprintf(stderr,"Socket %d is accepting! \n", new_rcsfd);
	// fprintf(stderr,"UDP Socket %d is accepting! \n", rcssoc_array[1]->ucpfd);
	addr = sender_addr;

	v = ucpSendTo(rcssoc_array[new_rcsfd]-> ucpfd, msg, 10, sender_addr);
	fprintf(stderr,"Message Sent %d ! \n", v);

	blocked = ucpRecvFrom(origin -> ucpfd, buffer, 10, sender_addr);

	if(buffer[0] != 'c') {
		return -1;
	}

	return new_rcsfd;
}

int rcsConnect(int sockfd, const struct sockaddr_in *addr)
{
	char *buffer[10];
	char *msg = "syn";
	struct sockaddr_in *sender_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	struct RCSSOC *origin = rcssoc_array[sockfd];
	int blocked = 0;
	int v;

	if(sockfd > 99 || sockfd < 0) {
		fprintf(stderr,"Invalid sockfd! \n");
		return -1;
	}
	if(origin -> isBinded == 0){
		fprintf(stderr,"Socket %d is not bounded! \n", sockfd);
		return -1;
	}

	v = ucpSendTo(origin -> ucpfd, msg, 10, addr);
	// fprintf(stderr, "Connect before block ------------  Send to upd %d ---- returned %d\n", origin -> ucpfd, v);
	
    blocked = ucpRecvFrom(origin -> ucpfd, buffer, 10, sender_addr);

	// fprintf(stderr, "Connect after block ---------%s--- \n", buffer);
    msg =  "ack";
	v = ucpSendTo(origin -> ucpfd, msg, 10, addr);

	return 0;
}

int rcsRecv(int sockfd, void *buf, int len)
{
	//receive from send, checksum, add to rcv buffer
	// check is buffer full or termination signal receive
	// if so return, else keep receiving
	struct RCSSOC *origin = rcssoc_array[sockfd];
	struct sockaddr_in *sender_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	char* msg;
	char* rcvbuffer;
	int ucp_socket_fd = origin -> ucpfd;
	int received_bytes = 0;
	int seq = origin -> seq;
	char* cs;
	char* send_buffer;

	received_bytes = ucpRecvFrom(ucp_socket_fd, rcvbuffer, len, sender_addr);

	if(verify_checksum(rcvbuffer)) {
		msg = "ack";
	} else {
		msg = "nack";
	}

	cs = get_checksum(seq, msg);
	send_buffer = (char*)make_pct(seq, msg, cs);

	ucpSendTo(ucp_socket_fd, send_buffer, sizeof(send_buffer), origin->src);

	buf = &rcvbuffer[42];
	return received_bytes - SHA_DIGEST_LENGTH - SEQUENCE_NUMBER_SIZE;
} 



int is_ack(void *buf, int seq) {
	char *seq_from_buffer[3];
	char *seq_from_int[3];
	char msg_from_buffer;
	memcpy( seq_from_buffer, &buf[40], 2 );
	seq_from_buffer[2] = '\0';
	sprintf(seq_from_int,"%ld", seq);

	if(strcmp(seq_from_buffer, seq_from_int) != 0){
		return 0;
	}

	memcpy( msg_from_buffer, &buff[42], 1);
	if(msg_from_buffer == 'n') {
		return 0;
	}

	return 1;
}

int rcsSend(int sockfd, void *buf, int len)
{
	char *ack_buffer;
	struct sockaddr_in *sender_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	struct RCSSOC *origin = rcssoc_array[sockfd];
	int ucp_socket_fd = origin -> ucpfd;
	char* cs = get_checksum(origin -> seq, buf);
	char* send_buffer = make_pct(origin -> seq, buf, cs);
	int status_code;

	// if(len > some max){
	// 	len = some max;
	// }

	if((status_code = ucpSendTo(ucp_socket_fd, send_buffer, len, origin->dest)) <= 0){
		fprintf(stderr,"Socket %d failed to send messgae! \n", sockfd);
		return -1;
	}

	while (1) {
		if(ucpSetSockRecvTimeout(ucp_socket_fd, 800) == EWOULDBLOCK){
			if((status_code = ucpSendTo(ucp_socket_fd, send_buffer, len, origin->dest)) <= 0){
				fprintf(stderr,"Socket %d failed to send messgae! \n", sockfd);
				return -1;
			}
		}  else if(ucpRecvFrom(origin -> ucpfd, ack_buffer, 10, sender_addr) > 0 ){
			if(verify_checksum(ack_buffer) && is_ack(ack_buffer, origin->seq)){
				break;
			}
		}
	}

	origin->seq += 1;

	return status_code - SEQUENCE_NUMBER_SIZE - SHA_DIGEST_LENGTH;
}

int rcsClose(int sockfd)
{
	char *buffer = "\0";
	fprintf(stderr, "Closing RCS socket %d \n ", sockfd);
	// rcsSend(sockfd, buffer, sizeof(char *buffer));
	rcssoc_array[sockfd] = 0;

	return 0;
}
