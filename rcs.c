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
#define CHECKSUM_LENGTH 4
#define UDP_DATAGRAM_SIZE 247
#define SEQUENCE_NUMBER_SIZE 4

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

int get_checksum(char* buf, int seq) {
	// Follows the standard Fletcher16
	int sum1;
	int sum2;
	int ret;
	int index = 0;
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
	/*
	char* index;
	int old_checksum;
	int new_checksum;
	int seq;
	int i;
	char* buf;
	index = payload;
	old_checksum = *((int*)index);
	index += 4;
	seq = *((int*)index);
	index += 4;
	i = 0;
	while(index[i] != '\0') i++;
	buf = malloc(i+2);
	strcpy(buf, index);
	buf[i+1] = '\0';
	new_checksum = get_checksum(buf, seq);
	free(buf);
	if (new_checksum == old_checksum) return 1;
	else return 0;*/
	return 1;
}

char* make_pct(int seq, char* buf, int checksum) {
	// TODO: malloc size issue.
	char* payload;
	char* index;
	int length;
	int i = 0;
	int j = 8;
	length = strlen(buf);
	if (length <= UDP_DATAGRAM_SIZE) {
		payload = malloc(length+8);
		memset(payload, 0, length+8);
	} else {
		payload = malloc(UDP_DATAGRAM_SIZE);
		memset(payload, 0, UDP_DATAGRAM_SIZE);
	}
	index = payload;
	index[0] = (checksum>>0) & 0xff;
	index[1] = (checksum>>8) & 0xff;
	index[2] = (checksum>>16) & 0xff;
	index[3] = (checksum>>24) & 0xff;
	index[4] = (seq>>0) & 0xff;
	index[5] = (seq>>8) & 0xff;
	index[6] = (seq>>16) & 0xff;
	index[7] = (seq>>24) & 0xff;
	while(buf[i] != '\0') {
		index[j] = buf[i];
		i++;
		j++;
	}
	index[j+1] = '\0';
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
	struct sockaddr_in addr_copy;
	
	if(sockfd > 99 || sockfd < 0) {
		fprintf(stderr,"Invalid sockfd! \n");
		errno = EINVAL; /* Invalid argument */
		return -1;
	}
	ucpfd = rcssoc_array[sockfd]->ucpfd;
	addr_copy = *(addr);
	status_code = ucpGetSockName(ucpfd, &addr_copy);
	if (status_code == -1) return -1;
	rcssoc_array[sockfd]->src = &addr_copy;
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

int rcsAccept(int sockfd, struct sockaddr_in *addr) {
	// TODO: errno
	int reclen;
	struct sockaddr_in* sender_addr;
	char buffer[5];
	int new_rcsfd;
	struct sockaddr_in new_addr;
	int index;
	char *msg;

	sender_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	memset(buffer, 0, 5);
	msg = malloc(sizeof(struct sockaddr_in));

	struct RCSSOC *origin = rcssoc_array[sockfd];
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

	reclen = ucpRecvFrom(origin->ucpfd, buffer, 5, sender_addr);
	if ((reclen < 0) ||( strcmp(buffer, "icon") != 0)) {
		fprintf(stderr,"The first msg from client is wrong!");
		return -1;
	}
	new_rcsfd = rcsSocket();
	memset(&new_addr, 0, sizeof(struct sockaddr_in));
    new_addr.sin_family = AF_INET;
    new_addr.sin_port = 0;
    if((new_addr.sin_addr.s_addr = getPublicIPAddr()) == 0) {
        fprintf(stderr, "Could not get public ip address. Exiting...\n");
        exit(0);
    }

	if(rcsBind(new_rcsfd, &new_addr) < 0) {
        fprintf(stderr, "rcsBind() failed. Exiting...\n");
        exit(0);
	}


	if(rcsGetSockName(new_rcsfd, &new_addr) < 0) {
        fprintf(stderr, "rcsGetSockName() failed. Exiting...\n");
        exit(0);
    }

    if(rcsListen(new_rcsfd) < 0) {
        perror("listen"); exit(0);
    }

	rcssoc_array[new_rcsfd]->dest = sender_addr;
	fprintf(stderr,"Socket %d is accepting! \n", new_rcsfd);

	// The server return new addr to the client.
	addr = sender_addr;
	memcpy(msg, &new_addr, sizeof(struct sockaddr_in));
	v = ucpSendTo(rcssoc_array[new_rcsfd]->ucpfd, msg, sizeof(struct sockaddr_in), sender_addr);
	if (v == -1) {
		// TODO: Timeout support
		fprintf(stderr, "server ack failed!\n");
		exit(0);
	}
	fprintf(stderr,"Message Sent %d !\n", v);

	// Wait for client ACK.
	memset(buffer, 0, 5);
	blocked = ucpRecvFrom(origin -> ucpfd, buffer, 5, sender_addr);
	fprintf(stderr, "Second message from client is %s\n", buffer);

	if(strcmp(buffer, "ack")) {
		return -1;
	}

	return new_rcsfd;
}

int rcsConnect(int sockfd, const struct sockaddr_in *addr) {
	struct RCSSOC *origin;
	char *msg;
	char *msg2;
	int v;
	int blocked;
	struct sockaddr_in* buffer;
	struct sockaddr_in* sender_addr;

	origin = rcssoc_array[sockfd];

	if(sockfd > 99 || sockfd < 0) {
		fprintf(stderr,"Invalid sockfd! \n");
		errno = EINVAL; /* Invalid argument */
		return -1;
	}

	if(origin->isBinded == 0){
		fprintf(stderr,"Socket %d is not bounded! \n", sockfd);
		return -1;
	}

	// Prepare for the first contact
	msg = "icon";
	v = ucpSendTo(origin->ucpfd, msg, 5, addr);
	if (v<0) {
		// TODO: retry
		// TODO: errno
		return -1;
	}
	fprintf(stderr, "Send from upd %d\n", origin->ucpfd);
	
	// wait for ack from server
	buffer = malloc(sizeof(struct sockaddr_in));
	sender_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	blocked = ucpRecvFrom(origin->ucpfd, buffer, sizeof(struct sockaddr_in), sender_addr);
	if (blocked < 0) {
		// TODO: retry
		// TODO: errno
		return -1;
	}

	// send ack back to server using old addr
	msg2 = "ack\0"; 
	v = ucpSendTo(origin->ucpfd, msg2, 4, addr);
    if (v<0) {
		// TODO: retry
		// TODO: errno
		fprintf(stderr, "Message sent %d\n", v);
		return -1;
	}

	origin->dest = buffer;

    //fprintf(stderr, "Meesage from server is %s\n", buffer);
	//fprintf(stderr, "Connect after block ---------%s--- \n", buffer);
	//origin->ucpfd = received_sockfd;
	//rcsBind(sockfd, origin->src);
	//rcsGetSockName(received_sockfd, origin->src);
	//rcsListen(received_sockfd);	
	return 0;
}

int rcsSend(int sockfd, void *buf, int len) {
	char* ack_buffer;
	struct sockaddr_in* sender_addr;
	struct RCSSOC* origin;
	int ucp_socket_fd;
	int cs;
	char* send_buffer;
	int status_code;
	int v;
	sender_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	ack_buffer = malloc(5);
	origin = rcssoc_array[sockfd];
	ucp_socket_fd=origin->ucpfd;
	cs = get_checksum(buf, origin -> seq);
	send_buffer = make_pct(origin->seq, buf, cs);

	while(1) {
		fprintf(stderr, "Sending on ucp %d\n", ucp_socket_fd);
		if((status_code = ucpSendTo(ucp_socket_fd, send_buffer, len+8, origin->dest)) <= 0){
			fprintf(stderr,"Socket %d failed to send messgae! \n", sockfd);
			return -1;
		}
		
		v = ucpRecvFrom(origin->ucpfd, ack_buffer, 5, sender_addr);
		printf("%d", v);
		if (v > 0) break;
		/*	
		if(ucpSetSockRecvTimeout(ucp_socket_fd, 800) == EWOULDBLOCK ||
			ucpRecvFrom(origin -> ucpfd, ack_buffer, 4, sender_addr) < 0) {
			continue;
		} else {
			printf("hello\n");
			if(verify_checksum(ack_buffer) && is_ack(ack_buffer, origin->seq)){
				break;
			}
		}*/
	}
	free(ack_buffer);
	memset(buf, 0, len);
	origin->seq += 1;
	printf("%d\n",status_code);
	return status_code - SEQUENCE_NUMBER_SIZE - CHECKSUM_LENGTH;
}

int rcsRecv(int sockfd, void *buf, int len) {
	// ireceive from send, checksum, add to rcv buffer
	// check is buffer full or termination signal receive
	// if so return, else keep receiving
	struct RCSSOC* origin;
	struct sockaddr_in *sender_addr;
	char* rcvbuffer;
	int ucp_socket_fd;
	int received_bytes;
	char* msg;
	char* send_buffer;
	int cs;
	int seq;
	int v;

	origin = rcssoc_array[sockfd];
	sender_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	rcvbuffer = malloc(len);
	ucp_socket_fd = origin->ucpfd;
	received_bytes = ucpRecvFrom(ucp_socket_fd, rcvbuffer, len, sender_addr);
	printf("%d\n", received_bytes);

	if(verify_checksum(rcvbuffer)) {
		msg = "ack";
	} else {
		msg = "nack";
	}
	
	seq = origin->seq;
	origin->seq += 1;
	cs = get_checksum(msg, seq);
	send_buffer = make_pct(seq, msg, cs);
	v = ucpSendTo(ucp_socket_fd, send_buffer, sizeof(send_buffer), origin->dest);
	buf = &(rcvbuffer[8]);
	free(send_buffer);
	//printf("%d\n", received_bytes - CHECKSUM_LENGTH - SEQUENCE_NUMBER_SIZE);
	//return received_bytes - CHECKSUM_LENGTH - SEQUENCE_NUMBER_SIZE;
	return received_bytes;
}

int is_ack(void *buf, int seq) {
	/*
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

	return 1;*/
	return 1;
}


/*
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
	//uint16_t oldcs;
	//char* index = send_buffer;
	//index += 2;
	//printf("seq=%d\n", *((int*)index));
	//oldcs = *((uint16_t*)send_buffer);
	//if (oldcs == cs) printf("hahahaha\n");
	if((status_code = ucpSendTo(ucp_socket_fd, send_buffer, len+6, origin->dest)) <= 0){
		fprintf(stderr,"Socket %d failed to send messgae! \n", sockfd);
		return -1;
	}

	while (1) {
		if(ucpSetSockRecvTimeout(ucp_socket_fd, 800) == EWOULDBLOCK) {
			if((status_code = ucpSendTo(ucp_socket_fd, send_buffer, len+6, origin->dest)) <= 0){
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
}*/

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
