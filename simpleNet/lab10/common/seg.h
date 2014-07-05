//
// 文件名: seg.h

// 描述: 这个文件包含STCP段定义, 以及用于发送和接收STCP段的接口sip_sendseg() and sip_rcvseg(), 及其支持函数的原型. 
//
// 创建日期: 2013年1月
//

#ifndef SEG_H
#define SEG_H

#include "constants.h"

//段类型定义, 用于STCP.
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

//段首部定义. 

typedef struct stcp_hdr {
	unsigned int src_port;        //源端口号
	unsigned int dest_port;       //目的端口号
	unsigned int seq_num;         //序号
	unsigned int ack_num;         //确认号
	unsigned short int length;    //段数据长度
	unsigned short int  type;     //段类型
	unsigned short int  rcv_win;  //当前未使用
	unsigned short int checksum;  //这个段的校验和
} stcp_hdr_t;

//段定义

typedef struct segment {
	stcp_hdr_t header;
	char data[MAX_SEG_LEN];
} seg_t;

//这是在SIP进程和STCP进程之间交换的数据结构.
//它包含一个节点ID和一个段. 
//对sip_sendseg()来说, 节点ID是段的目标节点ID.
//对sip_recvseg()来说, 节点ID是段的源节点ID.
typedef struct sendsegargument {
	int nodeID;		//节点ID 
	seg_t seg;		//一个段 
} sendseg_arg_t;

//STCP进程使用这个函数发送sendseg_arg_t结构(包含段及其目的节点ID)给SIP进程.
//参数sip_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t发送成功,就返回1,否则返回-1.
int sip_sendseg(int sip_conn, int dest_nodeID, seg_t* segPtr);

//STCP进程使用这个函数来接收来自SIP进程的包含段及其源节点ID的sendseg_arg_t结构.
//参数sip_conn是STCP进程和SIP进程之间连接的TCP描述符.
//当接收到段时, 使用seglost()来判断该段是否应被丢弃并检查校验和.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int sip_recvseg(int sip_conn, int* src_nodeID, seg_t* segPtr);

//SIP进程使用这个函数接收来自STCP进程的包含段及其目的节点ID的sendseg_arg_t结构.
//参数stcp_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr); 

//SIP进程使用这个函数发送包含段及其源节点ID的sendseg_arg_t结构给STCP进程.
//参数stcp_conn是STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t被成功发送就返回1, 否则返回-1.
int forwardsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr); 

// 一个段有PKT_LOST_RATE/2的可能性丢失, 或PKT_LOST_RATE/2的可能性有着错误的校验和.
// 如果数据包丢失了, 就返回1, 否则返回0. 
// 即使段没有丢失, 它也有PKT_LOST_RATE/2的可能性有着错误的校验和.
// 我们在段中反转一个随机比特来创建错误的校验和.
int seglost(seg_t* segPtr); 

//这个函数计算指定段的校验和.
//校验和计算覆盖段首部和段数据. 你应该首先将段首部中的校验和字段清零, 
//如果数据长度为奇数, 添加一个全零的字节来计算校验和.
//校验和计算使用1的补码.
unsigned short checksum(seg_t* segment);

//这个函数检查段中的校验和, 正确时返回1, 错误时返回-1.
int checkchecksum(seg_t* segment);

#endif
