#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include "../common/constants.h"
#include "stcp_server.h"

#define MAXLINE 128
#define NAME_LENGTH 20
#define SERV_PORT 6666
#define LISTENQ 10
#define random(x) (rand()%x)

typedef struct USER{
	pthread_mutex_t usrlock;
	char name[NAME_LENGTH];
	pthread_t id;
	int sock;
	int state;
	struct USER* next;
	int fcount;
	struct USER* fri[LISTENQ];
}user;

pthread_cond_t has_message = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
/*lock*/
char msgbuf[MAXLINE];	//public buffer
pthread_t msgid;
int userCount=0;
int count=0;		//count who can take this message
user * head=NULL;
/*lock*/

/*when this user want to send message,get info frome this user*/
void *GetTask(void *arg)
{
	user* usr=(user *)arg;
	char sendbuf[MAXLINE];
	char temp[MAXLINE];
	int n;
	int state;
	while(1)
	{
		memset(sendbuf,0,sizeof(sendbuf));
		memset(temp,0,sizeof(temp));
		n = stcp_server_recv(usr->sock,sendbuf,MAXLINE);		//get the message
		if(n<0)					//the user want to send
		{
			perror("ERROR reading from socket");
			return NULL;	
		}
		pthread_mutex_lock(&(usr->usrlock));
		state=usr->state;
		pthread_mutex_unlock(&(usr->usrlock));
		printf("state: %d  :buf: %s\n",state,sendbuf);
		switch(state){
			case 1:			//get name and come in
				if(sendbuf[0]=='1'){
					strcpy(usr->name,&sendbuf[1]);
					user* p=head;
					while(1){	//wait for all user get the old msg 
						//then update
						pthread_mutex_lock(&lock);
						if(count>0)
						{
							pthread_mutex_unlock(&lock);
							sleep(1);
						}
						else
							break;
					}	

					while(p!=NULL)		//if this name has been used
					{
						if(strcmp(p->name,usr->name)==0)
						{
							pthread_mutex_unlock(&lock);
							break;
						}
						p=p->next;
					}
					if(p==NULL)
					{
						usr->next=head;
						head=usr;
						count=userCount;
						userCount++;
						usr->fcount=0;
						msgid=usr->id;
						strcpy(msgbuf,"&&&&& ");
						strcat(msgbuf,usr->name);
						strcat(msgbuf," come in &&&&\n");
						pthread_cond_broadcast(&has_message);
						pthread_mutex_unlock(&lock);
						pthread_mutex_lock(&(usr->usrlock));
						usr->state=2;		//read for send msg to others
						pthread_mutex_unlock(&(usr->usrlock));
						sendbuf[0]='2';		
						stcp_server_send(usr->sock,sendbuf,MAXLINE);//tell user he has log in
						break;
					}
					pthread_mutex_unlock(&lock);
					sendbuf[0]='1';
					stcp_server_send(usr->sock,sendbuf,MAXLINE);
					//printf("1-2%s\n",sendbuf);
					break;
				}
				else if(sendbuf[0]=='4')
				{
					pthread_mutex_lock(&(usr->usrlock));
					usr->state=5;
					pthread_mutex_unlock(&(usr->usrlock));
					return NULL;	
				}
				break;

			case 2:			//the user want to send this msg
				if(sendbuf[0]=='2')
				{
					while(1){	//wait for all user get the old msg 
						//then update
						pthread_mutex_lock(&lock);
						if(count>0)
						{
							pthread_mutex_unlock(&lock);
							sleep(1);
						}
						else
							break;
					}	
					strcpy(msgbuf,sendbuf);
					count=userCount-1;
					msgid=usr->id;
					pthread_cond_broadcast(&has_message); //let the waiters know it
					printf("id:%d   msgbuf: %s",(int)msgid,msgbuf);
					pthread_mutex_unlock(&lock);
				}
				else if(sendbuf[0]=='3')	//user want to list all members
				{
					//printf("list 2-3\n");
					memset(sendbuf,0,sizeof(sendbuf));
					sendbuf[0]='3';
					pthread_mutex_lock(&lock);
					user* p=head;
					while(p!=NULL){
						if(strlen(sendbuf)+strlen(p->name)>MAXLINE)break;
						strcat(sendbuf,p->name);
						strcat(sendbuf," |*| ");
						p=p->next;
					}
					pthread_mutex_unlock(&lock);
					strcat(sendbuf,"\n");
					stcp_server_send(usr->sock,sendbuf,MAXLINE);
				}
				else if(sendbuf[0]=='@')	//send to the one
				{
					strcpy(temp,sendbuf);
					char *delim="@";
					strtok(temp,delim);
					strcpy(temp,&temp[1]);
					char *p=strtok(NULL,delim);
					//printf("%s,%s\n",temp,p);
					memset(sendbuf,0,sizeof(sendbuf));
					strcpy(sendbuf,"2");
					strcat(sendbuf,usr->name);
					//printf("%s\n",sendbuf);
					strcat(sendbuf, " (private): ");
					strcat(sendbuf,p);
					pthread_mutex_lock(&lock);
					user *q=head;
					while(q!=NULL){
						if(strcmp(q->name,temp)==0)			//find this gay
						{
							stcp_server_send(q->sock,sendbuf,MAXLINE);
							break;
						}
						q=q->next;
					}
					pthread_mutex_unlock(&lock);
				}
				else if(sendbuf[0]=='$')			//make friend
				{
					strcpy(temp,&sendbuf[1]);
					int l=strlen(temp);
					if(temp[l-1]=='\n')temp[l-1]=0;
					if(strcmp(temp,usr->name)==0)break;		//cannot be friend with yourself
					memset(sendbuf,0,sizeof(sendbuf));
					strcpy(sendbuf,"$");
					strcat(sendbuf,usr->name);
					pthread_mutex_lock(&lock);
					user *q=head;
					while(q!=NULL){
						if(strcmp(q->name,temp)==0)			//find this gay
						{
							printf("$$$$sendbuf:%s\n",sendbuf);
							int i=0;
							int flag=1;
							for(;i<LISTENQ;i++)
							{
								if(usr->fri[i]==q)flag=0;
							}
							if(flag)
								stcp_server_send(q->sock,sendbuf,MAXLINE);
							break;
						}
						q=q->next;
					}
					pthread_mutex_unlock(&lock);

				}
				else if(sendbuf[0]=='y'||sendbuf[0]=='n'){   //yes or no  be friends
					strcpy(temp,&sendbuf[1]);
					if(sendbuf[0]=='y')
					{
						memset(sendbuf,0,sizeof(sendbuf));
						strcpy(sendbuf,"y");
						strcat(sendbuf,usr->name);
					}
					else
					{
						memset(sendbuf,0,sizeof(sendbuf));
						strcpy(sendbuf,"n");
						strcat(sendbuf,usr->name);

					}
					pthread_mutex_lock(&lock);
					user *q=head;
					while(q!=NULL){
						if(strcmp(q->name,temp)==0)			//find this gay
						{
							printf("y/n temp:%s\n",temp);
							if(sendbuf[0]=='y')
							{
								q->fri[q->fcount]=usr;
								q->fcount=(q->fcount+1)%LISTENQ;
								usr->fri[usr->fcount]=q;
								usr->fcount=(usr->fcount+1)%LISTENQ;
							}
							stcp_server_send(q->sock,sendbuf,MAXLINE);
							printf("y/n sendbuf:%s\n",sendbuf);
							break;
						}
						q=q->next;
					}
					pthread_mutex_unlock(&lock);

				}
				else if(sendbuf[0]=='!')
				{
					printf("1!!!sendbuf: %s\n",sendbuf);
					memset(sendbuf,0,sizeof(sendbuf));
					pthread_mutex_lock(&lock);
					int i=0;
					for(;i<LISTENQ;i++)
					{
						if(usr->fri[i]!=NULL)
						{
							strcat(sendbuf,"!");
							strcat(sendbuf,usr->fri[i]->name);
							strcat(sendbuf," || ");
						}
					}
					printf("!!!sendbuf: %s\n",sendbuf);
					pthread_mutex_unlock(&lock);
					stcp_server_send(usr->sock,sendbuf,MAXLINE);
					break;
				}
				else if(sendbuf[0]=='4')		//user quit
				{
					while(1){	//wait for all user get the old msg 
						//then update
						pthread_mutex_lock(&lock);
						if(count>0)
						{
							pthread_mutex_unlock(&lock);
							sleep(1);
						}
						else
							break;
					}
					userCount--;
					pthread_mutex_lock(&(usr->usrlock));
					usr->state=4;
					pthread_mutex_unlock(&(usr->usrlock));
					count=userCount;
					msgid=usr->id;
					strcpy(msgbuf,"@@@@@ ");
					strcat(msgbuf,usr->name);
					strcat(msgbuf," out of room @@@@\n");
					pthread_cond_broadcast(&has_message);
					pthread_mutex_unlock(&lock);
					return NULL;	
				}
				break;

		}
	}
}

