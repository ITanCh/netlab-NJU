
#include "seg.h"
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>                                                                                                                                     

//STCP进程使用这个函数发送sendseg_arg_t结构(包含段及其目的节点ID)给SIP进程.
//参数sip_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t发送成功,就返回1,否则返回-1.
int sip_sendseg(int sip_conn, int dest_nodeID, seg_t* segPtr)
{
  	int i;
	char flag[2];
	flag[0]='!';
	flag[1]='&';
	i=send(sip_conn,flag,sizeof(flag),MSG_NOSIGNAL);
	if(i<0)return -1;
	//char buf[sizeof(seg_t)];
	sendseg_arg_t packet;
	packet.nodeID = dest_nodeID;
	memcpy(&(packet.seg), segPtr, segPtr->header.length + sizeof(stcp_hdr_t));
	i=send(sip_conn, &packet, sizeof(sendseg_arg_t), MSG_NOSIGNAL);
	if(i<0)return -1;
	//printf("send seg %d, seq :%d ,ack :%d\n",segPtr->header.dest_port,segPtr->header.seq_num,segPtr->header.ack_num);
	flag[0]='!';
	flag[1]='#';
	i=send(sip_conn,flag,sizeof(flag),MSG_NOSIGNAL);
	if(i<0)return -1;
	return 1;
}

//STCP进程使用这个函数来接收来自SIP进程的包含段及其源节点ID的sendseg_arg_t结构.
//参数sip_conn是STCP进程和SIP进程之间连接的TCP描述符.
//当接收到段时, 使用seglost()来判断该段是否应被丢弃并检查校验和.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int sip_recvseg(int sip_conn, int* src_nodeID, seg_t* segPtr)
{
  	int state=SEGSTART1;
	int len=sizeof(sendseg_arg_t);
	int i=0;
	char buf[len];
	char c;
	memset(buf,0,len);
	int flag=1;
	while(flag){
		if(recv(sip_conn,&c,1,0)<=0)
			return -1;
		//printf("sip recv c:%c\n",c);
		switch(state)
		{
			case SEGSTART1:if(c=='!')state=SEGSTART2;
							   break;
			case SEGSTART2:if(c=='&')state=SEGRECV;
							   else if(c=='!')break;
							   else 
								   state=SEGSTART1;
							   break;
			case SEGRECV:if(c=='!')state=SEGSTOP1;
							 else 
							 {
								 if(i<len)
								 {
									 buf[i]=c;
									 i++;
								 }
							 }
							 break;
			case SEGSTOP1:if(c=='#'){
							  flag = 0;
						  }
						  else if(c=='!'){
							  buf[i]='!';
							  i++;
						  }
						  else
						  {
							  if(strlen(buf)+1<len){
								  buf[i]='!';
								  i++;
								  buf[i]=c;
								  i++;
								  state=SEGRECV;
							  }
						  }
						  break;
		}
	}
	*src_nodeID = ((sendseg_arg_t *)buf)->nodeID;
	memcpy(segPtr, &(((sendseg_arg_t *)buf)->seg), sizeof(seg_t));
	int sl=0;	//seglost(segPtr);
	if(sl==0&&flag==0)
	{
		//printf("recv seg %d, seq :%d ,ack :%d\n",segPtr->header.dest_port,segPtr->header.seq_num,segPtr->header.ack_num);		
		return 1;
	}
	else
		return -1;
}

