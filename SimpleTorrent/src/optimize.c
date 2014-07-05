#include "btdata.h"
#include "util.h"



/*
 * This optimize.c file is reference, no actual realization
 *
 *
 * The addition of these optimization algorithms can bring some slight 
 * modification to the original project, mainly about the data structure,
 * so here showing is partial thought about the realization. 
 * When basic functions are finished, then I will change these fake code
 * to real ones.
 */

//end game 算法:
//
//begin from p2p.c function sendRequest()
//when detecting the request for the last piece
//	send the request to every peers
//
//waiting for the response...
//
//when getting the last-piece reply
//	cancel the last-piece request to other peers
//
//伪代码：

typedef struct _peer_list{
	peer_t *peer_node;
	peer_t *next;
} peer_list;

//将所有的peer存在一个链表中
peer_list g_all_peers;

void end_game(char *sendBuf){

	char recvline[MAXLINE];
	//send last-piece request to every peer
	peer_list *p = g_all_peers;
	while(p != NULL){
		send(p->peer_node->sockfd,sendBuf,17,0);
		p = p->next;
	}
	//waiting for the response
	//turn to function PToP()

	//cancel the request to other peers
	
	head->len = 13;
	head->id = 0x08;
	send(sockfd,sendline,sizeof(Head),0);
	g_done = 1;
	
	// status cancel : g_done = 1;
}

//resume from partial downloading 算法:
//
//调用client_shutdown函数时，认为断点出现
//everytime calling savePiece()
//	update a global variable g_complete_offset
//
//when start again, we can use the previous value of the g_complete_offset
//	to realize "resume from partial downloading"
//伪代码：
void _client_shutdown(int sig){
	
}