/*when this user get some message from others*/
void *GiveTask(void *arg)
{
	user* usr=(user*)arg;
	char temp[MAXLINE];
	int n;
	int state;
	while(1)
	{
		memset(temp,0,MAXLINE);
		pthread_mutex_lock(&(usr->usrlock));
		state=usr->state;
		pthread_mutex_unlock(&(usr->usrlock));
		switch(state){
			case 2:			//可以发送消息
				pthread_mutex_lock(&lock);
				pthread_cond_wait(&has_message,&lock);
				printf("wake send:%s\n",msgbuf);
				if(msgid!=usr->id){
					n=stcp_server_send(usr->sock,msgbuf,MAXLINE);
					if(n<0)
					{
						perror("EEEOR writing to socket");
						return NULL;
					}
					count--;
				}
				pthread_mutex_unlock(&lock);
				break;
			case 4:		//let recvtask of this user know it 
				temp[0]='q';
				int closesock=usr->sock;
				stcp_server_send(usr->sock,temp,MAXLINE);
				pthread_mutex_lock(&lock);
				int i=0;
				for(;i<LISTENQ;i++)			//cancel friend line
				{
					if(usr->fri[i]!=NULL)
					{
						user *f=usr->fri[i];
						int j=0;
						for(;j<LISTENQ;j++)
						{
							printf("quit :%d\n",j);
							if(f->fri[j]==usr)f->fri[j]=NULL;
						}
					}
				}
				user*p=head;		//free the memory
				if(p->id==usr->id)
				{
					head=p->next;
					free(p);
					p=NULL;
				}
				else{
					while(p->next!=NULL)
					{
						if(p->next->id==usr->id)	
							break;
						p=p->next;
					}
					user*q=p->next;
					if(q!=NULL)
					{
						p->next=q->next;
						free(q);
						q=NULL;
					}
					else
					{
						free(usr);
					}
				}
				pthread_mutex_unlock(&lock);
				stcp_server_close(closesock);
				return NULL;	
			case 5:				//quit without login 
				temp[0]='q';
				stcp_server_send(usr->sock,temp,MAXLINE);
				stcp_server_close(usr->sock);
				free(usr);
				return NULL;
			default:sleep(1);
		}
	}
}

