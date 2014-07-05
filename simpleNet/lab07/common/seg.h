//
// �ļ���: seg.h

// ����: ����ļ�����STCP�ζ���, �Լ����ڷ��ͺͽ���STCP�εĽӿ�sip_sendseg() and sip_rcvseg(), ����֧�ֺ�����ԭ��. 
//
// ��������: 2013��
//

#ifndef SEG_H
#define SEG_H

#include "constants.h"

//�����Ͷ���, ����STCP.
#define	SYN 0
#define	SYNACK 1
#define	FIN 2
#define	FINACK 3
#define	DATA 4
#define	DATAACK 5

#define SEGSTART1 0
#define SEGSTART2 1
#define SEGRECV	2
#define SEGSTOP1 4
//���ײ����� 

typedef struct stcp_hdr {
	unsigned int src_port;        //Դ�˿ں�
	unsigned int dest_port;       //Ŀ�Ķ˿ں�
	unsigned int seq_num;         //���
	unsigned int ack_num;         //ȷ�Ϻ�
	unsigned short int length;    //�����ݳ���
	unsigned short int  type;     //������
	unsigned short int  rcv_win;  //��ʵ��δʹ��
	unsigned short int checksum;  //����ε�У���,��ʵ��δʹ��
} stcp_hdr_t;

//�ζ���

typedef struct segment {
	stcp_hdr_t header;
	char data[MAX_SEG_LEN];
} seg_t;

//
//  �ͻ��˺ͷ�������SIP API 
//  =======================================
//
//  �����������ṩ��ÿ���������õ�ԭ�Ͷ����ϸ��˵��, ����Щֻ��ָ���Ե�, ����ȫ���Ը����Լ����뷨����ƴ���.
//
//  ע��: sip_sendseg()��sip_recvseg()����������ṩ�ķ���, ��SIP�ṩ��STCP.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int sip_sendseg(int connection, seg_t* segPtr);

// ͨ���ص�����(�ڱ�ʵ���У���һ��TCP����)����STCP��. ��ΪTCP���ֽ�����ʽ��������, 
// Ϊ��ͨ���ص�����TCP���ӷ���STCP��, ����Ҫ�ڴ���STCP��ʱ�������Ŀ�ͷ�ͽ�β���Ϸָ���. 
// �����ȷ��ͱ���һ���ο�ʼ�������ַ�"!&"; Ȼ����seg_t; ����ͱ���һ���ν����������ַ�"!#".  
// �ɹ�ʱ����1, ʧ��ʱ����-1. sip_sendseg()����ʹ��send()���������ַ�, Ȼ��ʹ��send()����seg_t,
// ���ʹ��send()���ͱ����ν����������ַ�.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int sip_recvseg(int connection, seg_t* segPtr);

// ͨ���ص�����(�ڱ�ʵ���У���һ��TCP����)����STCP��. ���ǽ�����ʹ��recv()һ�ν���һ���ֽ�.
// ����Ҫ����"!&", Ȼ����seg_t, �����"!#". ��ʵ������Ҫ��ʵ��һ��������FSM, ���Կ���ʹ��������ʾ��FSM.
// SEGSTART1 -- ��� 
// SEGSTART2 -- ���յ�'!', �ڴ�'&' 
// SEGRECV -- ���յ�'&', ��ʼ��������
// SEGSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ���
// ����ļ�����"!&"��"!#"��������ڶε����ݲ���(��Ȼ�൱����, ��ʵ�ֻ�򵥺ܶ�).
// ��Ӧ�����ַ��ķ�ʽһ�ζ�ȡһ���ֽ�, �����ݲ��ֿ������������з��ظ�������.
//
// ע��: ����һ�ִ���ʽ��������"!&"��"!#"�����ڶ��ײ���ε����ݲ���. ���崦��ʽ������ȷ����ȡ��!&��Ȼ��
// ֱ�Ӷ�ȡ������STCP���ײ�, ���������е������ַ�, Ȼ�����ײ��еĳ��ȶ�ȡ������, ���ȷ����!#��β.
//
// ע��: ����������һ��STCP��֮��,  ����Ҫ����seglost()��ģ�����������ݰ��Ķ�ʧ. 
// ������seglost()�Ĵ���.
// 
// ����ζ�ʧ��, �ͷ���1, ���򷵻�0. 
int seglost(); 
// seglost��Դ����
// ����������seg.c��
/*
int seglost() {
	int random = rand()%100;
	if(random<PKT_LOSS_RATE*100) {
		return 1;
	else 
		return 0;
}
*/
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

#endif
