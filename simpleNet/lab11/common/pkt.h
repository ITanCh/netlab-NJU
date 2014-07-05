#ifndef PKT_H
#define PKT_H

#include "constants.h"

//�������Ͷ���, ���ڱ����ײ��е�type�ֶ�
#define	ROUTE_UPDATE 1
#define SIP 2	

//SIP���ĸ�ʽ����
typedef struct sipheader {
  int src_nodeID;		          //Դ�ڵ�ID
  int dest_nodeID;		          //Ŀ��ڵ�ID
  unsigned short int length;	  //���������ݵĳ���
  unsigned short int type;	      //�������� 
} sip_hdr_t;

typedef struct packet {
  sip_hdr_t header;
  char data[MAX_PKT_LEN];
} sip_pkt_t;

//·�ɸ��±��Ķ���
//����·�ɸ��±�����˵, ·�ɸ�����Ϣ�洢�ڱ��ĵ�data�ֶ���

//һ��·�ɸ�����Ŀ
typedef struct routeupdate_entry {
        unsigned int nodeID;	//Ŀ��ڵ�ID
        unsigned int cost;	    //��Դ�ڵ�(�����ײ��е�src_nodeID)��Ŀ��ڵ����·����
} routeupdate_entry_t;

//·�ɸ��±��ĸ�ʽ
typedef struct pktrt{
        unsigned int entryNum;	//���·�ɸ��±����а�������Ŀ��
        routeupdate_entry_t entry[MAX_NODE_NUM];
} pkt_routeupdate_t;

// ���ݽṹsendpkt_arg_t���ں���son_sendpkt()��. 
// son_sendpkt()��SIP���̵���, ��������Ҫ��SON���̽����ķ��͵��ص�������. 
// 
// SON���̺�SIP����ͨ��һ������TCP���ӻ���, ��son_sendpkt()��, SIP����ͨ����TCP���ӽ�������ݽṹ���͸�SON����. 
// SON����ͨ������getpktToSend()����������ݽṹ. Ȼ��SON���̵���sendpkt()�����ķ��͸���һ��.
typedef struct sendpktargument {
  int nextNodeID;        //��һ���Ľڵ�ID
  sip_pkt_t pkt;         //Ҫ���͵ı���
} sendpkt_arg_t;

// son_sendpkt()��SIP���̵���, ��������Ҫ��SON���̽����ķ��͵��ص�������. SON���̺�SIP����ͨ��һ������TCP���ӻ���.
// ��son_sendpkt()��, ���ļ�����һ���Ľڵ�ID����װ�����ݽṹsendpkt_arg_t, ��ͨ��TCP���ӷ��͸�SON����. 
// ����son_conn��SIP���̺�SON����֮���TCP�����׽���������.
// ��ͨ��SIP���̺�SON����֮���TCP���ӷ������ݽṹsendpkt_arg_tʱ, ʹ��'!&'��'!#'��Ϊ�ָ���, ����'!& sendpkt_arg_t�ṹ !#'��˳����.
// ������ͳɹ�, ����1, ���򷵻�-1.
int son_sendpkt(int nextNodeID, sip_pkt_t* pkt, int son_conn);

// son_recvpkt()������SIP���̵���, �������ǽ�������SON���̵ı���. 
// ����son_conn��SIP���̺�SON����֮��TCP���ӵ��׽���������. ����ͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ��� 
// ����ɹ����ձ���, ����1, ���򷵻�-1.
int son_recvpkt(sip_pkt_t* pkt, int son_conn);

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
int getpktToSend(sip_pkt_t* pkt, int* nextNode,int sip_conn);

// forwardpktToSIP()��������SON���̽��յ������ص����������ھӵı��ĺ󱻵��õ�. 
// SON���̵����������������ת����SIP����. 
// ����sip_conn��SIP���̺�SON����֮���TCP���ӵ��׽���������. 
// ����ͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#, ����'!& ���� !#'��˳����. 
// ������ķ��ͳɹ�, ����1, ���򷵻�-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn);

// sendpkt()������SON���̵���, �������ǽ�������SIP���̵ı��ķ��͸���һ��.
// ����conn�ǵ���һ���ڵ��TCP���ӵ��׽���������.
// ����ͨ��SON���̺����ھӽڵ�֮���TCP���ӷ���, ʹ�÷ָ���!&��!#, ����'!& ���� !#'��˳����. 
// ������ķ��ͳɹ�, ����1, ���򷵻�-1.
int sendpkt(sip_pkt_t* pkt, int conn);

// recvpkt()������SON���̵���, �������ǽ��������ص����������ھӵı���.
// ����conn�ǵ����ھӵ�TCP���ӵ��׽���������.
// ����ͨ��SON���̺����ھ�֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ��� 
// ����ɹ����ձ���, ����1, ���򷵻�-1.
int recvpkt(sip_pkt_t* pkt, int conn);

#endif
