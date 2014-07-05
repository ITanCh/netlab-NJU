// 文件名 pkt.c
// 创建日期: 2013年1月

#include "pkt.h"
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>

#define PKTSTART1	0
#define PKTSTART2	1
#define PKTRECV		2
#define PKTSTOP1	3

// son_sendpkt()由SIP进程调用, 其作用是要求SON进程将报文发送到重叠网络中. SON进程和SIP进程通过一个本地TCP连接互连.
// 在son_sendpkt()中, 报文及其下一跳的节点ID被封装进数据结构sendpkt_arg_t, 并通过TCP连接发送给SON进程. 
// 参数son_conn是SIP进程和SON进程之间的TCP连接套接字描述符.
// 当通过SIP进程和SON进程之间的TCP连接发送数据结构sendpkt_arg_t时, 使用'!&'和'!#'作为分隔符, 按照'!& sendpkt_arg_t结构 !#'的顺序发送.
// 如果发送成功, 返回1, 否则返回-1.
int son_sendpkt(int nextNodeID, sip_pkt_t* pkt, int son_conn)
{
  int i;
	char flag[2];
	flag[0]='!';
	flag[1]='&';
	i=send(son_conn,flag,sizeof(flag),MSG_NOSIGNAL);
	if(i<0)return -1;
	sendpkt_arg_t packet;
	packet.nextNodeID = nextNodeID;
	int length = pkt->header.length + sizeof(sip_hdr_t);
	memcpy(&(packet.pkt), pkt, length);
	i=send(son_conn,&packet,sizeof(sendpkt_arg_t),MSG_NOSIGNAL);
	if(i<0)return -1;
	flag[0]='!';
	flag[1]='#';
	i=send(son_conn,flag,sizeof(flag),MSG_NOSIGNAL);
	if(i<0)return -1;
	return 1;
}

// son_recvpkt()函数由SIP进程调用, 其作用是接收来自SON进程的报文. 
// 参数son_conn是SIP进程和SON进程之间TCP连接的套接字描述符. 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int son_recvpkt(sip_pkt_t* pkt, int son_conn)
{
  int state=PKTSTART1;
	int len=sizeof(sip_pkt_t);
	int i=0;
	char *buf = (char *)malloc(sizeof(sip_pkt_t));
	char c;
	memset(buf,0,len);
	int flag=1;
	while(flag){
		if(recv(son_conn,&c,1,0) <= 0)
			return -1;
		//printf("sip recv c:%c\n",c);
		switch(state)
		{
			case PKTSTART1:if(c=='!')state=PKTSTART2;
							   break;
			case PKTSTART2:if(c=='&')state=PKTRECV;
							   else 
								   state=PKTSTART1;
							   break;
			case PKTRECV:if(c=='!')state=PKTSTOP1;
							 else 
							 {
								 if(i<len)
								 {
									buf[i]=c;
									i++;
								 }
							 }
							 break;
			case PKTSTOP1:if(c=='#'){
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
								  state=PKTRECV;
							  }
						  }
						  break;
		}
	}
	if(flag==0)
	{
		memcpy(pkt, buf, len);
		return 1;
	}
	else 
		return -1;
}

// 这个函数由SON进程调用, 其作用是接收数据结构sendpkt_arg_t.
// 报文和下一跳的节点ID被封装进sendpkt_arg_t结构.
// 参数sip_conn是在SIP进程和SON进程之间的TCP连接的套接字描述符. 
// sendpkt_arg_t结构通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 如果成功接收sendpkt_arg_t结构, 返回1, 否则返回-1.
int getpktToSend(sip_pkt_t* pkt, int* nextNode,int sip_conn)
{
  int state=PKTSTART1;
	int len=sizeof(sendpkt_arg_t);
	int i=0;
	char *buf = (char *)malloc(sizeof(sendpkt_arg_t));
	char c;
	memset(buf,0,len);
	int flag=1;
	while(flag){
		if(recv(sip_conn,&c,1,0) <= 0)
			return -1;
		//printf("sip recv c:%c\n",c);
		switch(state)
		{
			case PKTSTART1:if(c=='!')state=PKTSTART2;
							   break;
			case PKTSTART2:if(c=='&')state=PKTRECV;
							   else 
								   state=PKTSTART1;
							   break;
			case PKTRECV:if(c=='!')state=PKTSTOP1;
							 else 
							 {
								 if(i<len)
								 {
									buf[i]=c;
									i++;
								 }
							 }
							 break;
			case PKTSTOP1:if(c=='#'){
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
								  state=PKTRECV;
							  }
						  }
						  break;
		}
	}
	if(flag==0)
	{
		*nextNode = ((sendpkt_arg_t *)buf)->nextNodeID;
		memcpy(pkt, &(((sendpkt_arg_t *)buf)->pkt), sizeof(sip_pkt_t));
		return 1;
	}
	else 
		return -1;
}