//这个函数连接到本地SIP进程的端口SIP_PORT. 如果TCP连接失败, 返回-1. 连接成功, 返回TCP套接字描述符, STCP将使用该描述符发送段.
int connectToSIP() {

	//local IP Address
	char SERV_ADDR[15]="127.0.0.1";
	int sockfd;  
	struct sockaddr_in local_addr;
	//init
	memset(&local_addr,0,sizeof(local_addr)); 

	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = inet_addr(SERV_ADDR);
	local_addr.sin_port = htons(SIP_PORT);   
	//create socket
	if((sockfd = socket(PF_INET,SOCK_STREAM,0))<0)  
	{
		perror("STCP socket ERROR!");  
		return -1;  
	}  
	//connect to local son_port
	if(connect(sockfd,(struct sockaddr *)&local_addr,sizeof(struct sockaddr)) < 0)  
	{
		perror("STCP connect ERROR!");  
		return -1;  
	}
	printf("[Stress]Server-STCP: CONNECT TO SIP_PORT\n");
	return sockfd;
}

//这个函数断开到本地SIP进程的TCP连接. 
void disconnectToSIP(int sip_conn) {
	close(sip_conn);
	printf("[Stress]Server-STCP: DISCONNECT TO SIP_PORT\n");
}

int main(int argc, char*argv[])
{
	int i;
	//用于丢包率的随机数种子
	srand(time(NULL));

	//连接到SIP进程并获得TCP套接字描述符
	int sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("can not connect to the local SIP process\n");
	}

	//初始化STCP服务器
	stcp_server_init(sip_conn);

	//在端口SERVERPORT1上创建STCP服务器套接字 
	stcp_server_sock(SERV_PORT);

	int rc;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);

	printf("====server start=====\n");
	while (1) 
	{
		int newsockfd = stcp_server_accept();
		if (newsockfd < 0)
		{
			perror("ERROR on accept");
			exit(1);
		}
		printf("%d link in..\n",newsockfd);
		
		/* Create child process */
		user * p=(user *)malloc(sizeof(user));
		p->sock=newsockfd;
		p->state=1;
		for(i=0;i<LISTENQ;i++)
		{
			p->fri[i]=NULL;
		}
		pthread_mutex_init(&p->usrlock, NULL);
		rc=pthread_create(&p->id,&attr,GetTask,(void*)p);
		if(rc){
			printf("ERROE;return code from pthread_c    reate() is %d\n",rc);
		}		
		rc=pthread_create(&p->id,&attr,GiveTask,(void*)p);
		pthread_mutex_unlock(&lock);		
		if(rc){
			printf("ERROE;return code from pthread_c    reate() is %d\n",rc);
		}
	} /* end of while */
	pthread_attr_destroy(&attr);
	pthread_exit(NULL);
}	
