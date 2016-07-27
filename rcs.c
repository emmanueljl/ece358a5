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
	while((buf[index] != '\n') && (buf[index] != '\0')) {
		sum1 = (sum1 + (buf[index] - '0')) % 255;
		sum2 = (sum2 + sum1)  % 255;
		index += 1;
	}
	ret = sum2 << 8;
	ret = ret | sum1;
	return ret;
}

int verify_checksum(char* payload) {
	char* index = payload;
	int old_cs = *((int*)index);
	index += 4;
	int seq = *((int*)index);
	index += 4;
	int i = 0;
	while(index[i]!='\n') {
		i += 1;
	}
	char* msg;
	msg = malloc(i+1);
	msg[i] = '\0';
	while(i>=0) {
		msg[i] = index[i];
		i -= 1;
	}
	int new_cs = get_checksum(msg, seq);
	free(msg);
	if (new_cs == old_cs) {
		printf("Yes, it passed!\n");
		return 1;
	}
	else return 0;
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
	// TODO: Test.
	int ucpfd;
	int rcsfd;
	int i;
	struct RCSSOC* r;
	ucpfd = ucpSocket();
	
	if( ucpfd == -1) {
		errno = EBADF; /* Bad file descriptor */
		return -1;
	}
	
	rcsfd = 0;
	i = 0;
	while ((i<100) && (rcssoc_array[i]!=0)) {i++;}
	if (i==100) {
		errno = ENOBUFS; /* No buffer space available */
		return -1;
	}
	rcsfd = i;
	r = (struct RCSSOC*)malloc(sizeof(struct RCSSOC));
	r->ucpfd = ucpfd;
	r->inUse = 1;
	rcssoc_array[i] = r;

	fprintf(stderr, "Newly added socket is %d with ucp socket id %d\n", rcsfd, ucpfd);
	return rcsfd;
}

int rcsBind(int sockfd, struct sockaddr_in *addr) {
	// TODO: Test.
	int ucpfd;
	int status_code;
	if(sockfd > 99 || sockfd < 0) {
		fprintf(stderr,"Invalid sockfd! \n");
		errno = EINVAL; /* Invalid argument */
		return -1;
	}
	ucpfd = rcssoc_array[sockfd]->ucpfd;
	
	status_code = ucpBind(ucpfd, addr);

	if(status_code == 0){
		rcssoc_array[sockfd] -> isBinded = 1;
		return 0;
	} else {
		return -1; // errno already set in ucpBind -> mybind
	}
}

int rcsGetSockName(int sockfd, struct sockaddr_in *addr) {
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
	if (status_code == -1) return -1; // errno already set in ucpGetSockName -> getsockname
	rcssoc_array[sockfd]->src = &addr_copy;
	return status_code;
}

