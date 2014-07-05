//文件名: sip/sip.h
//
//描述: 这个文件定义用于SIP进程的数据结构和函数  
//
//创建日期: 2013年

#ifndef NETWORK_H
#define NETWORK_H

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT
//成功时返回连接描述符, 否则返回-1
int connectToSON();

//这个线程每隔ROUTEUPDATE_INTERVAL时间就发送一条路由更新报文
//在本实验中, 这个线程只广播空的路由更新报文给所有邻居, 
//我们通过设置SIP报文首部中的dest_nodeID为BROADCAST_NODEID来发送广播
void* routeupdate_daemon(void* arg);

//这个线程处理来自SON进程的进入报文
//它通过调用son_recvpkt()接收来自SON进程的报文
//在本实验中, 这个线程在接收到报文后, 只是显示报文已接收到的信息, 并不处理报文
void* pkthandler(void* arg); 

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数 
//它关闭所有连接, 释放所有动态分配的内存
void sip_stop();
#endif
