//文件名: client/stcp_client.c
//
//描述: 这个文件包含STCP客户端接口实现 
//
//创建日期: 2013年1月

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <assert.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "../topology/topology.h"
#include "stcp_client.h"
#include "../common/seg.h"

//声明tcbtable为全局变量
client_tcb_t *TCBTable[MAX_TRANSPORT_CONNECTIONS];
void usleep(unsigned long usec);
//声明到SIP进程的TCP连接为全局变量
int sip_conn;
int myNodeID;
/*********************************************************************/
//
//STCP API实现
//
/*********************************************************************/

// 这个函数初始化TCB表, 将所有条目标记为NULL.  
// 它还针对TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
void stcp_client_init(int conn) 
{
	int i;
	for(i=0;i<MAX_TRANSPORT_CONNECTIONS;i++)
	{
		TCBTable[i]=NULL;
	}
	sip_conn=conn;
	myNodeID=topology_getMyNodeID();
	pthread_t thread;		
	int rc;
	rc=pthread_create(&thread,NULL,seghandler,NULL);
	if(rc){
		printf("ERROE;return code from pthread_create() is %d\n",rc);
	}
	printf("stcp client init\n");
	return;
}

// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
int stcp_client_sock(unsigned int client_port) 
{
	int i=0;
	for(;i<MAX_TRANSPORT_CONNECTIONS;i++)
	{
		if(TCBTable[i]==NULL)break;
	}
	if(i==MAX_TRANSPORT_CONNECTIONS)return -1;
	TCBTable[i]=malloc(sizeof(client_tcb_t));
	client_tcb_t* t=TCBTable[i];
	t->client_portNum=client_port;
	t->state=CLOSED;	
	t->next_seqNum=0;
	t->bufMutex=malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(t->bufMutex,NULL);
	t->sendBufHead=NULL;
	t->sendBufunSent=NULL;
	t->sendBufTail=NULL;
	t->unAck_segNum=0;

	t->recvBufMutex=malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(t->recvBufMutex,NULL);
	t->expect_seqNum=0;
	t->recvBuf=malloc(RECEIVE_BUF_SIZE);
	t->usedBufLen=0;
	printf("stcp client sock\n");
	return i;
}

// 这个函数用于连接服务器. 它以套接字ID, 服务器节点ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器节点ID和服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
int stcp_client_connect(int sockfd, int nodeID, unsigned int server_port) 
{
	client_tcb_t* t=TCBTable[sockfd];
	if(t==NULL)return -1;
	t->server_nodeID=nodeID;			//id is useful
	t->server_portNum=server_port;
	t->client_nodeID=myNodeID;

	seg_t *seg=malloc(sizeof(seg_t));
	seg->header.src_port=t->client_portNum;
	seg->header.dest_port=t->server_portNum;
	seg->header.seq_num=t->next_seqNum;
	seg->header.ack_num=0;
	seg->header.length=0;
	t->next_seqNum=(t->next_seqNum+0)%MAX_SEQNUM;
	seg->header.type=SYN;
	seg->header.checksum=0;
	seg->header.checksum=checksum(seg);
	int i=0;
	int flag=0;
	while(flag<SYN_MAX_RETRY)
	{
		t->state=SYNSENT;
		flag++;
		i=sip_sendseg(sip_conn,nodeID,seg);
		double start=clock();
		if(i<0)continue;
		while(1)
		{
			if(t->state==CONNECTED)	//connect 
			{
				printf("connct to server successfully\n");
				return 1;
			}
			double now=clock();
			if(now-start>SYN_TIMEOUT/100){
				printf("SYN time out:%f\n",now-start);
				break;	//time out resend
			}
		}
	}
	t->state=CLOSED;
	printf("cannot connect to the server\n");
	return -1;
}

