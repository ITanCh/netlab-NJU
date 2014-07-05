//文件名: server/stcp_server.c
//
//描述: 这个文件包含STCP服务器接口实现. 
//
//创建日期: 2013年1月

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/select.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include "stcp_server.h"
#include "../topology/topology.h"
#include "../common/constants.h"


void replyACK(seg_t *send,server_tcb_t *recvTCB,int ackType);
void usleep(unsigned long usec);
//声明tcbtable为全局变量
server_tcb_t* tcbtable[MAX_TRANSPORT_CONNECTIONS];
//声明到SIP进程的连接为全局变量
int sip_conn;
unsigned int myPort;
int myID;
/*********************************************************************/
//
//STCP API实现
//
/*********************************************************************/

pthread_t thread;

// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对TCP套接字描述符conn初始化一个STCP层的全局变量, 
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
void stcp_server_init(int conn) 
{
	//init
	int i = 0;
	for(; i < MAX_TRANSPORT_CONNECTIONS;i++)
		tcbtable[i] = NULL;
	//set global variable
	sip_conn = conn;
	int rc = pthread_create(&thread,NULL,seghandler,NULL);
	if(rc)
	{
		printf("Server-init ERROR: return code from pthread_create() is %d\n",rc);
		exit(-1);
	}
	return;
}

// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
int stcp_server_sock(unsigned int server_port) 
{
	myPort=server_port;
	myID=topology_getMyNodeID();
	return 1;
}

// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后启动定时器进入忙等待直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
int stcp_server_accept() 
{
	int i;
	while(1)
	{
		for(i = 0; i < MAX_TRANSPORT_CONNECTIONS;i++)
			if(tcbtable[i]!=NULL&&tcbtable[i]->state==CLOSED)
			{
				tcbtable[i]->state=LISTENING;
				printf("Server-Accept: nodeID(%d) connection successfully\n",tcbtable[i]->client_nodeID);
				return i;
			}
	}
	return -1;
}

