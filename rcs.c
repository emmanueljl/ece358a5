/**
 * @brief: ECE358 RCS API interface dummy implementation
 *
 */
#include <stdint.h>
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
#define CHECKSUM_LENGTH 4
#define UDP_DATAGRAM_SIZE 500
#define SEQUENCE_NUMBER_SIZE 2

extern int errno;

struct RCSSOC {
	int inUse;
	int max_buffer;
	int ucpfd;
	int seq;
	int isBinded;
	int isListening;
	struct sockaddr_in *src;
	struct sockaddr_in *dest;
	char* rcvbuffer[500];
};

struct RCSSOC *rcssoc_array[100] = {0};

/*
Checksum functions:
1. uint16_t get_checksum(char *buf, int seq) returns the checksum based on Fletcher16
2. int verify_checksum(char *payload) returns the result of verification
*/

uint16_t get_checksum(char* buf, int seq) {
	// Follows the standard Fletcher16
	uint16_t sum1;
	uint16_t sum2;
	uint16_t hash;
	uint16_t ret;
	int index = 0;
	hash = 255;
	sum1 = seq % 255;
	sum2 = seq % 255;
	while(buf[index] != '\0') {
		sum1 = (sum1 + (buf[index] - '0')) % 255;
		sum2 = (sum2 + sum1)  % 255;
		index += 1;
	}
	ret = sum2 << 8;
	ret = ret | sum1;
	return ret;
}

int verify_checksum(char* payload) {
	// TODO: Test.
	char* index;
	uint16_t old_checksum;
	uint16_t new_checksum;
	int seq;
	index = payload;
	old_checksum = ((uint16_t *)index)[0];
	index += 4;
	seq = ((uint16_t *)index)[0];
	index += 2;
	new_checksum = get_checksum(index, seq);
	if (new_checksum == old_checksum) return 1;
	else return 0;
}

char* make_pct(int seq, char* buf, uint16_t checksum) {
	char* payload;
	char* index;
	payload = malloc(UDP_DATAGRAM_SIZE);
	memset(payload, 0, UDP_DATAGRAM_SIZE);
	index = payload;
	index[0] = (checksum>>0) & 0xff;
	index[1] = (checksum>>8) & 0xff;
	index[2] = (checksum>>16) & 0xff;
	index[3] = (checksum>>24) & 0xff;
	index[4] = (seq>>0) & 0xff;
	index[5] = (seq>>8) & 0xff;
	index += 6;
	strncpy(index, buf, UDP_DATAGRAM_SIZE - 6);
	return payload;
}
/*
RCS functions:
1. int rcsSocket() returns a socket descriptor or -1 as error.
2. int rcsBind(int, struct sockaddr_in*) returns 0 on success.
*/
int rcsSocket() {
	// TODO: errno should be set here.
	// TODO: Test.
	int ucpfd;
	int rcsfd;
	int i;
	struct RCSSOC* r;
	ucpfd = ucpSocket();
	fprintf(stderr, "UCPFD i is %d\n", ucpfd);
	if( ucpfd == -1) {
		errno = EBADF; /* Bad file descriptor */
		return -1;
	}
	// fprintf(stdout,"ucpfd in init is %d \n", ucpfd);
	rcsfd = 0;
	i = 0;
	while ((i<100) && (rcssoc_array[i]!=0)) {i++;}
	if (i==100) {
		errno = ENOBUFS; /* No buffer space available */
		return -1;
	}
	fprintf(stderr, "Current i is %d\n", i);
	rcsfd = i;
	r = (struct RCSSOC*)malloc(sizeof(struct RCSSOC));
	r->ucpfd = ucpfd;
	r->inUse = 1;
	rcssoc_array[i] = r;

	fprintf(stderr, "Newly added socket is %d with ucp socket id %d\n", rcsfd, ucpfd);
	return rcsfd;
	/* Old code. Have some coding issus. Rewritten.
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
	*/
}