// forwardpktToSIP()函数是在SON进程接收到来自重叠网络中其邻居的报文后被调用的. 
// SON进程调用这个函数将报文转发给SIP进程. 
// 参数sip_conn是SIP进程和SON进程之间的TCP连接的套接字描述符. 
// 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn)
{
  int i;
	char flag[2];
	flag[0]='!';
	flag[1]='&';
	i=send(sip_conn,flag,sizeof(flag),MSG_NOSIGNAL);
	if(i<0)return -1;
	//sendpkt_arg_t packet;
	//packet.nextNodeID = nextNodeID;
	//int length = pkt->header.length + sizeof(sip_hdr_t);
	//memcpy(&(packet.pkt), pkt, length);
	i=send(sip_conn,pkt,sizeof(sip_pkt_t),MSG_NOSIGNAL);
	if(i<0)return -1;
	flag[0]='!';
	flag[1]='#';
	i=send(sip_conn,flag,sizeof(flag),MSG_NOSIGNAL);
	if(i<0)return -1;
	return 1;
}

// sendpkt()函数由SON进程调用, 其作用是将接收自SIP进程的报文发送给下一跳.
// 参数conn是到下一跳节点的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居节点之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int sendpkt(sip_pkt_t* pkt, int conn)
{
  int i;
	char flag[2];
	flag[0]='!';
	flag[1]='&';
	i=send(conn,flag,sizeof(flag),MSG_NOSIGNAL);
	if(i<0)return -1;
	//sendpkt_arg_t packet;
	//packet.nextNodeID = nextNodeID;
	//int length = pkt->header.length + sizeof(sip_hdr_t);
	//memcpy(&(packet.pkt), pkt, length);
	i=send(conn,pkt,sizeof(sip_pkt_t),MSG_NOSIGNAL);
	if(i<0)return -1;
	flag[0]='!';
	flag[1]='#';
	i=send(conn,flag,sizeof(flag),MSG_NOSIGNAL);
	if(i<0)return -1;
	return 1;
}

// recvpkt()函数由SON进程调用, 其作用是接收来自重叠网络中其邻居的报文.
// 参数conn是到其邻居的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int recvpkt(sip_pkt_t* pkt, int conn)
{
  int state=PKTSTART1;
	int len=sizeof(sip_pkt_t);
	int i=0;
	char *buf = (char *)malloc(sizeof(sip_pkt_t));
	char c;
	memset(buf,0,len);
	int flag=1;
	while(flag){
		if(recv(conn,&c,1,0) <= 0)
			return -1;
		//printf("sip recv c:%c\n",c);
		switch(state)
		{
			case PKTSTART1:if(c=='!')state=PKTSTART2;
							   break;
			case PKTSTART2:if(c=='&')state=PKTRECV;
							   else 
								   state=PKTSTART1;
							   break;
			case PKTRECV:if(c=='!')state=PKTSTOP1;
							 else 
							 {
								 if(i<len)
								 {
									buf[i]=c;
									i++;
								 }
							 }
							 break;
			case PKTSTOP1:if(c=='#'){
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
								  state=PKTRECV;
							  }
						  }
						  break;
		}
	}
	if(flag==0)
	{
		memcpy(pkt, buf, len);
		return 1;
	}
	else 
		return -1;
}