// 接收来自STCP客户端的数据. 这个函数每隔RECVBUF_POLLING_INTERVAL时间
// 就查询接收缓冲区, 直到等待的数据到达, 它然后存储数据并返回1. 如果这个函数失败, 则返回-1.
int stcp_server_recv(int sockfd, void* buf, unsigned int length) 
{
	printf("Server-Recv: begin to fetch data\n");
	if(length > RECEIVE_BUF_SIZE)
	{
		printf("Server-Recv: too much data\n");
		return -1;
	}	
	int tag;
	server_tcb_t *recvTCB = tcbtable[sockfd];
	if(recvTCB == NULL)
	{
		printf("Server-Recv: TCB is NULL Error\n");
		return -1;
	}
	while(1)
	{
		pthread_mutex_lock(recvTCB->bufMutex);
		if(recvTCB->usedBufLen >= length)
		{
			tag = 0;
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

//send
int stcp_server_send(int sockfd, void* data, unsigned int length) 
{
	char * cdata=(char *)data;
	int i;
	if(length>RECEIVE_BUF_SIZE)return -1;
	while(length>MAX_SEG_LEN)
	{
		i=stcp_send(sockfd,cdata,MAX_SEG_LEN);
		if(i<=0)return -1;
		cdata+=MAX_SEG_LEN;
		length-=MAX_SEG_LEN;
	}
	i=stcp_send(sockfd,cdata,length);
	return i;
}

int stcp_send(int sockfd, void* data, unsigned int length) {
	server_tcb_t *t=tcbtable[sockfd];
	if(t==NULL)return -1;
	segBuf_t* sb=malloc(sizeof(segBuf_t));
	sb->next=NULL;
	//insert data
	memcpy(sb->seg.data,data,length);
	//init stcp head
	sb->seg.header.src_port=t->server_portNum;
	sb->seg.header.dest_port=t->client_portNum;
	sb->seg.header.seq_num=t->next_seqNum;
	sb->seg.header.length=length;
	t->next_seqNum=(t->next_seqNum+length)%MAX_SEQNUM;
	sb->seg.header.ack_num=0;
	sb->seg.header.type=DATA;
	sb->seg.header.checksum=0;
	sb->seg.header.checksum=checksum(&(sb->seg));
	//printf("!!!!!!checkchecksum:%d\n",checkchecksum(&(sb->seg)));
	pthread_mutex_lock(t->sendBufMutex);
	if(t->sendBufHead==NULL)
	{
		t->sendBufHead=sb;
		t->sendBufTail=sb;
		int i=sip_sendseg(sip_conn,t->client_nodeID,&(sb->seg));
		if(i<0)printf("error sip send \n");
		t->unAck_segNum++;
		sb->sentTime=clock();
		pthread_t timer_thread;		
		int rc=pthread_create(&timer_thread,NULL,sendBuf_timer,(void*)t);
		if(rc){
			printf("ERROE;return code from pthread_create() is %d\n",rc);
		}
	}
	else
	{
		t->sendBufTail->next=sb;
		t->sendBufTail=sb;
		if(t->sendBufunSent==NULL)			//all buf have been sent
			t->sendBufunSent=sb;
		while(t->unAck_segNum<GBN_WINDOW&&t->sendBufunSent!=NULL)
		{
			int i=sip_sendseg(sip_conn,t->client_nodeID,&(t->sendBufunSent->seg));
			if(i<0)printf("error sip send seg error\n");
			t->sendBufunSent->sentTime=(unsigned int)clock();
			t->sendBufunSent=t->sendBufunSent->next;
			t->unAck_segNum++;
		}
	}
	pthread_mutex_unlock(t->sendBufMutex);
	return 1;
}


// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
int stcp_server_close(int sockfd) 
{
	printf("Server-Close: begin to close %d\n",sockfd);
	server_tcb_t* closeTCB = tcbtable[sockfd];
	if(closeTCB == NULL)
	{
		printf("Server-Close: TCB is NULL error\n");
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
	pthread_mutex_destroy(closeTCB->sendBufMutex);
	//释放互斥变量
	free(closeTCB->bufMutex);
	free(closeTCB);
	tcbtable[sockfd] = NULL;
	return 1;
}

// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明到SIP进程的连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
void* seghandler(void* arg) 
{
	int i;
	seg_t *seg = (seg_t*)malloc(sizeof(seg_t));
	seg_t *segAck = (seg_t*)malloc(sizeof(seg_t));
	server_tcb_t *recvTCB = NULL;
	segBuf_t* sbp;
	segBuf_t* sbp2;
	int dataack=-1;
	int src_nodeID = 0;
	while(1)
	{
		recvTCB = NULL;
		memset(seg,0,sizeof(seg_t));
		memset(segAck,0,sizeof(seg_t));
		i = sip_recvseg(sip_conn,&src_nodeID,seg);
		if(i < 0)	//seg lost
		{
			printf("Server-Seghandler: Seg Lost!\n");
			continue;
		}
		if(checkchecksum(seg) == -1)
		{
			printf("Server-Seghandler Checksum Error!\n");
			continue;
		}
		printf("Server-Seghandler: port(%d) received a seg\n",seg->header.dest_port);

		for(i = 0; i < MAX_TRANSPORT_CONNECTIONS;i++)
		{
			if(tcbtable[i] != NULL && tcbtable[i]->client_portNum == seg->header.src_port&&tcbtable[i]->server_portNum == seg->header.dest_port&&tcbtable[i]->client_nodeID==src_nodeID) 
			{
				recvTCB = tcbtable[i];
				break;
			}
		}

		//a new client link in...
		if(recvTCB == NULL)
		{
			printf("Server-Seghandler: Can't find the TCB item\n");
			if(seg->header.dest_port==myPort)
			{
				for(i=0; i < MAX_TRANSPORT_CONNECTIONS;i++)
					if(tcbtable[i] == NULL)
					{
						printf("Server-STCP: find an empty TCB, position: %d \n",i);
						break;
					}
				if(i == MAX_TRANSPORT_CONNECTIONS)
					continue;
				//初始化TCB信息
				server_tcb_t *newTCB = (server_tcb_t*)malloc(sizeof(server_tcb_t));
				newTCB->state = CLOSED;
				newTCB->server_portNum = myPort;
				newTCB->recvBuf = (char*)malloc(RECEIVE_BUF_SIZE);
				newTCB->usedBufLen = 0;
				newTCB->bufMutex = malloc(sizeof(pthread_mutex_t));
				pthread_mutex_init(newTCB->bufMutex, PTHREAD_MUTEX_TIMED_NP);
				newTCB->client_portNum = seg->header.src_port;
				newTCB->expect_seqNum = seg->header.seq_num;
				newTCB->client_nodeID = src_nodeID;
				newTCB->server_nodeID=myID;

				newTCB->next_seqNum=0;
				newTCB->sendBufMutex=malloc(sizeof(pthread_mutex_t));
				pthread_mutex_init(newTCB->sendBufMutex,NULL);
				newTCB->sendBufHead=NULL;
				newTCB->sendBufunSent=NULL;
				newTCB->sendBufTail=NULL;
				newTCB->unAck_segNum=0;

				//存入全局TCB表中
				tcbtable[i] = newTCB;
				printf("Server-Sock: a new client in...client_nodeID(%d) ...created a new TCB\n",newTCB->client_nodeID);
			}
			continue;
		}
		switch(recvTCB->state)
		{
			case CLOSED:
				printf("Server-State: CLOSED\n");
				break;
			case LISTENING:
				printf("Server-State: LISTENING\n");
				if(seg->header.type == SYN)
				{
					printf("Server-State: LISTENING got SYN\n");
					recvTCB->client_portNum = seg->header.src_port;
					recvTCB->expect_seqNum = seg->header.seq_num;
					recvTCB->client_nodeID = src_nodeID;
					replyACK(segAck,recvTCB,SYNACK);
					recvTCB->state = CONNECTED;
				}
				break;
			case CLOSEWAIT:
				printf("Server-State: CLOSEWAIT\n");
				if(seg->header.type == FIN)
				{
					printf("Server-State: CLOSEWAIT got FIN\n");
					recvTCB->client_nodeID = src_nodeID;
					replyACK(segAck,recvTCB,FINACK);
					recvTCB->state = CLOSEWAIT;
					break;
				}
			case CONNECTED:
				printf("Server-State: CONNECTED\n");
				if(seg->header.type == SYN)
				{
					printf("Server-State: CONNECTED got SYN\n");
					recvTCB->client_portNum = seg->header.src_port;
					recvTCB->expect_seqNum = seg->header.seq_num;
					recvTCB->client_nodeID = src_nodeID;
					replyACK(segAck,recvTCB,SYNACK);
					recvTCB->state = CONNECTED;
				}
				else if(seg->header.type == FIN)
				{
					recvTCB->client_nodeID = src_nodeID;
					printf("Server-State: CONNECTED got FIN\n");
					if(recvTCB->expect_seqNum != seg->header.seq_num)
					{
						printf("Server-FIN: seq Error!");
					}
					else
					{
						replyACK(segAck,recvTCB,FINACK);
						recvTCB->state = CLOSEWAIT;
					}

				}
				else if(seg->header.type==DATAACK)
				{
					pthread_mutex_lock(recvTCB->sendBufMutex);
					sbp=recvTCB->sendBufHead;
					if(sbp==NULL){
						pthread_mutex_unlock(recvTCB->sendBufMutex);
						break;			//no data has been sent
					}
					dataack=seg->header.ack_num;//%MAX_SEQNUM
					//all seg have been sent will be ack
					if((recvTCB->sendBufunSent!=NULL&&dataack==recvTCB->sendBufunSent->seg.header.seq_num)||(recvTCB->sendBufunSent==NULL&&recvTCB->next_seqNum==dataack))
					{
						while(recvTCB->sendBufHead!=recvTCB->sendBufunSent)
						{
							sbp=recvTCB->sendBufHead;
							recvTCB->sendBufHead=recvTCB->sendBufHead->next;
							free(sbp);
							recvTCB->unAck_segNum--;
							if(recvTCB->sendBufHead==NULL)recvTCB->sendBufTail=NULL;
						}
					}
					else
					{
						while(sbp!=recvTCB->sendBufunSent)
						{
							if(dataack==sbp->seg.header.seq_num)break;
							sbp=sbp->next;
						}
						if(sbp!=recvTCB->sendBufunSent)
						{
							while(recvTCB->sendBufHead!=sbp)
							{
								sbp2=recvTCB->sendBufHead;
								recvTCB->sendBufHead=recvTCB->sendBufHead->next;
								recvTCB->unAck_segNum--;
								free(sbp2);
							}
						}
					}
					//send some unSend seg
					while(recvTCB->sendBufunSent!=NULL&&recvTCB->unAck_segNum<GBN_WINDOW)		
					{
						i=sip_sendseg(sip_conn,recvTCB->client_nodeID,&(recvTCB->sendBufunSent->seg));
						if(i<0)printf("error sip send seg error\n");
						recvTCB->sendBufunSent->sentTime=(unsigned int)clock();
						recvTCB->sendBufunSent=recvTCB->sendBufunSent->next;
						recvTCB->unAck_segNum++;
					}
					pthread_mutex_unlock(recvTCB->sendBufMutex);
				}

				else if(seg->header.type == DATA)
				{
					printf("Server-State: CONNECTED got DATA\n");
					if(recvTCB->expect_seqNum == seg->header.seq_num)
					{
						printf("Server-DATA: sequence equal, then save data\n");
						pthread_mutex_lock(recvTCB->bufMutex);
						printf("data length:%d\n",seg->header.length);
						memcpy(recvTCB->recvBuf+recvTCB->usedBufLen,seg->data,seg->header.length);
						recvTCB->usedBufLen += seg->header.length;

						recvTCB->expect_seqNum =(recvTCB->expect_seqNum+seg->header.length)%MAX_SEQNUM;
						pthread_mutex_unlock(recvTCB->bufMutex);

						replyACK(segAck,recvTCB,DATAACK);
					}
					else
					{
						printf("Server-DATA: sequence not equal\n");
						replyACK(segAck,recvTCB,DATAACK);
					}
				}
		}

	}
	return 0;
}


void replyACK(seg_t *send,server_tcb_t *recvTCB,int ackType) 
{
	printf("Server-ACK: begin to send ACk to client(%d)\n",recvTCB->client_portNum);

	send->header.type = ackType;
	send->header.dest_port = recvTCB->client_portNum;
	send->header.src_port = recvTCB->server_portNum;
	send->header.ack_num = recvTCB->expect_seqNum;
	memset(&(send->header.checksum),0,sizeof(unsigned int));
	send->header.checksum = checksum(send);

	sip_sendseg(sip_conn,recvTCB->client_nodeID,send);
	printf("Server-ACK: finish sending ACK to client(%d)\n",recvTCB->client_portNum);
}

//这个线程持续轮询发送缓冲区以触发超时事件. 如果发送缓冲区非空, 它应一直运行.
//如果(当前时间 - 第一个已发送但未被确认段的发送时间) > DATA_TIMEOUT, 就发生一次超时事件.
//当超时事件发生时, 重新发送所有已发送但未被确认段. 当发送缓冲区为空时, 这个线程将终止.
void* sendBuf_timer(void* tcb) 
{
	server_tcb_t * t=(server_tcb_t*)tcb;
	if(t==NULL)return NULL;
	while(1)
	{
		pthread_mutex_lock(t->bufMutex);
		if(t->sendBufHead==NULL)
		{
			pthread_mutex_unlock(t->bufMutex);
			return NULL;
		}
		if(t->sendBufunSent==t->sendBufHead)
		{
			pthread_mutex_unlock(t->bufMutex);
			continue;
		}
		unsigned int now=(unsigned int)clock();
		if(now - t->sendBufHead->sentTime>DATA_TIMEOUT/1000000)
		{
			printf("time out resend them:%d\n",now-t->sendBufHead->sentTime);
			segBuf_t* sb=t->sendBufHead;
			while(sb!=t->sendBufunSent)
			{
				int i=sip_sendseg(sip_conn,t->client_nodeID,&(sb->seg));
				if(i<0)printf("error sip send seg error\n");
				sb->sentTime=(unsigned int)clock();
				sb=sb->next;
			}
		}
		pthread_mutex_unlock(t->bufMutex);
		usleep(SENDBUF_POLLING_INTERVAL/1000);		// s
	}
}

