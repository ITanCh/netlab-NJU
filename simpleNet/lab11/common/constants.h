//文件名: common/constants.h

//描述: 这个文件包含常量

//创建日期: 2013年1月

#ifndef CONSTANTS_H
#define CONSTANTS_H

/*******************************************************************/
//传输层参数
/*******************************************************************/

//这是STCP可以支持的最大连接数. 你的TCB表应包含MAX_TRANSPORT_CONNECTIONS个条目.
#define MAX_TRANSPORT_CONNECTIONS 10
#define MAX_SEQNUM 100000
//最大段长度
//MAX_SEG_LEN = 1500 - sizeof(seg header) - sizeof(ip header)
#define MAX_SEG_LEN  1464
//数据包丢失率为10%
#define PKT_LOSS_RATE 0.1
//SYN_TIMEOUT值, 单位为纳秒
#define SYN_TIMEOUT 500000000
//FIN_TIMEOUT值, 单位为纳秒
#define FIN_TIMEOUT 500000000
//stcp_client_connect()中的最大SYN重传次数
#define SYN_MAX_RETRY 10
//stcp_client_disconnect()中的最大FIN重传次数
#define FIN_MAX_RETRY 5
//服务器CLOSEWAIT超时值, 单位为秒
#define CLOSEWAIT_TIMEOUT 5
//sendBuf_timer线程的轮询间隔, 单位为纳秒
#define SENDBUF_POLLING_INTERVAL 500000000
//STCP客户端在stcp_server_recv()函数中使用这个时间间隔来轮询接收缓冲区, 以检查是否请求的数据已全部到达, 单位为秒.
#define RECVBUF_POLLING_INTERVAL 1
//stcp_server_accept()函数使用这个时间间隔来忙等待TCB状态转换, 单位为纳秒
#define ACCEPT_POLLING_INTERVAL 500000000
//接收缓冲区大小
#define RECEIVE_BUF_SIZE 1000000
//数据段超时值, 单位为纳秒
#define DATA_TIMEOUT 500000000
//GBN窗口大小
#define GBN_WINDOW 10

/*******************************************************************/
//SON参数
/*******************************************************************/

//这个端口号用于重叠网络中节点之间的互联, 你应该修改它为一个随机值以避免和其他同学的设置发生冲突
#define CONNECTION_PORT 3176

//这个端口号由SON进程打开, 并由SIP进程连接, 你应该修改它为一个随机值以避免和其他同学的设置发生冲突.
#define SON_PORT 3177

//最大SIP报文数据长度: 1500 - sizeof(sip header)
#define MAX_PKT_LEN 1488 

/*******************************************************************/
//SIP参数
/*******************************************************************/
//重叠网络支持的最大节点数  
#define MAX_NODE_NUM 10

//最大路由表槽数 
#define MAX_ROUTINGTABLE_SLOTS 10

//无穷大的链路代价值, 如果两个节点断开连接了, 它们之间的链路代价值就是INFINITE_COST
#define INFINITE_COST 999

//SIP进程打开这个端口并等待来自STCP进程的连接, 你应该修改它为一个随机值以避免和其他同学的设置发生冲突.
#define SIP_PORT 4522

//这是广播节点ID. 
#define BROADCAST_NODEID 9999

//路由更新广播间隔, 以秒为单位
#define ROUTEUPDATE_INTERVAL 5
#endif
