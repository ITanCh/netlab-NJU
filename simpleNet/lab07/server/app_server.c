//文件名: server/app_server.c

//描述: 这是服务器应用程序代码. 服务器首先通过在客户端和服务器之间创建TCP连接,启动重叠网络层. 然后它调用stcp_server_init()初始化STCP服务器. 
//它通过两次调用stcp_server_sock()和stcp_server_accept()创建2个套接字并等待来自客户端的连接. 最后, 服务器通过调用stcp_server_close()关闭套接字.
//重叠网络层通过调用son_stop()停止.

//创建日期: 2013年

//输入: 无

//输出: STCP服务器状态

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

#include "../common/constants.h"
#include "stcp_server.h"

//创建两个连接, 一个使用客户端端口号87和服务器端口号88. 另一个使用客户端端口号89和服务器端口号90
#define CLIENTPORT1 87
#define SERVERPORT1 88
#define CLIENTPORT2 89
#define SERVERPORT2 90
//在连接创建后, 等待10秒, 然后关闭连接
#define WAITTIME 10

//这个函数通过在客户和服务器之间创建TCP连接来启动重叠网络层. 它返回TCP套接字描述符, STCP将使用该描述符发送段. 如果TCP连接失败, 返回-1.
int son_start() {
	int listenfd,connfd;
	socklen_t clilen;
	struct sockaddr_in servaddr,cliaddr;
	if((listenfd = socket(AF_INET,SOCK_STREAM,0)) < 0){
			perror("Problem in creating the socket...\n");
			return -1;
	}
	clilen = sizeof(cliaddr);
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SON_PORT);

	bind(listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr));
	listen(listenfd,MAX_TRANSPORT_CONNECTIONS);
	connfd = accept(listenfd,(struct sockaddr*)&cliaddr,&clilen);
	printf("son_started, Server is waiting for connections...\n");
	return connfd;
}

//这个函数通过关闭客户和服务器之间的TCP连接来停止重叠网络层
void son_stop(int son_conn) {
	close(son_conn);
}

int main() {
	//用于丢包率的随机数种子
	srand(time(NULL));

	//启动重叠网络层并获取重叠网络层TCP套接字描述符
	int son_conn = son_start();
	if(son_conn<0) {
		printf("can not start overlay network\n");
	}

	//初始化STCP服务器
	stcp_server_init(son_conn);
	
	//在端口SERVERPORT1上创建STCP服务器套接字 
	int sockfd= stcp_server_sock(SERVERPORT1);
	if(sockfd<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//监听并接受来自STCP客户端的连接 
	stcp_server_accept(sockfd);

	//在端口SERVERPORT2上创建另一个STCP服务器套接字
	int sockfd2= stcp_server_sock(SERVERPORT2);
	if(sockfd2<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//监听并接受来自STCP客户端的连接 
	stcp_server_accept(sockfd2);

	sleep(WAITTIME);

	//关闭STCP服务器 
	if(stcp_server_close(sockfd)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				
	if(stcp_server_close(sockfd2)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				

	//停止重叠网络层
	son_stop(son_conn);
}
