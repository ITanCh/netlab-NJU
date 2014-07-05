//文件名: son/son.c
//
//描述: 这个文件实现SON进程 
//SON进程首先连接到所有邻居, 然后启动listen_to_neighbor线程, 每个该线程持续接收来自一个邻居的进入报文, 并将该报文转发给SIP进程. 
//然后SON进程等待来自SIP进程的连接. 在与SIP进程建立连接之后, SON进程持续接收来自SIP进程的sendpkt_arg_t结构, 并将接收到的报文发送到重叠网络中. 
//
//创建日期: 2013年

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "son.h"
#include "../topology/topology.h"
#include "neighbortable.h"

//你应该在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 60

/**************************************************************/
//声明全局变量
/**************************************************************/

//将邻居表声明为一个全局变量 
nbr_entry_t* nt; 
//将与SIP进程之间的TCP连接声明为一个全局变量
int sip_conn; 

/**************************************************************/
//实现重叠网络函数
/**************************************************************/

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程终止. 
void* waitNbrs(void* arg) {
	//你需要编写这里的代码.
	int sockfd,newsockfd,clilen;
	struct sockaddr_in serv_addr, cli_addr;
	/* First call to socket() function */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
	{
		perror("ERROR opening socket\n");
		exit(0);
	}
	int on = 1;	//port可以立即重新使用
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(CONNECTION_PORT);

	/* Now bind the host address using bind() call.*/
	if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
	{
		perror("ERROR on binding");
		exit(2) ;
	}
	/* Now start listening for the clients, here 
	   process will go in sleep mode and will wait 
	   for the incoming connection   */
	int bigCount=getBigCount();
	listen(sockfd,bigCount);
	clilen = sizeof(cli_addr);
	printf("son start to connect greate neighbor\n");
	int i=0;
	for(;i<bigCount;i++)
	{
		memset(&cli_addr,0,sizeof(cli_addr));
		newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr,(socklen_t * restrict)&clilen);
		if (newsockfd < 0)
		{
			perror("ERROR on accept");
			exit(2);
		}
		printf("new client linked in\n");
		struct in_addr* nbIp = &(cli_addr.sin_addr);	
		int nbId=topology_getNodeIDfromip(nbIp);
		if(nt_addconn(nt, nbId, newsockfd) == -1)
			printf("cannot add sock to new neighbor\n");
		printf("neighbor id:%d connected me\n",nbId);
	}
	close(sockfd);
	//exit(0);
	pthread_exit(NULL);
}

// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs() {
	//你需要编写这里的代码.
	int nbCount=topology_getNbrNum();
	int i=0;
	int myId=topology_getMyNodeID();
	for(;i<nbCount;i++)
	{
		if(nt[i].nodeID<myId)
		{
			int client_sockfd;  
			struct sockaddr_in remote_addr; //服务器端网络地址结构体  f
			memset(&remote_addr,0,sizeof(remote_addr)); //数据初始化--清零  
			remote_addr.sin_family=AF_INET; //设置为IP通信  
			remote_addr.sin_addr.s_addr=nt[i].nodeIP;//服务器IP114.212.191.33
			//地址从转包TCP中获得 
			remote_addr.sin_port=htons(CONNECTION_PORT); //服务器端口号  
			/*创建客户端套接字--IPv4协议，面向连接通信，TCP协议*/  
			if((client_sockfd=socket(PF_INET,SOCK_STREAM,0))<0)  
			{
				perror("connectNbr socket\n");  
				return -1;  
			}  

			/*将套接字绑定到服务器的网络地址上*/  
			if(connect(client_sockfd,(struct sockaddr *)&remote_addr,sizeof(struct sockaddr))<0)  
			{
				perror("connectNbr connect\n");  
				return -1;  
			}  
			nt[i].conn=client_sockfd;
			printf("I connect to neighbor ID:%d\n",nt[i].nodeID);
		}
	}
	return 1;
}

//每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
//所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的. 
void* listen_to_neighbor(void* arg) {
	int i=*(int *)arg;
	sip_pkt_t * recvbuf=malloc(sizeof(sip_pkt_t));	
	while(1)
	{
		memset(recvbuf,0,sizeof(sip_pkt_t));
		if(recvpkt(recvbuf,nt[i].conn) > 0)
		{
			printf("recv a pkt from id:%d \n",nt[i].nodeID);
		}
		else
		{
			printf("recv from id:%d error\n",nt[i].nodeID);
			nt[i].conn = -1;
			return NULL;
		}
		if(sip_conn != -1)
		{
			if(forwardpktToSIP(recvbuf,sip_conn) > 0)
			{
				printf("forward pkt to sip from id:%d\n",nt[i].nodeID);
			}
			else
			{
				printf("forward pkt to sip from id:%d error\n",nt[i].nodeID);
				sip_conn = -1;
				return NULL;
			}
		}
	}
}