int rcsBind(int sockfd, struct sockaddr_in *addr) {
	// TODO: set errno
	// TODO: Test.
	int ucpfd;
	int status_code;
	if(sockfd > 99 || sockfd < 0) {
		fprintf(stderr,"Invalid sockfd! \n");
		errno = EINVAL; /* Invalid argument */
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

int rcsGetSockName(int sockfd, struct sockaddr_in *addr) {
	// TODO: set errno
	// TODO: Test.
	int ucpfd;
	int status_code;
	if(sockfd > 99 || sockfd < 0) {
		fprintf(stderr,"Invalid sockfd! \n");
		errno = EINVAL; /* Invalid argument */
		return -1;
	}
	ucpfd = rcssoc_array[sockfd]->ucpfd;
	status_code = ucpGetSockName(ucpfd, addr);
	if (status_code == -1) return -1;
	rcssoc_array[sockfd]->src = addr;
	return status_code;
}

int rcsListen(int sockfd) {
	// TODO: set errno
	if(sockfd > 99 || sockfd < 0) {
		fprintf(stderr,"Invalid sockfd! \n");
		errno = EINVAL; /* Invalid argument */
		return -1;
	}
	rcssoc_array[sockfd]->isListening = 1;
	// fprintf(stderr,"Socket %d is listening -- %d! \n", sockfd, rcssoc_array[sockfd]->isListening);
	return 0;
}

// From this point, the code has not been checked.
int rcsAccept(int sockfd, struct sockaddr_in *addr) {
	char buffer[4] = {0};
	char *msg = "syn-ack\0";
	struct sockaddr_in *sender_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	struct RCSSOC *origin = rcssoc_array[sockfd];
	int new_rcsfd ;
	int i;
	int blocked = 0;
	int v;


	if(sockfd > 99 || sockfd < 0) {
		fprintf(stderr,"Invalid sockfd! \n");
		errno = EINVAL; /* Invalid argument */
		return -1;
	}
	if(origin->isListening == 0){
		fprintf(stderr,"Socket %d is not listening! \n", sockfd);
		errno = EOPNOTSUPP; /* Operation not supported on socket */
		return -1;
	}

	fprintf(stderr, "Accept before block ------------  receive from %d \n", origin -> ucpfd);


	blocked = ucpRecvFrom(origin -> ucpfd, buffer, 4, sender_addr);
	fprintf(stderr, "First message from client is %s\n", buffer);

	// for(i = 0; i < 3; i++){
	// 	fprintf(stderr,"Before ADD!!! Socket %d has value ------- %s! \n", i, rcssoc_array[i]);
	// }
	for(i = 0; i < 100; i ++){
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

	v = ucpSendTo(rcssoc_array[new_rcsfd]-> ucpfd, msg, 8, sender_addr);
	fprintf(stderr,"Message Sent %d ! \n", v);

	memset(buffer, 0, 4);
	blocked = ucpRecvFrom(origin -> ucpfd, buffer, 4, sender_addr);
	fprintf(stderr, "Second message from client is %s\n", buffer);


	if(strcmp(buffer, "ack")) {
		return -1;
	}

	return new_rcsfd;
}

int rcsConnect(int sockfd, const struct sockaddr_in *addr)
{
	char buffer[8] = {0};
	char *msg = "syn\0";
	struct sockaddr_in *sender_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	struct RCSSOC *origin = rcssoc_array[sockfd];
	int blocked = 0;
	int v;

	if(sockfd > 99 || sockfd < 0) {
		fprintf(stderr,"Invalid sockfd! \n");
		errno = EINVAL; /* Invalid argument */
		return -1;
	}
	if(origin -> isBinded == 0){
		fprintf(stderr,"Socket %d is not bounded! \n", sockfd);
		return -1;
	}

	v = ucpSendTo(origin -> ucpfd, msg, 4, addr);
	fprintf(stderr, "Connect before block ------------  Send to upd %d ---- returned %d\n", origin -> ucpfd, v);

    blocked = ucpRecvFrom(origin -> ucpfd, buffer, 8, sender_addr);

    fprintf(stderr, "Meesage from server is %s\n", buffer);
	fprintf(stderr, "Connect after block ---------%s--- \n", buffer);

    msg = "ack\0";
	v = ucpSendTo(origin -> ucpfd, msg, 10, addr);
	origin->dest = sender_addr;
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
	uint16_t cs;
	char* send_buffer;

	received_bytes = ucpRecvFrom(ucp_socket_fd, rcvbuffer, len, sender_addr);
	printf("received bytes:%d\n", received_bytes);

	if(verify_checksum(rcvbuffer)) {
		msg = "ack";
	} else {
		msg = "nack";
	}

	cs = get_checksum(msg, seq);
	send_buffer = (char*)make_pct(seq, msg, cs);

	ucpSendTo(ucp_socket_fd, send_buffer, sizeof(send_buffer), origin->src);

	buf = &(rcvbuffer[6]);
	//printf("%d\n", received_bytes - CHECKSUM_LENGTH - SEQUENCE_NUMBER_SIZE);
	//return received_bytes - CHECKSUM_LENGTH - SEQUENCE_NUMBER_SIZE;
	return received_bytes;
}

int is_ack(void *buf, int seq) {
	int seq_from_buffer;
	char msg_from_buffer[1];
	char* index;
	index = buf+4;
	seq_from_buffer = ((int*)index)[0];

	if(seq == seq_from_buffer) {
		return 0;
	}

	index += 2;

	memcpy( msg_from_buffer, index, 1);

	if(msg_from_buffer[0] == 'n') {
		return 0;
	}

	return 1;
}

int rcsSend(int sockfd, void *buf, int len) {
	char *ack_buffer;
	struct sockaddr_in *sender_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	struct RCSSOC *origin = rcssoc_array[sockfd];
	int ucp_socket_fd = origin -> ucpfd;
	uint16_t cs = get_checksum(buf, origin -> seq);
	char* send_buffer = make_pct(origin->seq, buf, cs);
	int status_code;
	// if(len > some max){
	// 	len = some max;
	// }
	if((status_code = ucpSendTo(ucp_socket_fd, send_buffer, len, origin->dest)) <= 0){
		fprintf(stderr,"Socket %d failed to send messgae! \n", sockfd);
		return -1;
	}

	while (1) {
		if(ucpSetSockRecvTimeout(ucp_socket_fd, 800) == EWOULDBLOCK) {
			if((status_code = ucpSendTo(ucp_socket_fd, send_buffer, len, origin->dest)) <= 0){
				fprintf(stderr,"Socket %d failed to send messgae! \n", sockfd);
				return -1;
			}
			printf("%d\n", status_code);
		}  else if(ucpRecvFrom(origin -> ucpfd, ack_buffer, 10, sender_addr) > 0 ){
			if(verify_checksum(ack_buffer) && is_ack(ack_buffer, origin->seq)){
				break;
			}
		}
	}

	origin->seq += 1;

	return status_code - SEQUENCE_NUMBER_SIZE - CHECKSUM_LENGTH;
}

int rcsClose(int sockfd)
{
	char *buffer = "\0";
	fprintf(stderr, "Closing RCS socket %d \n ", sockfd);
	// rcsSend(sockfd, buffer, sizeof(char *buffer));
	rcssoc_array[sockfd] = 0;

	return 0;
}

/* Old implementations. Delete later.
char* get_checksum(int seq, char* buf) {
	char *payload; //This is a local payload area, used to calculate checksome
	char *checksum; //SHA1 value of SEQ+DATA
	int length; //The size of SEQ+DATA
    unsigned char hash[SHA_DIGEST_LENGTH]; //char array holding the SHA1 value
	char *index;
	int i;
	int payload_size = UDP_DATAGRAM_SIZE - SHA_DIGEST_LENGTH;
	i = 0;
	checksum = malloc(SHA_DIGEST_LENGTH+1);
	memset(checksum, 0, SHA_DIGEST_LENGTH+1);
    payload = malloc(payload_size);
    memset(payload, 0, payload_size);
	index = payload;
    memcpy(payload, &seq, SEQUENCE_NUMBER_SIZE);
	index += 2;
    strncpy(index, buf, payload_size -SEQUENCE_NUMBER_SIZE-1);
    length = 2+strlen(index);
    SHA1(payload, length, hash);
    for (i = 0; i < SHA_DIGEST_LENGTH; i++) {
        checksum[i] = hash[i];
    }
	free(payload);
    return &(checksum[0]);
}

int verify_checksum(char *buf) {

    int payload_size = UDP_DATAGRAM_SIZE - SHA_DIGEST_LENGTH;
    char *payload;
    strncpy(payload, &(buf[40]), payload_size);
    unsigned char hash_from_buf[SHA_DIGEST_LENGTH];
    strncpy(hash_from_buf, buf, SHA_DIGEST_LENGTH);
    size_t length = sizeof(payload);
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(payload, length, hash);
    return strcmp(hash,hash_from_buf) == 0 ? 1 : 0;
}
*/