int rcsListen(int sockfd) {
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

	reclen = ucpRecvFrom(origin->ucpfd, buffer, 5, sender_addr);
	if ((reclen < 0) ||( strcmp(buffer, "icon") != 0)) {
		fprintf(stderr,"The first msg from client is wrong!");
		errno = EBADMSG; /* Bad message */
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
	fprintf(stderr, "Ackknowledge message from client is %s\n", buffer);

	if(strcmp(buffer, "ack")) {
		errno = EBADMSG; /* Bad message */
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
		errno = ENOTCONN; /* The socket is not connected */
		return -1;
	}

	// Prepare for the first contact
	msg = "icon";
	v = ucpSendTo(origin->ucpfd, msg, 5, addr);
	if (v<0) {
		// TODO: retry
		return -1; // errno already set in ucpSendTo
	}

	// wait for ack from server
	buffer = malloc(sizeof(struct sockaddr_in));
	sender_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	blocked = ucpRecvFrom(origin->ucpfd, buffer, sizeof(struct sockaddr_in), sender_addr);
	if (blocked < 0) {
		// TODO: retry
		return -1; // errno already set in ucpRecvFrom -> recvfrom
	}

	// send ack back to server using old addr
	msg2 = "ack\0";
	v = ucpSendTo(origin->ucpfd, msg2, 4, addr);
    if (v<0) {
		// TODO: retry
		fprintf(stderr, "Message sent %d\n", v);
		return -1; // errno already set in ucpSendTo
	}

	origin->dest = buffer;

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
	int cnt = 0;
	sender_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	ack_buffer = malloc(5);
	origin = rcssoc_array[sockfd];
	ucp_socket_fd=origin->ucpfd;
	cs = get_checksum(buf, origin -> seq);
	send_buffer = make_pct(origin->seq, buf, cs);

	while(1) {
		printf("CNT = %d\n", cnt);
		fprintf(stderr, "Sending on ucp %d\n", ucp_socket_fd);
		if((status_code = ucpSendTo(ucp_socket_fd, send_buffer, len+8, origin->dest)) <= 0){
			fprintf(stderr,"Socket %d failed to send messgae! \n", sockfd);
			return -1; // errno already set in ucpSendTo
		}
		cnt ++;
		ucpSetSockRecvTimeout(ucp_socket_fd, 512);
		
		if(errno == EWOULDBLOCK || ucpRecvFrom(origin -> ucpfd, ack_buffer, 12, sender_addr) < 0) {
			printf("Error number matches: %d\n", errno == EWOULDBLOCK );
			errno = 0;
			continue;
		} else {
			printf("the received ack is=%s\n", ack_buffer+8);
			if(is_ack(ack_buffer, origin->seq)){
				break;
			} 
		}
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
	char msg[4];
	char* send_buffer;
	int cs;
	int seq;
	int v;

	origin = rcssoc_array[sockfd];
	sender_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	rcvbuffer = malloc(len);
	ucp_socket_fd = origin->ucpfd;
	received_bytes = ucpRecvFrom(ucp_socket_fd, rcvbuffer, len, sender_addr);
	printf("Received bytes on rcsRecv is %d\n", received_bytes);
	printf("Received message is %s", rcvbuffer+8);
	printf("end\n");
	if(verify_checksum(rcvbuffer)) {
		msg[0] = 'a';
		msg[1] = 'c';
		msg[2] = 'k';
		msg[3] = '\n';
	} else {
		msg[0] = 'n';
		msg[1] = 'a';
		msg[2] = 'c';
		msg[3] = '\n';
 	}
	seq = origin->seq;
	cs = get_checksum(msg, seq);
	printf("The correct ack cs is=%d\n", cs);
	printf("The message is %s\n", msg);
	send_buffer = make_pct(seq, msg, cs);
	printf("The send_buffer messgae is %s\n",  send_buffer+8);
	v = ucpSendTo(ucp_socket_fd, send_buffer, 12, origin->dest);
	printf("The v is %d\n", v);
	memcpy(buf, rcvbuffer+8, received_bytes-8);
	//buf = rcvbuffer+8;
	free(send_buffer);
	//printf("%d\n", received_bytes - CHECKSUM_LENGTH - SEQUENCE_NUMBER_SIZE);
	//return received_bytes - CHECKSUM_LENGTH - SEQUENCE_NUMBER_SIZE;
	origin->seq += 1;
	return received_bytes - 8;
}

int is_ack(void *buf, int seq) {
	int cs = 0;
	int seq_from_buffer;
	printf("msg is = %s", buf+8);
	printf("end!\n");
	cs = verify_checksum(buf);
	if (cs != 1) {
		printf("Ack checksum failed!\n");
		return 0;
	}
	char* index=buf;
	index += 4;
	seq_from_buffer = *((int*)index);
	index += 4;
	if(seq != seq_from_buffer) {
		printf("Ack match sequence number failed!\n");
		return 0;
	}

	if( index[0] == 'n') {
		printf("NAck!\n");
		return 0;
	}
	return 1;
}

int rcsClose(int sockfd)
{
	char *buffer = "\0";
	fprintf(stderr, "Closing RCS socket %d \n ", sockfd);
	// rcsSend(sockfd, buffer, sizeof(char *buffer));
	rcssoc_array[sockfd] = 0;

	return 0;
}

