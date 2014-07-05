#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include "stcp_client.h"

/*面向应用层的接口*/

//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// stcp客户端初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL.  
// 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
client_tcb_t *TCBTable[MAX_TRANSPORT_CONNECTIONS];
int son_conn;

pthread_t thread;		
int rc;
void * status;

void stcp_client_init(int conn) {
	int i;
	for(i=0;i<MAX_TRANSPORT_CONNECTIONS;i++)
	{
		TCBTable[i]=NULL;
	}
	son_conn=conn;
	rc=pthread_create(&thread,NULL,seghandler,NULL);
	if(rc){
		printf("ERROE;return code from pthread_create() is %d\n",rc);
	}
	printf("stcp client init\n");
	return;
}

// 创建一个客户端TCB条目, 返回套接字描述符
//
// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_sock(unsigned int client_port) {
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
	printf("stcp client sock\n");
	return i;
}

// 连接STCP服务器
//
// 这个函数用于连接服务器. 它以套接字ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_client_connect(int sockfd, unsigned int server_port) {
	client_tcb_t* t=TCBTable[sockfd];
	if(t==NULL)return -1;
	t->server_portNum=server_port;

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
		i=sip_sendseg(son_conn,seg);
		double start=clock();
		if(i<0)continue;
		while(1)
		{
			if(t->state==CONNECTED)	//connect 
			{ 
				printf("connct successfully\n");
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

// 发送数据给STCP服务器
//
// 这个函数发送数据给STCP服务器//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_client_send(int sockfd, void* data, unsigned int length) 
{
	if(length>RECEIVE_BUF_SIZE)return -1;
	while(length>MAX_SEG_LEN)
	{
		stcp_send(sockfd,data,MAX_SEG_LEN);
		data+=MAX_SEG_LEN;
		length-=MAX_SEG_LEN;
	}
	stcp_send(sockfd,data,length);
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
		int i=sip_sendseg(son_conn,&(sb->seg));
		if(i<0)printf("error sip send \n");
		t->unAck_segNum++;
		sb->sentTime=clock();
		pthread_t timer_thread;		
		rc=pthread_create(&thread,NULL,sendBuf_timer,(void*)t);
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
			int i=sip_sendseg(son_conn,&(t->sendBufunSent->seg));
			if(i<0)printf("error sip send seg error\n");
			t->sendBufunSent->sentTime=(unsigned int)clock();
			t->sendBufunSent=t->sendBufunSent->next;
			t->unAck_segNum++;
		}
	}
	pthread_mutex_unlock(t->bufMutex);
	return 1;
}

// 断开到STCP服务器的连接
//
// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN segment给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_disconnect(int sockfd) {
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
		i=sip_sendseg(son_conn,seg);
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

// 关闭STCP客户
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_close(int sockfd) {
	client_tcb_t* t=TCBTable[sockfd];
	if(t==NULL)return 1;
	if(t->state!=CLOSED){
		printf("close unsuccessfully\n");
		return -1;
	}
	free(t->bufMutex);
	free(t);
	TCBTable[sockfd]==NULL;
	printf("close successfully\n");
	return 1;
}

// 处理进入段的线程
//
// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明重叠网络连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void *seghandler(void* arg) {	
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
		i=sip_recvseg(son_conn,seg);
		if(i<0)continue;	//this seg lost
		if(checkchecksum(seg)==-1){
			printf("checksum error\n");
			continue;
		}
		printf("receive a seg dest is :%d \nack is :%d type is:%d\n",seg->header.dest_port,seg->header.ack_num,seg->header.type);
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
			case CONNECTED:if(seg->header.type==DATAACK)
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
								   i=sip_sendseg(son_conn,&(t->sendBufunSent->seg));
								   if(i<0)printf("error sip send seg error\n");
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

void* sendBuf_timer(void* clienttcb){
	client_tcb_t * t=(client_tcb_t*)clienttcb;
	if(t==NULL)return;
	while(1)
	{
		pthread_mutex_lock(t->bufMutex);
		if(t->sendBufHead==NULL)
		{
			pthread_mutex_unlock(t->bufMutex);
			return;
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
				int i=sip_sendseg(son_conn,&(sb->seg));
				if(i<0)printf("error sip send seg error\n");
				sb->sentTime=(unsigned int)clock();
				sb=sb->next;
			}
		}
		pthread_mutex_unlock(t->bufMutex);
		usleep(SENDBUF_POLLING_INTERVAL/1000);		// s
	}
};

