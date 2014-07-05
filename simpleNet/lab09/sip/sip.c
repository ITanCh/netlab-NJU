//文件名: sip/sip.c
//
//描述: 这个文件实现SIP进程  
//
//创建日期: 2013年

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../topology/topology.h"
#include "sip.h"

/**************************************************************/
//声明全局变量
/**************************************************************/
int son_conn; 		//到重叠网络的连接

/**************************************************************/
//实现SIP的函数
/**************************************************************/

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT
//成功时返回连接描述符, 否则返回-1
int connectToSON() { 
	//local IP Address
	char SERV_ADDR[15]="127.0.0.1";
	int sockfd;  
	struct sockaddr_in local_addr;
	//init
	memset(&local_addr,0,sizeof(local_addr)); 
	
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = inet_addr(SERV_ADDR);
	local_addr.sin_port = htons(SON_PORT);   
	//create socket
	if((sockfd = socket(PF_INET,SOCK_STREAM,0))<0)  
	{
		perror("son socket");  
		return -1;  
	}  
	//connect to local son_port
	if(connect(sockfd,(struct sockaddr *)&local_addr,sizeof(struct sockaddr)) < 0)  
	{
		perror("son connect");  
		return -1;  
	}
	printf("sip: CONNECT TO SON_PORT\n");
	return sockfd;
}

//这个线程每隔ROUTEUPDATE_INTERVAL时间就发送一条路由更新报文
//在本实验中, 这个线程只广播空的路由更新报文给所有邻居, 
//我们通过设置SIP报文首部中的dest_nodeID为BROADCAST_NODEID来发送广播
void* routeupdate_daemon(void* arg) {
	
	sip_pkt_t *update_sip = (sip_pkt_t*)malloc(sizeof(sip_pkt_t));
	//settings
	update_sip->header.src_nodeID = topology_getMyNodeID();;
	update_sip->header.dest_nodeID = BROADCAST_NODEID;
	update_sip->header.length = 0;
	update_sip->header.type = ROUTE_UPDATE;

	while(1)
	{
		sleep(ROUTEUPDATE_INTERVAL);
		//printf("sip: update broadcast\n");
		if(son_conn != -1 && son_sendpkt(BROADCAST_NODEID,update_sip,son_conn) > 0)
			printf("SIP send a pkt to SON\n");
		else
			son_conn = -1;;
		//printf("sip: broadcast done\n");
	}
	return 0;
}

//这个线程处理来自SON进程的进入报文
//它通过调用son_recvpkt()接收来自SON进程的报文
//在本实验中, 这个线程在接收到报文后, 只是显示报文已接收到的信息, 并不处理报文 
void* pkthandler(void* arg) {
	sip_pkt_t pkt;

	while(son_conn != -1 && son_recvpkt(&pkt,son_conn) > 0) {
		printf("Routing: received a packet from neighbor %d\n",pkt.header.src_nodeID);
		printf("sip[INFO] dest:%d length:%d type:%d\n",pkt.header.dest_nodeID, pkt.header.length, pkt.header.type);
	}
	close(son_conn);
	son_conn = -1;
	pthread_exit(NULL);
}

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数. 
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop() {
	close(son_conn);
	son_conn = -1;
	printf("sip: CLOSE SIP\n");
}

int main(int argc, char *argv[]) {
	printf("SIP layer is starting, please wait...\n");

	//初始化全局变量
	son_conn = -1;

	//注册用于终止进程的信号句柄
	signal(SIGINT, sip_stop);

	//连接到重叠网络层SON
	son_conn = connectToSON();
	if(son_conn<0) {
		printf("can't connect to SON process\n");
		exit(1);		
	}
	
	//启动线程处理来自SON进程的进入报文
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//启动路由更新线程 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("SIP layer is started...\n");

	//永久睡眠
	while(1) {
		sleep(60);
	}
}


