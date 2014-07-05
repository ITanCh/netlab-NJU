// 文件名: stcp_server.c
//
// 描述: 这个文件包含服务器STCP套接字接口定义. 你需要实现所有这些接口.
//
// 创建日期: 2013年
//

#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "stcp_server.h"
#include "../common/constants.h"

//
//  用于服务器程序的STCP套接字API. 
//  ===================================
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现. 
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量, 
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void replyACK(seg_t *recv,seg_t *send,server_tcb_t *recvTCB,int ackType);

server_tcb_t *s_TCBTable[MAX_TRANSPORT_CONNECTIONS];
int g_son_conn;

pthread_t thread;

void stcp_server_init(int conn)
{
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

// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_server_sock(unsigned int server_port)
{
	int i = 0;
	for(; i < MAX_TRANSPORT_CONNECTIONS;i++)
		if(s_TCBTable[i] == NULL)
		{
			printf("[Server] find an empty TCB, position: %d \n",i);
			break;
		}
	if(i == MAX_TRANSPORT_CONNECTIONS)
		return -1;

	//初始化TCB信息
	server_tcb_t *newTCB = (server_tcb_t*)malloc(sizeof(server_tcb_t));
	newTCB->state = CLOSED;
	newTCB->server_portNum = server_port;
	newTCB->expect_seqNum = 0;
	newTCB->recvBuf = (char*)malloc(RECEIVE_BUF_SIZE);
	newTCB->usedBufLen = 0;
	newTCB->bufMutex = malloc(sizeof(pthread_mutex_t));

	//初始化缓冲区互斥变量
	if(pthread_mutex_init(newTCB->bufMutex, PTHREAD_MUTEX_TIMED_NP) != 0)
	{
		printf("fail to create buffer mutex\n");
		exit(-1);
	}

	//存入全局TCB表中
	s_TCBTable[i] = newTCB;
	printf("[Server] port(%d) created a new TCB\n",server_port);
	return i;
}

// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后启动定时器进入忙等待直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_server_accept(int sockfd)
{
	server_tcb_t *accTCB = s_TCBTable[sockfd];
	if(accTCB == NULL)
	{
		printf("[Server] accept TCB error\n");
		return -1;
	}
	accTCB->state = LISTENING;
	while(accTCB->state != CONNECTED)
		usleep(ACCEPT_POLLING_INTERVAL/1000);
	printf("[Server] port(%d) connection successfully\n",accTCB->server_portNum);
	return 1;
}

// 接收来自STCP客户端的数据. 请回忆STCP使用的是单向传输, 数据从客户端发送到服务器端.
// 信号/控制信息(如SYN, SYNACK等)则是双向传递. 这个函数每隔RECVBUF_ROLLING_INTERVAL时间
// 就查询接收缓冲区, 直到等待的数据到达, 它然后存储数据并返回1. 如果这个函数失败, 则返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_server_recv(int sockfd, void* buf, unsigned int length)
{
	if(length>RECEIVE_BUF_SIZE)return -1;
	printf("[Server] begin to fetch data\n");
	int tag;
	server_tcb_t *recvTCB = s_TCBTable[sockfd];
	if(recvTCB == NULL)
	{
		printf("[Server] fetch data Error\n");
		return -1;
	}
	while(1)
	{
		pthread_mutex_lock(recvTCB->bufMutex);
		if(recvTCB->usedBufLen >= length)
		{
			//printf("!!!!2!!!!!length:%d ,%d\n",recvTCB->usedBufLen,length);
			tag = 0;
			int i;
			/*
			   for(i=0;i<length;i++)
			   {
			   printf("%c",recvTCB->recvBuf[i]);
			   }*/	
			memcpy((char*)buf,recvTCB->recvBuf,length);
			//char *p = recvTCB->recvBuf + length;
			recvTCB->usedBufLen -= length;
			//memcpy(recvTCB->recvBuf,p,recvTCB->usedBufLen);
			for(;tag < recvTCB->usedBufLen;tag++)
			{
				recvTCB->recvBuf[tag] = recvTCB->recvBuf[tag+length];
				//	printf("%c",recvTCB->recvBuf[tag]);
			}
			pthread_mutex_unlock(recvTCB->bufMutex);
			return 1;
		}
		pthread_mutex_unlock(recvTCB->bufMutex);
		sleep(RECVBUF_POLLING_INTERVAL);
	}

}

// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_server_close(int sockfd)
{
	printf("close it now\n");
	server_tcb_t* closeTCB = s_TCBTable[sockfd];
	if(closeTCB == NULL)
	{
		printf("[Server] close TCB error\n");
		return -1;
	}
	//wait
	if(closeTCB->state!=CLOSED&&closeTCB->state!=CLOSEWAIT)
		sleep(CLOSEWAIT_TIMEOUT*2);
	if(closeTCB->state!=CLOSED&&closeTCB->state!=CLOSEWAIT)
		return -1;
	sleep(CLOSEWAIT_TIMEOUT);
	closeTCB->state = CLOSED;
	closeTCB->usedBufLen = 0;
	//释放接收缓冲区
	free(closeTCB->recvBuf);
	//销毁互斥变量
	pthread_mutex_destroy(closeTCB->bufMutex);
	//释放互斥变量
	free(closeTCB->bufMutex);
	free(closeTCB);
	s_TCBTable[sockfd] = NULL;
	return 1;
}

// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明重叠网络连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void* seghandler(void* arg)
{
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
			printf("[Server] Seg Lost!\n");
			continue;
		}
		if(checkchecksum(seg) == -1)
		{
			printf("[Server] Checksum Error!\n");
			continue;
		}
		printf("[Server] port(%d) received a seg\n",seg->header.dest_port);

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
			printf("[Server] Can't find the TCB item\n");
			continue;
		}
		switch(recvTCB->state)
		{
			case CLOSED:
				printf("[Server] State CLOSED\n");
				break;
			case LISTENING:
				printf("[Server] State LISTENING\n");
				if(seg->header.type == SYN)
				{
					recvTCB->client_portNum = seg->header.src_port;
					recvTCB->expect_seqNum = seg->header.seq_num + 0;
					replyACK(seg,segAck,recvTCB,SYNACK);
					recvTCB->state = CONNECTED;
				}
				break;
			case CLOSEWAIT:
				printf("[Server] State CLOSEWAIT\n");
				if(seg->header.type == FIN)
				{
					replyACK(seg,segAck,recvTCB,FINACK);
					recvTCB->state = CLOSEWAIT;
				}
				break;
			case CONNECTED:
				if(seg->header.type == SYN)
				{
					printf("[Server] State CONNECTED -> SYN\n");
					recvTCB->client_portNum = seg->header.src_port;
					recvTCB->expect_seqNum = seg->header.seq_num + 0;
					replyACK(seg,segAck,recvTCB,SYNACK);
					recvTCB->state = CONNECTED;
				}
				else if(seg->header.type == FIN)
				{
					printf("[Server] State CONNECTED -> FIN\n");
					if(recvTCB->expect_seqNum != seg->header.seq_num)
					{
						printf("[Server] FIN seq Error!");
					}
					else
					{
						replyACK(seg,segAck,recvTCB,FINACK);
						recvTCB->state = CLOSEWAIT;
					}
					//TODO closewait handle

				}
				else if(seg->header.type == DATA)
				{
					printf("[Server] State CONNECTED -> DATA\n");
					if(recvTCB->expect_seqNum == seg->header.seq_num)
					{
						printf("[Server] sequence equal, then save data\n");
						pthread_mutex_lock(recvTCB->bufMutex);
						printf("data length:%d\n",seg->header.length);
						memcpy(recvTCB->recvBuf+recvTCB->usedBufLen,seg->data,seg->header.length);
						recvTCB->usedBufLen += seg->header.length;
						recvTCB->expect_seqNum =(recvTCB->expect_seqNum+seg->header.length)%MAX_SEQNUM;
						pthread_mutex_unlock(recvTCB->bufMutex);

						replyACK(seg,segAck,recvTCB,DATAACK);
					}
					else
					{
						printf("[Server] sequence not equal\n");

						segAck->header.type = DATAACK;
						segAck->header.dest_port = recvTCB->client_portNum;
						segAck->header.src_port = recvTCB->server_portNum;
						segAck->header.ack_num = recvTCB->expect_seqNum;;
						segAck->header.checksum = checksum(segAck);

						sip_sendseg(g_son_conn,segAck);
						printf("[Server] ACK to client(%d)\n",recvTCB->client_portNum);
					}
				}
		}

	}
	return 0;
}


void replyACK(seg_t *recv,seg_t *send,server_tcb_t *recvTCB,int ackType) 
{
	printf("[Server] begin to send ACk to client(%d)\n",recvTCB->client_portNum);

	send->header.type = ackType;
	send->header.dest_port = recvTCB->client_portNum;
	send->header.src_port = recvTCB->server_portNum;
	send->header.ack_num = recvTCB->expect_seqNum;
	memset(&(send->header.checksum),0,sizeof(unsigned int));
	send->header.checksum = checksum(send);

	sip_sendseg(g_son_conn,send);
	printf("[Server] finish sending ACK to client(%d)\n",recvTCB->client_portNum);
}