//SIP进程使用这个函数接收来自STCP进程的包含段及其目的节点ID的sendseg_arg_t结构.
//参数stcp_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr)
{
  	int state=SEGSTART1;
	int len=sizeof(sendseg_arg_t);
	int i=0;
	char buf[len];
	char c;
	memset(buf,0,len);
	int flag=1;
	while(flag){
		if(recv(stcp_conn,&c,1,0)<=0)
			return -1;
		//printf("sip recv c:%c\n",c);
		switch(state)
		{
			case SEGSTART1:if(c=='!')state=SEGSTART2;
							   break;
			case SEGSTART2:if(c=='&')state=SEGRECV;
							   else if(c=='!')break;
							   else 
								   state=SEGSTART1;
							   break;
			case SEGRECV:if(c=='!')state=SEGSTOP1;
							 else 
							 {
								 if(i<len)
								 {
									 buf[i]=c;
									 i++;
								 }
							 }
							 break;
			case SEGSTOP1:if(c=='#'){
							  flag = 0;
						  }
						  else if(c=='!'){
							  buf[i]='!';
							  i++;
						  }
						  else
						  {
							  if(strlen(buf)+1<len){
								  buf[i]='!';
								  i++;
								  buf[i]=c;
								  i++;
								  state=SEGRECV;
							  }
						  }
						  break;
		}
	}
	//int sl=	seglost(segPtr);
	if(flag==0)
	{
		*dest_nodeID = ((sendseg_arg_t *)buf)->nodeID;
		memcpy(segPtr, &(((sendseg_arg_t *)buf)->seg), sizeof(seg_t));
		return 1;
	}
	else
		return -1;
}

//SIP进程使用这个函数发送包含段及其源节点ID的sendseg_arg_t结构给STCP进程.
//参数stcp_conn是STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t被成功发送就返回1, 否则返回-1.
int forwardsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr)
{
  	int i;
	char flag[2];
	flag[0]='!';
	flag[1]='&';
	i=send(stcp_conn,flag,sizeof(flag),MSG_NOSIGNAL);
	if(i<0)return -1;
	//char buf[sizeof(seg_t)];
	sendseg_arg_t packet;
	packet.nodeID = src_nodeID;
	memcpy(&(packet.seg), segPtr, segPtr->header.length + sizeof(stcp_hdr_t));
	i=send(stcp_conn, &packet, sizeof(sendseg_arg_t), MSG_NOSIGNAL);
	if(i<0)return -1;
	flag[0]='!';
	flag[1]='#';
	i=send(stcp_conn,flag,sizeof(flag),MSG_NOSIGNAL);
	if(i<0)return -1;
	return 1;
}

// 一个段有PKT_LOST_RATE/2的可能性丢失, 或PKT_LOST_RATE/2的可能性有着错误的校验和.
// 如果数据包丢失了, 就返回1, 否则返回0. 
// 即使段没有丢失, 它也有PKT_LOST_RATE/2的可能性有着错误的校验和.
// 我们在段中反转一个随机比特来创建错误的校验和.
int seglost(seg_t* segPtr)
{
  	int random = rand()%100;
	if(random<PKT_LOSS_RATE*100) {
		//50%可能性丢失段
		if(rand()%2==0) {
			printf("son: seg lost!\n");
			return 1;
		}
		//50%可能性是错误的校验和
		else {
			//获取数据长度
			int len = sizeof(stcp_hdr_t)+segPtr->header.length;
			//获取要反转的随机位
			int errorbit = rand()%(len*8);
			//反转该比特
			char* temp = (char*)segPtr;
			temp = temp + errorbit/8;
			*temp = *temp^(1<<(errorbit%8));
			return 0;
		}
	}
	return 0;
}

//这个函数计算指定段的校验和.
//校验和计算覆盖段首部和段数据. 你应该首先将段首部中的校验和字段清零, 
//如果数据长度为奇数, 添加一个全零的字节来计算校验和.
//校验和计算使用1的补码.
unsigned short checksum(seg_t* segment)
{
  	unsigned short *buffer = (unsigned short *)segment;
	int size = segment->header.length + 24;
	unsigned long cksum = 0;
	while (size > 1) {
		cksum += *buffer++;
		size -= sizeof(unsigned short);
	}
	if (size) {
		//假如还有剩余的字节，也加上
		cksum += *(unsigned char*)buffer * 256;
	}
	while(cksum >> 16) {
		//反复将cksum高位和低位相加，直到高位为0
		cksum = (cksum & 0xffff) + (cksum >> 16);
	}
	return (unsigned short)(~cksum);
}

//这个函数检查段中的校验和, 正确时返回1, 错误时返回-1.
int checkchecksum(seg_t* segment)
{
  	unsigned short result = checksum(segment);
	//printf("son :checkchecksum result:%d\n",result);
	if(result == 0) {
		return 1;
	}
	else {
		return -1;
	}
}