// 发送数据给STCP服务器. 这个函数使用套接字ID找到TCB表中的条目.
// 然后它使用提供的数据创建segBuf, 将它附加到发送缓冲区链表中.
// 如果发送缓冲区在插入数据之前为空, 一个名为sendbuf_timer的线程就会启动.
// 每隔SENDBUF_ROLLING_INTERVAL时间查询发送缓冲区以检查是否有超时事件发生. 
// 这个函数在成功时返回1，否则返回-1. 
// stcp_client_send是一个非阻塞函数调用.
// 因为用户数据被分片为固定大小的STCP段, 所以一次stcp_client_send调用可能会产生多个segBuf
// 被添加到发送缓冲区链表中. 如果调用成功, 数据就被放入TCB发送缓冲区链表中, 根据滑动窗口的情况,
// 数据可能被传输到网络中, 或在队列中等待传输.
int stcp_client_send(int sockfd, void* data, unsigned int length) 
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
	client_tcb_t *t=TCBTable[sockfd];
	if(t==NULL)return -1;
	segBuf_t* sb=malloc(sizeof(segBuf_t));
	sb->next=NULL;
	//insert data
	memcpy(sb->seg.data,data,length);
	//init stcp head
	sb->seg.header.src_port=t->client_portNum;
	sb->seg.header.dest_port=t->server_portNum;
	sb->seg.header.seq_num=t->next_seqNum;
	sb->seg.header.length=length;
	t->next_seqNum=(t->next_seqNum+length)%MAX_SEQNUM;
	sb->seg.header.ack_num=0;
	sb->seg.header.type=DATA;
	sb->seg.header.checksum=0;
	sb->seg.header.checksum=checksum(&(sb->seg));
	//printf("!!!!!!checkchecksum:%d\n",checkchecksum(&(sb->seg)));
	pthread_mutex_lock(t->bufMutex);
	if(t->sendBufHead==NULL)
	{
		t->sendBufHead=sb;
		t->sendBufTail=sb;
		int i=sip_sendseg(sip_conn,t->server_nodeID,&(sb->seg));
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
			int i=sip_sendseg(sip_conn,t->server_nodeID,&(t->sendBufunSent->seg));
			if(i<0)printf("error sip send seg error\n");
			t->sendBufunSent->sentTime=(unsigned int)clock();
			t->sendBufunSent=t->sendBufunSent->next;
			t->unAck_segNum++;
		}
	}
	pthread_mutex_unlock(t->bufMutex);
	return 1;
}

//recv
int stcp_client_recv(int sockfd, void* buf, unsigned int length) 
{

	//printf("client-Recv: begin to fetch data\n");
	if(length > RECEIVE_BUF_SIZE)
	{
		printf("client-Recv: too much data\n");
		return -1;
	}	
	int tag;
	client_tcb_t *recvTCB = TCBTable[sockfd];
	if(recvTCB == NULL)
	{
		printf("client-Recv: TCB is NULL Error\n");
		return -1;
	}
	while(1)
	{
		pthread_mutex_lock(recvTCB->recvBufMutex);
		if(recvTCB->usedBufLen >= length)
		{
			tag = 0;
			memcpy((char*)buf,recvTCB->recvBuf,length);

			recvTCB->usedBufLen -= length;
			for(;tag < recvTCB->usedBufLen;tag++)
			{
				recvTCB->recvBuf[tag] = recvTCB->recvBuf[tag+length];
				//	printf("%c",recvTCB->recvBuf[tag]);
			}
			pthread_mutex_unlock(recvTCB->recvBufMutex);
			return 1;
		}
		pthread_mutex_unlock(recvTCB->recvBufMutex);
		sleep(RECVBUF_POLLING_INTERVAL);
	}
}

// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN段给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
int stcp_client_disconnect(int sockfd) 
{
	client_tcb_t * t=TCBTable[sockfd];
	if(t==NULL)return -1;
	int flag=1;
	while(flag)
	{
		pthread_mutex_lock(t->bufMutex);
		if(t->sendBufHead==NULL)flag=0;
		pthread_mutex_unlock(t->bufMutex);
		sleep(CLOSEWAIT_TIMEOUT);
	}
	seg_t *seg=malloc(sizeof(seg_t));
	seg->header.src_port=t->client_portNum;
	seg->header.dest_port=t->server_portNum;
	seg->header.length=0;
	seg->header.type=FIN;
	printf("fin seq num:%d\n",t->next_seqNum);
	seg->header.seq_num=t->next_seqNum;
	seg->header.ack_num=0;
	t->next_seqNum=(t->next_seqNum+0)%MAX_SEQNUM;
	seg->header.checksum=0;
	seg->header.checksum=checksum(seg);
	//printf("!!!!!!checkchecksum:%d\n",checkchecksum(&(sb->seg)));
	int i=0;
	flag=0;
	while(flag<FIN_MAX_RETRY)
	{
		t->state=FINWAIT;
		flag++;
		i=sip_sendseg(sip_conn,t->server_nodeID,seg);
		double start=clock();
		if(i<0)continue;
		while(1)
		{
			if(t->state==CLOSED)	//closed
			{
				printf("disconnect successfully\n");
				return 1;
			}
			double now=clock();
			if(now-start>CLOSEWAIT_TIMEOUT*1000000)
			{
				printf("FIN time out:%f\n",now-start);
				break;	//time out resend
			}
		}
	}
	printf("disconnect unsuccessfully\n");
	t->state=CLOSED;
	return -1;
}

// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
int stcp_client_close(int sockfd) 
{
	client_tcb_t* t=TCBTable[sockfd];
	if(t==NULL)return 1;
	if(t->state!=CLOSED){
		printf("close unsuccessfully\n");
		return -1;
	}
	free(t->bufMutex);
	free(t->recvBufMutex);
	free(t->recvBuf);
	free(t);
	TCBTable[sockfd]=NULL;
	printf("close successfully\n");
	return 1;
}

// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明到SIP进程的连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
void* seghandler(void* arg) 
{
	int i;
	seg_t* seg=malloc(sizeof(seg_t));
	client_tcb_t *t=NULL;
	segBuf_t* sbp;
	segBuf_t* sbp2;
	int dataack=-1;
	while(1)
	{
		sbp=NULL;
		sbp2=NULL;
		memset(seg,0,sizeof(seg_t));
		int srcNode;
		i=sip_recvseg(sip_conn,&srcNode,seg);
		if(i<0)
		{
			printf("seg lost!\n");
			continue;	//this seg lost
		}
		if(checkchecksum(seg)==-1){
			printf("checksum error\n");
			continue;
		}
		//printf("receive a seg dest is :%d \nack is :%d type is:%d\n",seg->header.dest_port,seg->header.ack_num,seg->header.type);
		t=NULL;
		for(i=0;i<MAX_TRANSPORT_CONNECTIONS;i++)
		{
			if(TCBTable[i]!=NULL&&TCBTable[i]->server_portNum==seg->header.src_port&&TCBTable[i]->client_portNum==seg->header.dest_port){
				t=TCBTable[i];
				break;
			}
		}
		if(t==NULL)continue;
		switch(t->state)
		{
			case CLOSED:break;
			case SYNSENT:if(seg->header.ack_num==t->next_seqNum&&seg->header.type==SYNACK)
							 t->state=CONNECTED;
						 break;
			case FINWAIT:if(seg->header.ack_num==t->next_seqNum&&seg->header.type==FINACK)
						 {
							 t->state=CLOSED;
							 break;
						 }
						 //go to connected
			case CONNECTED:
						 if(seg->header.type==DATA)
						 {
							 //printf("client: CONNECTED got DATA\n");
							 if(t->expect_seqNum == seg->header.seq_num)
							 {
								 //printf("client-DATA: sequence equal, then save data\n");
								 pthread_mutex_lock(t->recvBufMutex);
								 //printf("data length:%d\n",seg->header.length);
								 memcpy(t->recvBuf+t->usedBufLen,seg->data,seg->header.length);
								 t->usedBufLen += seg->header.length;
								 t->expect_seqNum =(t->expect_seqNum+seg->header.length)%MAX_SEQNUM;
								 pthread_mutex_unlock(t->recvBufMutex);
								 replyACK(t,DATAACK);
							 }
							 else
							 {
								 //printf("client-DATA: sequence not equal\n");
								 replyACK(t,DATAACK);
							 }
						 }
						 else if(seg->header.type==DATAACK)
						 {
							 pthread_mutex_lock(t->bufMutex);
							 sbp=t->sendBufHead;
							 if(sbp==NULL){
								 pthread_mutex_unlock(t->bufMutex);
								 break;			//no data has been sent
							 }
							 dataack=seg->header.ack_num;//%MAX_SEQNUM
							 //all seg have been sent will be ack
							 if((t->sendBufunSent!=NULL&&dataack==t->sendBufunSent->seg.header.seq_num)||(t->sendBufunSent==NULL&&t->next_seqNum==dataack))
							 {
								 while(t->sendBufHead!=t->sendBufunSent)
								 {
									 sbp=t->sendBufHead;
									 t->sendBufHead=t->sendBufHead->next;
									 free(sbp);
									 t->unAck_segNum--;
									 if(t->sendBufHead==NULL)t->sendBufTail=NULL;
								 }
							 }
							 else
							 {
								 while(sbp!=t->sendBufunSent)
								 {
									 if(dataack==sbp->seg.header.seq_num)break;
									 sbp=sbp->next;
								 }
								 if(sbp!=t->sendBufunSent)
								 {
									 while(t->sendBufHead!=sbp)
									 {
										 sbp2=t->sendBufHead;
										 t->sendBufHead=t->sendBufHead->next;
										 t->unAck_segNum--;
										 free(sbp2);
									 }
								 }
							 }
							 //send some unSend seg
							 while(t->sendBufunSent!=NULL&&t->unAck_segNum<GBN_WINDOW)		
							 {
								 i=sip_sendseg(sip_conn,t->server_nodeID,&(t->sendBufunSent->seg));
								 //if(i<0)printf("error sip send seg error\n");
								 t->sendBufunSent->sentTime=(unsigned int)clock();
								 t->sendBufunSent=t->sendBufunSent->next;
								 t->unAck_segNum++;
							 }
							 pthread_mutex_unlock(t->bufMutex);
						 }
						 break;
		}
	}
	return 0;
}

void replyACK(client_tcb_t *recvTCB,int ackType) 
{
	seg_t*send=malloc(sizeof(seg_t));
	send->header.type = ackType;
	send->header.dest_port = recvTCB->server_portNum;
	send->header.src_port = recvTCB->client_portNum;
	send->header.ack_num = recvTCB->expect_seqNum;
	memset(&(send->header.checksum),0,sizeof(unsigned int));
	send->header.checksum = checksum(send);

	sip_sendseg(sip_conn,recvTCB->server_nodeID,send);
	//printf("client-ACK: finish sending ACK to server(%d)\n",recvTCB->server_portNum);
}


//这个线程持续轮询发送缓冲区以触发超时事件. 如果发送缓冲区非空, 它应一直运行.
//如果(当前时间 - 第一个已发送但未被确认段的发送时间) > DATA_TIMEOUT, 就发生一次超时事件.
//当超时事件发生时, 重新发送所有已发送但未被确认段. 当发送缓冲区为空时, 这个线程将终止.
void* sendBuf_timer(void* clienttcb) 
{
	client_tcb_t * t=(client_tcb_t*)clienttcb;
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
			//printf("time out resend them:%d\n",now-t->sendBufHead->sentTime);
			segBuf_t* sb=t->sendBufHead;
			while(sb!=t->sendBufunSent)
			{
				sip_sendseg(sip_conn,t->server_nodeID,&(sb->seg));
				//if(i<0)printf("error sip send seg error\n");
				sb->sentTime=(unsigned int)clock();
				sb=sb->next;
			}
		}
		pthread_mutex_unlock(t->bufMutex);
		//usleep(SENDBUF_POLLING_INTERVAL/1000);		// s
	}
}

