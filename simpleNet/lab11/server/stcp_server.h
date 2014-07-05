// 文件名: stcp_server.h
//
// 描述: 这个文件包含服务器状态定义, 一些重要的数据结构和服务器STCP套接字接口定义. 你需要实现所有这些接口.
//
// 创建日期: 2013年1月
//

#ifndef STCPSERVER_H
#define STCPSERVER_H

#include <pthread.h>
#include "../common/seg.h"
#include "../common/constants.h"

//FSM中使用的服务器状态
#define	CLOSED 1
#define	LISTENING 2
#define	CONNECTED 3
#define	CLOSEWAIT 4

//在发送缓冲区链表中存储段的单元.
typedef struct segBuf {
        seg_t seg;
        unsigned int sentTime;
        struct segBuf* next;
} segBuf_t;


//服务器传输控制块. 一个STCP连接的服务器端使用这个数据结构记录连接信息.
typedef struct server_tcb {
	unsigned int server_nodeID;        //服务器节点ID, 类似IP地址
	unsigned int server_portNum;       //服务器端口号
	unsigned int client_nodeID;     //客户端节点ID, 类似IP地址
	unsigned int client_portNum;    //客户端端口号
	unsigned int state;         	//服务器状态
	unsigned int expect_seqNum;     //服务器期待的数据序号	
	char* recvBuf;                  //指向接收缓冲区的指针
	unsigned int  usedBufLen;       //接收缓冲区中已接收数据的大小
	pthread_mutex_t* bufMutex;      //指向一个互斥量的指针, 该互斥量用于对接收缓冲区的访问

	unsigned int next_seqNum;       //新段准备使用的下一个序号 
	pthread_mutex_t* sendBufMutex;      //发送缓冲区互斥量
	segBuf_t* sendBufHead;          //发送缓冲区头
	segBuf_t* sendBufunSent;        //发送缓冲区中的第一个未发送段
	segBuf_t* sendBufTail;          //发送缓冲区尾
	unsigned int unAck_segNum;      //已发送但未收到确认段的数量

} server_tcb_t;

//
//  用于服务器端应用程序的STCP套接字API. 
//  ===================================
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void stcp_server_init(int conn);

// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对TCP套接字描述符conn初始化一个STCP层的全局变量, 
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_server_sock(unsigned int server_port);

// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_server_accept();

// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后启动定时器进入忙等待直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_server_send(int sockfd,void*data,unsigned int length);
int stcp_send(int sockfd,void*data,unsigned int length);
int stcp_server_recv(int sockfd, void* buf, unsigned int length);

// 接收来自STCP客户端的数据. 请回忆STCP使用的是单向传输, 数据从客户端发送到服务器端.
// 信号/控制信息(如SYN, SYNACK等)则是双向传递. 这个函数每隔RECVBUF_POLLING_INTERVAL时间
// 就查询接收缓冲区, 直到等待的数据到达, 它然后存储数据并返回1. 如果这个函数失败, 则返回-1.
//
// 注意: stcp_server_recv在返回数据给应用程序之前, 它阻塞等待用户请求的字节数(即length)到达服务器.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_server_close(int sockfd);

// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void* seghandler(void* arg);

// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明到SIP进程的连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void * sendBuf_timer(void* servertcb);

#endif
