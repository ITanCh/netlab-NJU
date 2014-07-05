//�ļ���: client/stcp_client.c
//
//����: ����ļ�����STCP�ͻ��˽ӿ�ʵ�� 
//
//��������: 2013��1��

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

//����tcbtableΪȫ�ֱ���
client_tcb_t *TCBTable[MAX_TRANSPORT_CONNECTIONS];
void usleep(unsigned long usec);
//������SIP���̵�TCP����Ϊȫ�ֱ���
int sip_conn;
int myNodeID;
/*********************************************************************/
//
//STCP APIʵ��
//
/*********************************************************************/

// ���������ʼ��TCB��, ��������Ŀ���ΪNULL.  
// �������TCP�׽���������conn��ʼ��һ��STCP���ȫ�ֱ���, �ñ�����Ϊsip_sendseg��sip_recvseg���������.
// ���, �����������seghandler�߳�����������STCP��. �ͻ���ֻ��һ��seghandler.
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

// ����������ҿͻ���TCB�����ҵ���һ��NULL��Ŀ, Ȼ��ʹ��malloc()Ϊ����Ŀ����һ���µ�TCB��Ŀ.
// ��TCB�е������ֶζ�����ʼ��. ����, TCB state������ΪCLOSED���ͻ��˶˿ڱ�����Ϊ�������ò���client_port. 
// TCB������Ŀ��������Ӧ��Ϊ�ͻ��˵����׽���ID�������������, �����ڱ�ʶ�ͻ��˵�����. 
// ���TCB����û����Ŀ����, �����������-1.
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

// ��������������ӷ�����. �����׽���ID, �������ڵ�ID�ͷ������Ķ˿ں���Ϊ�������. �׽���ID�����ҵ�TCB��Ŀ.  
// �����������TCB�ķ������ڵ�ID�ͷ������˿ں�,  Ȼ��ʹ��sip_sendseg()����һ��SYN�θ�������.  
// �ڷ�����SYN��֮��, һ����ʱ��������. �����SYNSEG_TIMEOUTʱ��֮��û���յ�SYNACK, SYN �ν����ش�. 
// ����յ���, �ͷ���1. ����, ����ش�SYN�Ĵ�������SYN_MAX_RETRY, �ͽ�stateת����CLOSED, ������-1.
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

// �������ݸ�STCP������. �������ʹ���׽���ID�ҵ�TCB���е���Ŀ.
// Ȼ����ʹ���ṩ�����ݴ���segBuf, �������ӵ����ͻ�����������.
// ������ͻ������ڲ�������֮ǰΪ��, һ����Ϊsendbuf_timer���߳̾ͻ�����.
// ÿ��SENDBUF_ROLLING_INTERVALʱ���ѯ���ͻ������Լ���Ƿ��г�ʱ�¼�����. 
// ��������ڳɹ�ʱ����1�����򷵻�-1. 
// stcp_client_send��һ����������������.
// ��Ϊ�û����ݱ���ƬΪ�̶���С��STCP��, ����һ��stcp_client_send���ÿ��ܻ�������segBuf
// ����ӵ����ͻ�����������. ������óɹ�, ���ݾͱ�����TCB���ͻ�����������, ���ݻ������ڵ����,
// ���ݿ��ܱ����䵽������, ���ڶ����еȴ�����.
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

// ����������ڶϿ���������������. �����׽���ID��Ϊ�������. �׽���ID�����ҵ�TCB���е���Ŀ.  
// �����������FIN�θ�������. �ڷ���FIN֮��, state��ת����FINWAIT, ������һ����ʱ��.
// ��������ճ�ʱ֮ǰstateת����CLOSED, �����FINACK�ѱ��ɹ�����. ����, ����ھ���FIN_MAX_RETRY�γ���֮��,
// state��ȻΪFINWAIT, state��ת����CLOSED, ������-1.
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

// �����������free()�ͷ�TCB��Ŀ. ��������Ŀ���ΪNULL, �ɹ�ʱ(��λ����ȷ��״̬)����1,
// ʧ��ʱ(��λ�ڴ����״̬)����-1.
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

// ������stcp_client_init()�������߳�. �������������Է������Ľ����. 
// seghandler�����Ϊһ������sip_recvseg()������ѭ��. ���sip_recvseg()ʧ��, ��˵����SIP���̵������ѹر�,
// �߳̽���ֹ. ����STCP�ε���ʱ����������״̬, ���Բ�ȡ��ͬ�Ķ���. ��鿴�ͻ���FSM���˽����ϸ��.
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


//����̳߳�����ѯ���ͻ������Դ�����ʱ�¼�. ������ͻ������ǿ�, ��Ӧһֱ����.
//���(��ǰʱ�� - ��һ���ѷ��͵�δ��ȷ�϶εķ���ʱ��) > DATA_TIMEOUT, �ͷ���һ�γ�ʱ�¼�.
//����ʱ�¼�����ʱ, ���·��������ѷ��͵�δ��ȷ�϶�. �����ͻ�����Ϊ��ʱ, ����߳̽���ֹ.
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

