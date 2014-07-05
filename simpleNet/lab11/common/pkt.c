// �ļ��� pkt.c
// ��������: 2013��1��

#include "pkt.h"
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>

#define PKTSTART1	0
#define PKTSTART2	1
#define PKTRECV		2
#define PKTSTOP1	3

// son_sendpkt()��SIP���̵���, ��������Ҫ��SON���̽����ķ��͵��ص�������. SON���̺�SIP����ͨ��һ������TCP���ӻ���.
// ��son_sendpkt()��, ���ļ�����һ���Ľڵ�ID����װ�����ݽṹsendpkt_arg_t, ��ͨ��TCP���ӷ��͸�SON����. 
// ����son_conn��SIP���̺�SON����֮���TCP�����׽���������.
// ��ͨ��SIP���̺�SON����֮���TCP���ӷ������ݽṹsendpkt_arg_tʱ, ʹ��'!&'��'!#'��Ϊ�ָ���, ����'!& sendpkt_arg_t�ṹ !#'��˳����.
// ������ͳɹ�, ����1, ���򷵻�-1.
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

// son_recvpkt()������SIP���̵���, �������ǽ�������SON���̵ı���. 
// ����son_conn��SIP���̺�SON����֮��TCP���ӵ��׽���������. ����ͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ��� 
// ����ɹ����ձ���, ����1, ���򷵻�-1.
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

// ���������SON���̵���, �������ǽ������ݽṹsendpkt_arg_t.
// ���ĺ���һ���Ľڵ�ID����װ��sendpkt_arg_t�ṹ.
// ����sip_conn����SIP���̺�SON����֮���TCP���ӵ��׽���������. 
// sendpkt_arg_t�ṹͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ���
// ����ɹ�����sendpkt_arg_t�ṹ, ����1, ���򷵻�-1.
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

// forwardpktToSIP()��������SON���̽��յ������ص����������ھӵı��ĺ󱻵��õ�. 
// SON���̵����������������ת����SIP����. 
// ����sip_conn��SIP���̺�SON����֮���TCP���ӵ��׽���������. 
// ����ͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#, ����'!& ���� !#'��˳����. 
// ������ķ��ͳɹ�, ����1, ���򷵻�-1.
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

// sendpkt()������SON���̵���, �������ǽ�������SIP���̵ı��ķ��͸���һ��.
// ����conn�ǵ���һ���ڵ��TCP���ӵ��׽���������.
// ����ͨ��SON���̺����ھӽڵ�֮���TCP���ӷ���, ʹ�÷ָ���!&��!#, ����'!& ���� !#'��˳����. 
// ������ķ��ͳɹ�, ����1, ���򷵻�-1.
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

// recvpkt()������SON���̵���, �������ǽ��������ص����������ھӵı���.
// ����conn�ǵ����ھӵ�TCP���ӵ��׽���������.
// ����ͨ��SON���̺����ھ�֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ��� 
// ����ɹ����ձ���, ����1, ���򷵻�-1.
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