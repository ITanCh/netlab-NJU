#include <stdio.h>  
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include <assert.h>
#include "../common/constants.h"
#include "../topology/topology.h"
#include "stcp_client.h"

#define MAXLINE 128
#define SERVERPORT 6666
#define CLIENTPORT 88
#define NAME_LENGTH 20
#define STARTDELAY 1
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
char name[NAME_LENGTH];
char fname[NAME_LENGTH];
int state=0;
/*send task*/
void *SendTask(void *arg){
	int sockfd=(intptr_t)arg;
	char sendbuf[MAXLINE];
	char temp[MAXLINE-NAME_LENGTH-1];
	while(1){
		memset(sendbuf,0,MAXLINE);
		memset(temp,0,sizeof(temp));
		//printf("state: %d",state);
		switch(state){
			case 0:
				system("clear");
				printf("Welcom to IMQQ\n");
				printf("clear(c),help(h),quit(q)\n");	
				printf("===============================================\n");
				state=1;break;
			case 1:					//set name state
				printf("Please enter your name:\n");
				fgets(name,NAME_LENGTH-1,stdin);
				if(strcmp(name,"c\n")==0){state=0;break;}
				if(strcmp(name,"h\n")==0)
				{
					printf("HELP:\n1.It is an IM..\n2.You can google it..\n");
					printf("3.for more info..\n");
					break;
				}
				if(strcmp(name,"q\n")==0){
					state=4;
					break;
				}
				int name_l=strlen(name);
				if(name[name_l-1]=='\n')name[name_l-1]=0;
				name_l=strlen(name);
				if(name_l<1){system("clear");break;}             //防止name为空
				if(name_l>=NAME_LENGTH-2)//名字过长，清空缓存
				{
					printf("name is too long......\n");
					char ch;
					while((ch=getchar())!='\n'&&ch!=EOF);
					break;
				}
				sendbuf[0]='1';
				strcat(sendbuf,name);
				//printf("name give :%s\n",sendbuf);
				stcp_client_send(sockfd,sendbuf,MAXLINE);
				//memset(sendbuf,0,sizeof(sendbuf));
				stcp_client_recv(sockfd,sendbuf,MAXLINE);
				//printf("name recv:%s\n",sendbuf);
				if(sendbuf[0]=='1')
				{
					printf("sorry,name has been used..please change..\n");
					break;
				}
				else if(sendbuf[0]=='2')
				{
					printf("===============================\n    welcom [%s]\n==============================\n",name);
					state=2;break;
				}
				break;
			case 2:					//ready to send msg
				//printf("00000state:%d\n",state);
				fgets(temp,MAXLINE-NAME_LENGTH*2,stdin);
				if(strcmp(temp,"c\n")==0){system("clear");break;}
				if(strcmp(temp,"h\n")==0)
				{
					printf("HELP:\n1.It is an IM..\n2.You can list all online members with 'l'\n");
					printf("3.talk with someone with '@name@...'..\n4.make friends with $name\n5.show your friends(!)\n");
					break;
				}
				if(strcmp(temp,"q\n")==0){
					state=4;
					break;
				}
				if(strcmp(temp,"l\n")==0){
					sendbuf[0]='3';
					stcp_client_send(sockfd,sendbuf,MAXLINE);
					printf("online list : \n");
					break;
				}
				if(strcmp(temp,"y\n")==0||strcmp(temp,"n\n")==0)
				{
					pthread_mutex_lock(&lock);
					if(strlen(fname))
					{
						sendbuf[0]=temp[0];
						strcat(sendbuf,fname);
						stcp_client_send(sockfd,sendbuf,MAXLINE);
						pthread_mutex_unlock(&lock);
						break;
					}
					pthread_mutex_unlock(&lock);
				}
				if(strcmp(temp,"!\n")==0)
				{
					sendbuf[0]='!';
					stcp_client_send(sockfd,sendbuf,MAXLINE);
					printf("your friends:\n");
					break;
				}
				if(temp[0]=='@')		//user want to talk to one person
				{
					strcpy(sendbuf,temp);
					stcp_client_send(sockfd,sendbuf,MAXLINE);
					break;
				}
				if(temp[0]=='$')		//want to make friends with someone
				{
					strcpy(sendbuf,temp);
					stcp_client_send(sockfd,sendbuf,MAXLINE);
					break;

				}
				if(temp[0]=='\n')break;
				sendbuf[0]='2';	
				strcat(sendbuf,"[");
				strcat(sendbuf,name);
				strcat(sendbuf,"]: ");
				strcat(sendbuf,temp);
				stcp_client_send(sockfd,sendbuf,MAXLINE);
				break;
			case 4:				//quit
				sendbuf[0]='4';		//tell server this user quit
				stcp_client_send(sockfd,sendbuf,MAXLINE);
				printf("quit\n");
				state=4;
				return NULL;
		}
	}
}

/*receive task*/
void *RecvTask(void *arg){
	int sockfd=(intptr_t)arg;
	char recvbuf[MAXLINE];
	while(1){
		memset(recvbuf,0,MAXLINE);
		switch(state){
			case 2:		//ready to get msg
				if(stcp_client_recv(sockfd,recvbuf,MAXLINE)<=0)
				{	
					perror("recv server problem..\n");
					return NULL;	
				}
				if(recvbuf[0]=='!'){
					printf("%s\n",recvbuf);
				}
				if(recvbuf[0]=='$')	//some one want to be friend with me
				{
					pthread_mutex_lock(&lock);
					strcpy(fname,&recvbuf[1]);
					pthread_mutex_unlock(&lock);
					printf("%s want to be friends with you\ndo you agree?(y/n)?\n",&recvbuf[1]);
					break;
				}
				if(recvbuf[0]=='y')
				{
					printf("%s agree to be friends with you!!\n",&recvbuf[1]);
					break;
				}
				if(recvbuf[0]=='n')
				{
					printf("sorry,%s do not agree to be friends with you...\n",&recvbuf[1]);
					break;
				}
				printf("%s",&recvbuf[1]);
				break;
			case 4:return NULL;
			default: sleep(1);
		}
	}
	return NULL;
}

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
int main(int argc, char *argv[])  
{  
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
	int client_sockfd = stcp_client_sock(CLIENTPORT);
	if(client_sockfd<0) {
		printf("fail to create stcp client sock");
		exit(1);
	}
	if(stcp_client_connect(client_sockfd,server_nodeID,SERVERPORT)<0) {
		printf("fail to connect to server \n");
		exit(1);
	}
	printf("client connected to server, client port:%d, server port %d\n",CLIENTPORT,SERVERPORT);

	pthread_t threads[2];		//2 threads
	int rc;
	pthread_attr_t attr;
	void * status;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);

	/*create threads*/
	rc=pthread_create(&threads[0],&attr,SendTask,(void*)(intptr_t)client_sockfd);
	if(rc){
		printf("ERROE;return code from pthread_create() is %d\n",rc);
	}

	rc=pthread_create(&threads[1],&attr,RecvTask,(void*)(intptr_t)client_sockfd);
	if(rc){
		printf("ERROE;return code from pthread_create() is %d\n",rc);
	}

	/*wait for the other threads*/
	int i=0;
	for(;i<2;i++)
	{
		rc=pthread_join(threads[i],&status);
		if(rc){
			printf("ERROE;return code from pthread_create() is %d\n",rc);
			exit(-1);
		}
	}
	pthread_attr_destroy(&attr);
	//pthread_exit(NULL);
	stcp_client_disconnect(client_sockfd);//关闭套接字  
	stcp_client_close(client_sockfd);
	return 0;  
}  
