/**
 * @brief: ECE358 RCS API interface dummy implementation 
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include "rcs.h"
#include "ucp.h"
#define LISTEN_BACKLOG 10

struct RCSSOC {
	int ucpfd;
	int seq;
	int isBinded;
	int isListening;
	struct sockaddr_in *src;
	struct sockaddr_in *dest;
};

struct RCSSOC *rcssoc_array[100] = {0}; 

int rcsSocket() 
{
	int ucpfd = ucpSocket();
	fprintf(stdout,"ucpfd in init is %d \n", ucpfd);
	int rcsfd = 0;
	int i;
	for(i = 0; i < sizeof(rcssoc_array) / sizeof(struct RCSSOC); i ++){
		if(rcssoc_array[i] == 0){
			
			rcsfd = i;
			struct RCSSOC *r = (struct RCSSOC*)malloc(sizeof(struct RCSSOC));
			r->ucpfd = ucpfd;
			rcssoc_array[i] = r;
			fprintf(stderr, "Current used index is %d\n", i);
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
	fprintf(stdout,"ucpfd in bind is %d \n", ucpfd);
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
	fprintf(stderr,"Socket %d is listening -- %d! \n", sockfd, rcssoc_array[sockfd]->isListening);
	return 0;
}

int rcsAccept(int sockfd, struct sockaddr_in *addr)
{
	char *buffer[10];
	char *msg = "syn-ack";
	struct sockaddr_in *sender_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	struct RCSSOC *origin = rcssoc_array[sockfd];
	int new_rcsfd ;
	int i;
	int blocked = 0;
	int v;
	struct RCSSOC *r = (struct RCSSOC*)malloc(sizeof(struct RCSSOC));

	if(sockfd > 99 || sockfd < 0) {
		fprintf(stderr,"Invalid sockfd! \n");
		return -1;
	}
	if(origin->isListening == 0){
		fprintf(stderr,"Socket %d is not listening! \n", sockfd);
		return -1;
	}

	fprintf(stderr, "Accept before block ------------  receive from %d \n", origin -> ucpfd);
	
	
	blocked = ucpRecvFrom(origin -> ucpfd, buffer, 10, sender_addr);
	
	fprintf(stderr, "Accept after block ------------ %s\n", sender_addr);

	for(i = 0; i < sizeof(rcssoc_array) / sizeof(struct RCSSOC); i ++){
		if(rcssoc_array[i] == 0){
			new_rcsfd = i;
			r -> ucpfd = ucpSocket();
			r -> src = origin -> src;
			r -> dest = sender_addr;
			rcssoc_array[i] = r;
			break;
		}
	}

	fprintf(stderr,"Socket %d is accepting! \n", new_rcsfd);
	addr = sender_addr;

	v = ucpSendTo(r-> ucpfd, msg, 10, sender_addr);
	fprintf(stderr,"Message Sent %d ! \n", v);

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
	fprintf(stderr, "Connect before block ------------  Send to upd %d ---- returned %d\n", origin -> ucpfd, v);
	
    blocked = ucpRecvFrom(origin -> ucpfd, buffer, 10, sender_addr);

	fprintf(stderr, "Connect after block ---------%s--- \n", buffer);

	return 0;
}

int rcsRecv(int sockfd, void *buf, int len)
{
	return -1;
}

int rcsSend(int sockfd, void *buf, int len)
{
	return -1;
}

int rcsClose(int sockfd)
{
	rcssoc_array[sockfd] = 0;
	return 0;
}
