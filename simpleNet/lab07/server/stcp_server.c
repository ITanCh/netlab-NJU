#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include "stcp_server.h"
#include "../common/constants.h"

/*面向应用层的接口*/

//
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//

// stcp服务器初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量,
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
//

server_tcb_t *s_TCBTable[MAX_TRANSPORT_CONNECTIONS];
int g_son_conn;

pthread_t thread;

void stcp_server_init(int conn) {
	//init
	int i = 0;
	for(; i < MAX_TRANSPORT_CONNECTIONS;i++)
		s_TCBTable[i] = NULL;
	//set global variable
	g_son_conn = conn;
	int rc = pthread_create(&thread,NULL,seghandler,NULL);
	if(rc)
	{
		printf("ERROR: return code from pthread_create() is %d\n",rc);
		exit(-1);
	}
	return;
}

// 创建服务器套接字
//
// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.

int stcp_server_sock(unsigned int server_port) {
	int i = 0;
	for(; i < MAX_TRANSPORT_CONNECTIONS;i++)
		if(s_TCBTable[i] == NULL)
			break;
	if(i == MAX_TRANSPORT_CONNECTIONS)
		return -1;
	server_tcb_t *newTCB = (server_tcb_t*)malloc(sizeof(server_tcb_t));
	newTCB->state = CLOSED;
	newTCB->server_portNum = server_port;
	newTCB->expect_seqNum = 0;
	s_TCBTable[i] = newTCB;
	printf("port:%d new stcp server sock done\n",server_port);
	return i;
}

// 接受来自STCP客户端的连接
//
// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后进入忙等待(busy wait)直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
//

int stcp_server_accept(int sockfd) {
	server_tcb_t *accTCB = s_TCBTable[sockfd];
	if(accTCB == NULL)
		return -1;
	accTCB->state = LISTENING;
	while(accTCB->state != CONNECTED)
		usleep(ACCEPT_POLLING_INTERVAL/10000);
	printf("accept port successfully:%d\n",accTCB->server_portNum);
	return 1;
}

// 接收来自STCP客户端的数据
//
// 这个函数接收来自STCP客户端的数据. 你不需要在本实验中实现它.
//
int stcp_server_recv(int sockfd, void* buf, unsigned int length) {
	return 1;
}

// 关闭STCP服务器
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//

int stcp_server_close(int sockfd){
	server_tcb_t* t=s_TCBTable[sockfd];
	if(t==NULL||t->state!=CLOSED&&t->state!=CLOSEWAIT)return -1;
	free(t);
	s_TCBTable[sockfd] = NULL;
	return 1;
}

// 处理进入段的线程
//
// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明重叠网络连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
//

void *seghandler(void* arg) {
	int i;
	seg_t *seg = malloc(sizeof(seg_t));
	seg_t *segAck = malloc(sizeof(seg_t));
	server_tcb_t *recvTCB = NULL;
	while(1)
	{
		recvTCB = NULL;
		memset(seg,0,sizeof(seg_t));
		memset(segAck,0,sizeof(seg_t));
		i = sip_recvseg(g_son_conn,seg);
		if(i < 0)	//seg lost
		{
			printf("Server: Seg Lost\n");
			continue;
		}
		printf("Server: Receive a seg dest_port:%d\n",seg->header.dest_port);
		for(i = 0; i < MAX_TRANSPORT_CONNECTIONS;i++)
		{
			if(s_TCBTable[i] != NULL && s_TCBTable[i]->server_portNum == seg->header.dest_port) 
			{
				recvTCB = s_TCBTable[i];
				break;
			}
		}
		if(recvTCB == NULL)
		{
			printf("Server: Can't find the TCB item\n");
			continue;
		}
		switch(recvTCB->state)
		{
			case CLOSED:
				printf("Server: State CLOSED\n");
				break;
			case LISTENING:
				printf("Server: State LISTENING\n");
				if(seg->header.type == SYN)
				{
					segAck->header.type = SYNACK;	
					recvTCB->client_portNum = seg->header.src_port;
					segAck->header.dest_port = recvTCB->client_portNum;
					segAck->header.src_port = recvTCB->server_portNum;
					segAck->header.ack_num = (seg->header.seq_num + 1)%MAX_SEQNUM;
					segAck->header.seq_num = 0;
					sip_sendseg(g_son_conn,segAck);
					recvTCB->state = CONNECTED;
					recvTCB->expect_seqNum=segAck->header.ack_num;
				}
				break;
			case CLOSEWAIT:
				printf("Server: State CLOSEWAIT\n");
				if(seg->header.type == FIN&&seg->header.seq_num==seg->header.seq_num)
				{
					segAck->header.type = FINACK;	
					segAck->header.dest_port = recvTCB->client_portNum;
					segAck->header.src_port = recvTCB->server_portNum;
					segAck->header.ack_num = (seg->header.seq_num + 1)%MAX_SEQNUM;
					segAck->header.seq_num = 0;
					sip_sendseg(g_son_conn,segAck);
					recvTCB->state = CLOSEWAIT;
					recvTCB->expect_seqNum=segAck->header.ack_num;
				}
				break;
			case CONNECTED:
				if(seg->header.type == SYN&&seg->header.seq_num==seg->header.seq_num)
				{
					printf("Server: State CONNECTED -> SYN\n");
					segAck->header.type = SYNACK;	
					recvTCB->client_portNum = seg->header.src_port;
					segAck->header.dest_port = recvTCB->client_portNum;
					segAck->header.src_port = recvTCB->server_portNum;
					segAck->header.ack_num = (seg->header.seq_num + 1)%MAX_SEQNUM;
					segAck->header.seq_num = 0;
					sip_sendseg(g_son_conn,segAck);
					recvTCB->state = CONNECTED;
					recvTCB->expect_seqNum=segAck->header.ack_num;
				}
				else if(seg->header.type == FIN&&seg->header.seq_num==seg->header.seq_num)
				{
					printf("Server: State CONNECTED -> FIN\n");
					segAck->header.type = FINACK;	
					segAck->header.dest_port = recvTCB->client_portNum;
					segAck->header.src_port = recvTCB->server_portNum;
					segAck->header.ack_num = (seg->header.seq_num + 1)%MAX_SEQNUM;
					segAck->header.seq_num = 0;
					sip_sendseg(g_son_conn,segAck);
					recvTCB->state = CLOSEWAIT;
					recvTCB->expect_seqNum=segAck->header.ack_num;
				}
		}
		
	}
	return 0;
}