//这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接. 
//在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳. 
//如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP() {
	//你需要编写这里的代码.
	int sockfd,newsockfd,clilen;
	struct sockaddr_in serv_addr, cli_addr;
	/* First call to socket() function */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
	{
		perror("waitSIP ERROR opening socket\n");
		return;
	}
	int on = 1;	//port可以立即重新使用
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(SON_PORT);

	/* Now bind the host address using bind() call.*/
	if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
	{
		perror("waitSIP ERROR on binding\n");
		return;
	}
	/* Now start listening for the clients, here 
	   process will go in sleep mode and will wait 
	   for the incoming connection   */
	listen(sockfd,1);
	clilen = sizeof(cli_addr);
	printf("son start to wait sip to connect\n");
	memset(&cli_addr,0,sizeof(cli_addr));
	newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, (socklen_t * restrict)&clilen);
	//close(sockfd);
	/*if (newsockfd < 0)
	{
		perror("waitSIP ERROR on accept\n");
		return;
	}*/
	sip_conn=newsockfd;
	printf("sip %d link in..\n",newsockfd);
	sip_pkt_t * buf=malloc(sizeof(sip_pkt_t));
	int nextNode = 0;
	while(getpktToSend(buf,&nextNode,sip_conn) > 0)
	{
		printf("get pkt from sip\n");
		int nbCount=topology_getNbrNum();
		if(nextNode == BROADCAST_NODEID)
		{
			int i=0;
			for(;i<nbCount;i++)
			{
				if(nt[i].conn > 0 && sendpkt(buf,nt[i].conn) > 0)
					printf("send pkt to id:%d \n",nt[i].nodeID);
				else
				{
					printf("send pkt to id:%d error\n",nt[i].nodeID);
					nt[i].conn = -1;
				}
			}
		}
		else{
			int nextConn=-1;
			int i=0;
			for(;i<nbCount;i++)
			{
				if(nt[i].nodeID == nextNode)
				{
					nextConn = nt[i].conn;
					break;
				}
			}
			if(nextConn != -1 && sendpkt(buf,nextConn) > 0)
				printf("send pkt to id:%d \n",nt[i].nodeID);
			else
				printf("send pkt to id:%d error\n",nt[i].nodeID);
		}
	}
	sip_conn = -1;
}

//这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
//它关闭所有的连接, 释放所有动态分配的内存.
void son_stop() {
	//你需要编写这里的代码.
	close(sip_conn);
	sip_conn = -1;
	nt_destroy(nt);
}

int main() {
	//启动重叠网络初始化工作
	printf("Overlay network: Node %d initializing...\n",topology_getMyNodeID());	

	//创建一个邻居表
	nt = nt_create();
	//将sip_conn初始化为-1, 即还未与SIP进程连接
	sip_conn = -1;
	
	//注册一个信号句柄, 用于终止进程
	signal(SIGINT, son_stop);

	//打印所有邻居
	int nbrNum = topology_getNbrNum();
	int i;
	for(i=0;i<nbrNum;i++) {
		printf("Overlay network: neighbor %d:%d\n",i+1,nt[i].nodeID);
	}

	//启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread,NULL,waitNbrs,(void*)0);

	//等待其他节点启动
	sleep(SON_START_DELAY);
	
	//连接到节点ID比自己小的所有邻居
	connectNbrs();

	//等待waitNbrs线程返回
	pthread_join(waitNbrs_thread,NULL);	

	//此时, 所有与邻居之间的连接都建立好了
	
	//创建线程监听所有邻居
	for(i=0;i<nbrNum;i++) {
		int* idx = (int*)malloc(sizeof(int));
		*idx = i;
		pthread_t nbr_listen_thread;
		pthread_create(&nbr_listen_thread,NULL,listen_to_neighbor,(void*)idx);
	}
	printf("Overlay network: node initialized...\n");
	printf("Overlay network: waiting for connection from SIP process...\n");

	//等待来自SIP进程的连接
	waitSIP();
}
