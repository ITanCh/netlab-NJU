//文件名: sip/sip.c
//
//描述: 这个文件实现SIP进程  
//
//创建日期: 2013年1月

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/seg.h"
#include "../topology/topology.h"
#include "sip.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"

//SIP层等待这段时间让SIP路由协议建立路由路径. 
#define SIP_WAITTIME 60

/**************************************************************/
//声明全局变量
/**************************************************************/
int son_conn; 			//到重叠网络的连接
int stcp_conn;			//到STCP的连接
nbr_cost_entry_t* nct;			//邻居代价表
dv_t* dv;				//距离矢量表
pthread_mutex_t* dv_mutex;		//距离矢量表互斥量
routingtable_t* routingtable;		//路由表
pthread_mutex_t* routingtable_mutex;	//路由表互斥量

/**************************************************************/
//实现SIP的函数
/**************************************************************/

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT.
//成功时返回连接描述符, 否则返回-1.
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
		perror("SIP: SON socket error\n");  
		return -1;  
	}  
	//connect to local son_port
	if(connect(sockfd,(struct sockaddr *)&local_addr,sizeof(struct sockaddr)) < 0)  
	{
		perror("SIP: SON connect error\n");  
		return -1;  
	}
	printf("sip: CONNECT TO SON_PORT\n");
	return sockfd;
}

//这个线程每隔ROUTEUPDATE_INTERVAL时间发送路由更新报文.路由更新报文包含这个节点
//的距离矢量.广播是通过设置SIP报文头中的dest_nodeID为BROADCAST_NODEID,并通过son_sendpkt()发送报文来完成的.
void* routeupdate_daemon(void* arg) {
	sip_pkt_t *update_sip = (sip_pkt_t*)malloc(sizeof(sip_pkt_t));
	pkt_routeupdate_t *routeupdate_pkt = (pkt_routeupdate_t*)malloc(sizeof(pkt_routeupdate_t));
	int i = 0;
	//settings
	int node_num = topology_getNodeNum();
	int nbr_num = topology_getNbrNum();
	routeupdate_pkt->entryNum = node_num;
	int local_nodeID = topology_getMyNodeID();
	//find local node distance vector
	//int my_dv = 0;
	dv_entry_t *my_dv_entry = NULL;
	for(i = 0;i < nbr_num + 1;i++)
	{
		if(local_nodeID == dv[i].nodeID)
		{
			my_dv_entry = dv[i].dvEntry;
			break;
		}
	}
	update_sip->header.src_nodeID = topology_getMyNodeID();;
	update_sip->header.dest_nodeID = BROADCAST_NODEID;
	update_sip->header.length = sizeof(pkt_routeupdate_t);
	update_sip->header.type = ROUTE_UPDATE;
	while(1)
	{
		sleep(ROUTEUPDATE_INTERVAL);
		//printf("SIP: Update Broadcast\n");
		pthread_mutex_lock(dv_mutex);
		for(i = 0;i < node_num;i++)
		{
			routeupdate_pkt->entry[i].nodeID = my_dv_entry[i].nodeID;
			routeupdate_pkt->entry[i].cost = my_dv_entry[i].cost;
			printf("【Send】update entry %d: nodeID %d \tcost %d\n",i,routeupdate_pkt->entry[i].nodeID,routeupdate_pkt->entry[i].cost);
		}
		memcpy(update_sip->data,routeupdate_pkt,update_sip->header.length);
		pthread_mutex_unlock(dv_mutex);
		if(son_conn != -1 && son_sendpkt(BROADCAST_NODEID,update_sip,son_conn) > 0)
			printf("SIP send a pkt to SON\n");
		else
			son_conn = -1;
		//printf("SIP: Broadcast Done\n");
	}
	return 0;
}

