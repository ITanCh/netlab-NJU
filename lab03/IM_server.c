#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

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
		n = read(usr->sock,sendbuf,MAXLINE);		//get the message
		if(n<0)					//the user want to send
		{
			perror("ERROR reading from socket");
			return;	
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
						write(usr->sock,sendbuf,MAXLINE);//tell user he has log in
						break;
					}
					pthread_mutex_unlock(&lock);
					sendbuf[0]='1';
					write(usr->sock,sendbuf,MAXLINE);       //name has been used
					//printf("1-2%s\n",sendbuf);
					break;
				}
				else if(sendbuf[0]=='4')
				{
					pthread_mutex_lock(&(usr->usrlock));
					usr->state=5;
					pthread_mutex_unlock(&(usr->usrlock));
					return;	
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
					write(usr->sock,sendbuf,MAXLINE);
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
							write(q->sock,sendbuf,MAXLINE);
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
								write(q->sock,sendbuf,MAXLINE);
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
							write(q->sock,sendbuf,MAXLINE);
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
					write(usr->sock,sendbuf,MAXLINE);
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
					return;	
				}
				break;

		}
	}
}

/*when this user get some message from others*/
void *GiveTask(void *arg)
{
	user* usr=(user*)arg;
	int n;
	int state;
	while(1)
	{
		pthread_mutex_lock(&(usr->usrlock));
		state=usr->state;
		pthread_mutex_unlock(&(usr->usrlock));
		switch(state){
			case 2:			//可以发送消息
				pthread_mutex_lock(&lock);
				pthread_cond_wait(&has_message,&lock);
				printf("wake send:%s\n",msgbuf);
				if(msgid!=usr->id){
					write(usr->sock,msgbuf,MAXLINE);
					if(n<0)
					{
						perror("EEEOR writing to socket");
						return;
					}
					count--;
				}
				pthread_mutex_unlock(&lock);
				break;
			case 4:		//let recvtask of this user know it 
				write(usr->sock,"q",MAXLINE);
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
				return;	
			case 5:				//quit without login 
				write(usr->sock,"q",MAXLINE);
				free(usr);
				return;
			default:sleep(1);
		}
	}
}


int main(int argc, char*argv[])
{
	int sockfd,newsockfd,clilen;
	struct sockaddr_in serv_addr, cli_addr;
	int  n;

	int id=0;
	int rc;
	pthread_attr_t attr;
	void * status;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);

	/* First call to socket() function */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
	{
		perror("ERROR opening socket");
		exit(1);
	}
	int on = 1;	//port可以立即重新使用
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(SERV_PORT);

	/* Now bind the host address using bind() call.*/
	if (bind(sockfd, (struct sockaddr *) &serv_addr,
				sizeof(serv_addr)) < 0)
	{
		perror("ERROR on binding");
		exit(1);
	}
	/* Now start listening for the clients, here 
	 *      * process will go in sleep mode and will wait 
	 *           * for the incoming connection
	 *                */
	listen(sockfd,LISTENQ);
	clilen = sizeof(cli_addr);
	printf("====server start=====\n");
	while (1) 
	{
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		printf("%d link in..\n",newsockfd);
		if (newsockfd < 0)
		{
			perror("ERROR on accept");
			exit(1);
		}
		/* Create child process */
		user * p=(user *)malloc(sizeof(user));
		p->sock=newsockfd;
		p->state=1;
		int i=0;
		for(;i<LISTENQ;i++)
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
