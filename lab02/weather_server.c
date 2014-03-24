#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>  

#define MAXLINE 4068
#define SERV_PORT 6666
#define LISTENQ 10
#define random(x) (rand()%x)

void  handlecli(int sock)
{
	int n;
	char sendbuf[MAXLINE];
	char recvbuf[MAXLINE];
	char name[50];
	//获取时间
	struct tm *ptr;
	time_t lt;
	lt =time(NULL);
	ptr=localtime(&lt);
	int year=ptr->tm_year+1900;
	int month=ptr->tm_mon+1;
	int day=ptr->tm_mday;
	while(1){
		memset(sendbuf,0,sizeof(sendbuf));
		memset(recvbuf,0,sizeof(recvbuf));
		n = read(sock,recvbuf,MAXLINE);
		if (n < 0)
		{
			perror("ERROR reading from socket");
			exit(1);
		}
		if(recvbuf[0]==0x01&&recvbuf[1]==0x00)	//查询该地名是否存在
		{
			strcpy(name,&recvbuf[2]);
			printf("pid: %d 查询该%s是否存在\n",getpid(),name);
			if(strcmp(name,"nanjing")==0||strcmp(name,"beijing")==0||strcmp(name,"shanghai")==0)
				sendbuf[0]=0x03;
			else
				sendbuf[0]=0x04;
			strcpy(&sendbuf[2],name);
			write(sock,sendbuf,137);
			if (n < 0) 
			{
				perror("ERROR writing to socket");
				exit(1);
			}

		}
		else if(recvbuf[0]==0x02&&recvbuf[1]==0x01)//查询某一天
		{
			printf("pid: %d 查询第%d天\n",getpid(),recvbuf[32]);
			printf("year:%d\n",year);
			srand((int)time(0));
			sendbuf[0]=0x01;
			sendbuf[1]=0x41;
			strcpy(&sendbuf[2],&recvbuf[2]);
			sendbuf[32]=year/16/16;
			sendbuf[33]=year%(16*16);
			sendbuf[34]=month;
			sendbuf[35]=day;
			printf("year1:%x year:%x\n",sendbuf[32],sendbuf[33]);
			sendbuf[37]=random(5);	//天气
			sendbuf[38]=random(25);
			n=write(sock,sendbuf,137);
			if (n < 0) 
			{
				perror("ERROR writing to socket");
				exit(1);
			}

		}
		else if(recvbuf[0]==0x02&&recvbuf[1]==0x02)	//查询3天
		{
			printf("pid: %d 查询3天\n",getpid());
			srand((int)time(0));
			sendbuf[0]=0x01;
			sendbuf[1]=0x42;
			strcpy(&sendbuf[2],&recvbuf[2]);
			sendbuf[32]=year/16/16;
			sendbuf[33]=year%(16*16);
			sendbuf[34]=month;
			sendbuf[35]=day;
			sendbuf[37]=random(5);	//天气
			sendbuf[38]=random(25);
			sendbuf[39]=random(5);	//天气
			sendbuf[40]=random(25);
			sendbuf[41]=random(5);	//天气
			sendbuf[42]=random(25);
			n=write(sock,sendbuf,137);
			if (n < 0) 
			{
				perror("ERROR writing to socket");
				exit(1);
			}
		}
	}
}


int main(int argc, char*argv[])
{
	int sockfd,newsockfd,clilen;
	char buffer[MAXLINE];
	struct sockaddr_in serv_addr, cli_addr;
	int  n;
	pid_t pid;

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
	while (1) 
	{
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0)
		{
			perror("ERROR on accept");
			exit(1);
		}
		/* Create child process */
		pid = fork();
		if (pid < 0)
		{
			perror("ERROR on fork");
			exit(1);
		}
		if (pid == 0)  
		{
			/* This is the client process */
			close(sockfd);
			handlecli(newsockfd);
			exit(0);
		}
		else
		{
			close(newsockfd);
		}
	} /* end of while */
}