//这个线程处理来自SON进程的进入报文. 它通过调用son_recvpkt()接收来自SON进程的报文.
//如果报文是SIP报文,并且目的节点就是本节点,就转发报文给STCP进程. 如果目的节点不是本节点,
//就根据路由表转发报文给下一跳.如果报文是路由更新报文,就更新距离矢量表和路由表.
void* pkthandler(void* arg) {
	sip_pkt_t pkt;

	while(son_conn != -1 && son_recvpkt(&pkt,son_conn) > 0) 
	{
		//printf("Routing: received a packet from neighbor %d\n",pkt.header.src_nodeID);
		//printf("sip[INFO] dest:%d length:%d type:%d\n",pkt.header.dest_nodeID, pkt.header.length, pkt.header.type);
		if(pkt.header.type == ROUTE_UPDATE)
		{	// broadcast update 
			pkt_routeupdate_t *route_update = (pkt_routeupdate_t*)pkt.data;
			//preparations
			int entry_num = route_update->entryNum;
			routeupdate_entry_t *rupentry = route_update->entry;
			int src_nodeID = pkt.header.src_nodeID;
			int local_nodeID = topology_getMyNodeID();
			int nbr_num = topology_getNbrNum();
			int node_num = topology_getNodeNum();
			//int *nbr_array = topology_getNbrArray();
			dv_entry_t *local_dv = NULL;
			int i = 0;
			int j = 0;
			int cost1 = 0;
			int cost2 = 0;
			printf("SIP got ROUTE_UPDATE pkt from node(%d)\n",src_nodeID);
			for(i = 0;i < node_num;i++)
				printf("【Recv】update entry %d: nodeID %d \tcost %d\n",i,rupentry[i].nodeID,rupentry[i].cost);

			pthread_mutex_lock(dv_mutex);
			pthread_mutex_lock(routingtable_mutex);
			for(i = 0;i < entry_num;i++)
				dvtable_setcost(dv,src_nodeID,rupentry[i].nodeID,rupentry[i].cost);
			
			for(j = 0;j < nbr_num + 1;j++)
				if(local_nodeID == dv[j].nodeID)
				{
					local_dv = dv[j].dvEntry;
					break;
				}

			for(i = 0;i < node_num;i++)
			{
				int dest_nodeID = local_dv[i].nodeID;
				int old_cost = local_dv[i].cost;
				//for(j = 0;j < nbr_num;j++)
				//{
				int medi_nodeID = src_nodeID;
				cost1 = nbrcosttable_getcost(nct,medi_nodeID);
				cost2 = dvtable_getcost(dv,medi_nodeID,dest_nodeID);
				if(cost1 + cost2 < old_cost)
				{
					printf("update dvtable and routingtable\n");
					dvtable_setcost(dv,local_nodeID,dest_nodeID,cost1+cost2);
					routingtable_setnextnode(routingtable,dest_nodeID,medi_nodeID);
				}
				//}
			}
			pthread_mutex_unlock(dv_mutex);
			pthread_mutex_unlock(routingtable_mutex);
		
		}
		else if(pkt.header.type == SIP)
		{
			int local_nodeID = topology_getMyNodeID();
			if(pkt.header.dest_nodeID == local_nodeID)
			{	//forward pkt to stcp
				printf("SIP forward pkt to STCP\n");
				forwardsegToSTCP(stcp_conn,pkt.header.src_nodeID,(seg_t*)pkt.data);
			}
			else
			{	//forward pkt to son
				int next_nodeID = routingtable_getnextnode(routingtable,pkt.header.dest_nodeID);
				son_sendpkt(next_nodeID,&pkt,son_conn);
				printf("SIP forward pkt to SON to node %d\n",next_nodeID);
			}
		}
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
	printf("SIP: close SIP\n");
}

//这个函数打开端口SIP_PORT并等待来自本地STCP进程的TCP连接.
//在连接建立后, 这个函数从STCP进程处持续接收包含段及其目的节点ID的sendseg_arg_t. 
//接收的段被封装进数据报(一个段在一个数据报中), 然后使用son_sendpkt发送该报文到下一跳. 下一跳节点ID提取自路由表.
//当本地STCP进程断开连接时, 这个函数等待下一个STCP进程的连接.
void waitSTCP() {
	int sockfd,newsockfd;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	/* First call to socket() function */
	if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) 
	{
		perror("waitSIP ERROR opening socket\n");
		exit(1);
	}
	int on = 1;	//port可以立即重新使用
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	//init
	memset(&serv_addr,0,sizeof(serv_addr));
	//settings
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(SIP_PORT);
	/* Now bind the host address using bind() call.*/
	if(bind(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
	{
		perror("SIP-waitSTCP: ERROR on binding\n");
		exit(2);
	}
	listen(sockfd,10);
	printf("SIP-WaitSTCP: Begin to wait for STCP Connectins...\n");
	int local_nodeID = topology_getMyNodeID();
	while(1)
	{
		clilen = sizeof(cli_addr);
		memset(&cli_addr,0,sizeof(cli_addr));
		newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr,&clilen);

		stcp_conn = newsockfd;
		printf("STCP %d link in..\n",newsockfd);
	
		seg_t *buf = (seg_t*)malloc(sizeof(seg_t));
		int dest_nodeID = 0;
		int next_nodeID = 0;
		while(getsegToSend(stcp_conn,&dest_nodeID,buf) > 0)
		{
			printf("SIP-Send: begin to send seg from STCP in a pkt\n");
			sip_pkt_t *new_pkt = (sip_pkt_t*)malloc(sizeof(sip_pkt_t));
			//settings
			new_pkt->header.src_nodeID = local_nodeID;
			new_pkt->header.dest_nodeID = dest_nodeID;
			new_pkt->header.length = sizeof(stcp_hdr_t) + buf->header.length;
			new_pkt->header.type = SIP;
			memcpy(new_pkt->data,buf,new_pkt->header.length);
			//search for next_nodeID
			next_nodeID = routingtable_getnextnode(routingtable,dest_nodeID);
			//send pkt
			if(son_conn != -1 && son_sendpkt(next_nodeID,new_pkt,son_conn) > 0)
				printf("SIP send a pkt to SON\n");
			else
				son_conn = -1;
			printf("SIP-Send: node(%d) sent a pkt to node(%d)",local_nodeID,dest_nodeID);
		}
	}
	printf("SIP-WaitSTCP: Finish waiting for connections...\n");
	return ;
}

int main(int argc, char *argv[]) {
	printf("SIP layer is starting, pls wait...\n");

	//初始化全局变量
	nct = nbrcosttable_create();
	dv = dvtable_create();
	dv_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(dv_mutex,NULL);
	routingtable = routingtable_create();
	routingtable_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(routingtable_mutex,NULL);
	son_conn = -1;
	stcp_conn = -1;

	nbrcosttable_print(nct);
	dvtable_print(dv);
	routingtable_print(routingtable);

	//注册用于终止进程的信号句柄
	signal(SIGINT, sip_stop);

	//连接到本地SON进程 
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
	printf("waiting for routes to be established\n");
	sleep(SIP_WAITTIME);
	routingtable_print(routingtable);

	//等待来自STCP进程的连接
	printf("waiting for connection from STCP process\n");
	waitSTCP(); 

}


