//文件名: client/app_stress_client.c
//
//描述: 这是压力测试版本的客户端程序代码. 客户端首先连接到本地SIP进程, 然后它调用stcp_client_init()初始化STCP客户端. 
//它通过调用stcp_client_sock()和stcp_client_connect()创建套接字并连接到服务器.
//然后它读取文件sendthis.txt中的文本数据, 将文件的长度和文件数据发送给服务器. 经过一段时候后, 客户端调用stcp_client_disconnect()断开到服务器的连接.
//最后,客户端调用stcp_client_close()关闭套接字并断开到本地SIP进程的连接.

//创建日期: 2013年1月

//输入: 无

//输出: STCP客户端状态

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../common/constants.h"
#include "../topology/topology.h"
#include "stcp_client.h"

//创建一个连接, 使用客户端端口号87和服务器端口号88. 
#define CLIENTPORT1 87
#define SERVERPORT1 88

//在连接到SIP进程后, 等待1秒, 让服务器启动.
#define STARTDELAY 1
//在发送文件后, 等待5秒, 然后关闭连接.
#define WAITTIME 5

//这个函数连接到本地SIP进程的端口SIP_PORT. 如果TCP连接失败, 返回-1. 连接成功, 返回TCP套接字描述符, STCP将使用该描述符发送段.
int connectToSIP() {
	//connect to local sip
	char SERV_ADDR[15]="127.0.0.1";
	int sockfd;  
	struct sockaddr_in remote_addr; //服务器端网络地址结构体  
	memset(&remote_addr,0,sizeof(remote_addr)); //数据初始化--清零  
	remote_addr.sin_family=AF_INET; //设置为IP通信  
	remote_addr.sin_addr.s_addr=inet_addr(SERV_ADDR);
	//地址从转包TCP中获得 
	remote_addr.sin_port=htons(SIP_PORT); //服务器端口号  
	/*创建客户端套接字--IPv4协议，面向连接通信，TCP协议*/  
	if((sockfd=socket(PF_INET,SOCK_STREAM,0))<0)  
	{
		perror("error:sip socket");  
		return -1;  
	}  

	/*将套接字绑定到服务器的网络地址上*/  
	if(connect(sockfd,(struct sockaddr *)&remote_addr,sizeof(struct sockaddr))<0)  
	{
		perror("error:sip connect");  
		return -1;  
	}  
	return sockfd;
}

//这个函数断开到本地SIP进程的TCP连接. 
void disconnectToSIP(int sip_conn) {
	close(sip_conn);
}

int main() {
	//用于丢包率的随机数种子
	srand(time(NULL));

	//连接到SIP进程并获得TCP套接字描述符	
	int sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("fail to connect to the local SIP process\n");
		exit(1);
	}

	//初始化stcp客户端
	stcp_client_init(sip_conn);
	sleep(STARTDELAY);

	char hostname[50];
	printf("Enter server name to connect:");
	scanf("%s",hostname);
	int server_nodeID = topology_getNodeIDfromname(hostname);
	if(server_nodeID == -1) {
		printf("host name error!\n");
		exit(1);
	} else {
		printf("connecting to node %d\n",server_nodeID);
	}

	//在端口87上创建STCP客户端套接字, 并连接到STCP服务器端口88.
	int sockfd = stcp_client_sock(CLIENTPORT1);
	if(sockfd<0) {
		printf("fail to create stcp client sock");
		exit(1);
	}
	if(stcp_client_connect(sockfd,server_nodeID,SERVERPORT1)<0) {
		printf("fail to connect to stcp server\n");
		exit(1);
	}
	printf("client connected to server, client port:%d, server port %d\n",CLIENTPORT1,SERVERPORT1);
	
	//获取sendthis.txt文件长度, 创建缓冲区并读取文件中的数据
	FILE *f;
	f = fopen("Oliver Twist.txt","r");
	assert(f!=NULL);
	fseek(f,0,SEEK_END);
	int fileLen = ftell(f);
	fseek(f,0,SEEK_SET);
	char *buffer = (char*)malloc(fileLen);
	fread(buffer,fileLen,1,f);
	fclose(f);
	//首先发送文件长度, 然后发送整个文件.
	stcp_client_send(sockfd,&fileLen,sizeof(int));
    stcp_client_send(sockfd, buffer, fileLen);
	free(buffer);
	//等待一段时间, 然后关闭连接.
	sleep(WAITTIME);

	if(stcp_client_disconnect(sockfd)<0) {
		printf("fail to disconnect from stcp server\n");
		exit(1);
	}
	if(stcp_client_close(sockfd)<0) {
		printf("fail to close stcp client\n");
		exit(1);
	}
	
	//断开与SIP进程之间的连接
	disconnectToSIP(sip_conn);
}