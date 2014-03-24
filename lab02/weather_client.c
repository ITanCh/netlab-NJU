#include <stdio.h>  
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  

#define MAXLINE 4096
#define SERV_PORT 6666 /*可以从wireshark获取的TCP中获得*/
void select_weather(int a){
	switch(a){
		case 0:printf("shower ");break;
		case 1:printf("clear ");break;
		case 2:printf("cloudy ");break;
		case 3:printf("rainy ");break;
		case 4:printf("fog ");break;
	}
}
int main(int argc, char *argv[])  
{  
	char SERV_ADDR[15]="114.212.191.33";
	int client_sockfd;  
	int len;  
	struct sockaddr_in remote_addr; //服务器端网络地址结构体  
	char sendbuf[MAXLINE];  //数据传送的缓冲区 
	char recvbuf[MAXLINE];
	char name[50];	
	char choose;
	char ch;
	memset(&remote_addr,0,sizeof(remote_addr)); //数据初始化--清零  
	remote_addr.sin_family=AF_INET; //设置为IP通信  
	remote_addr.sin_addr.s_addr=inet_addr(SERV_ADDR);//服务器IP114.212.191.33
	//地址从转包TCP中获得 
	remote_addr.sin_port=htons(SERV_PORT); //服务器端口号  

	/*创建客户端套接字--IPv4协议，面向连接通信，TCP协议*/  
	if((client_sockfd=socket(PF_INET,SOCK_STREAM,0))<0)  
	{  
		perror("socket");  
		return 1;  
	}  

	/*将套接字绑定到服务器的网络地址上*/  
	if(connect(client_sockfd,(struct sockaddr *)&remote_addr,sizeof(struct sockaddr))<0)  
	{  
		perror("connect");  
		return 1;  
	}  
	printf("connected to server\n");  
	system("clear");
	/*循环的发送接收信息并打印接收信息--recv返回接收到的字节数，send返回发送的字节数*/  
	int status=0;	//0:begin
	while(1)  
	{  
		memset(sendbuf,0,MAXLINE);
		memset(recvbuf,0,MAXLINE);

		switch(status){
			case 0:		//开始状态
				printf("================================================\n");
				printf("Server IP:%s\n",SERV_ADDR);
				printf("Welcome to NJUCS Weather Forecast  Program!\n");
				printf("Please input City Name in Chinese pinyin (e.g. nanjing or beijing)\n");
				printf("(c)cls,(#)exit\n");
				printf("================================================\n");
				fgets(name,20,stdin);
				int namel=strlen(name);
				if(name[namel-1]=='\n')name[namel-1]=0;//去掉空格
				namel=strlen(name);
				if(namel<1){system("clear");break;}		//防止name为空
				if(namel>=19)//名字过长，清空缓存
				{
					printf("name is too long......\n");
					while((ch=getchar())!='\n'&&ch!=EOF);
					break;
				}
				if(strcmp(name,"c")==0){system("clear");status=0;break;}
				if(strcmp(name,"#")==0){status=1;break;}
				strcpy(&sendbuf[2],name);//根据数据包判断name应该放在何处
				sendbuf[0]=0x01;
				send(client_sockfd,sendbuf,33,0);//长度33，根据数据包
				if(recv(client_sockfd,recvbuf,MAXLINE,0)==0)
				{perror("server problem..\n");status=1;break;}
				//printf("%c\n",recvbuf[0]);	
				if(recvbuf[0]==0x04)	//返回信息表示没有该城市
				{printf("don't have this city..\n");status=0;break;}
				if(recvbuf[0]==0x03)	//有该城市信息
				{status=2;break;}
				break;
			case 1:exit(0);	//中止状态
			case 2:		//城市名称正确后
			       system("clear");
			       printf("Please enter the given number to query\n");
			       printf("1.today\n");
			       printf("2.three days from today\n");
			       printf("3.custom day by yourself\n");
			       printf("(r)back,(c)cls,(#)exit\n");
			       printf("==============================================\n");
			       status=3;break;
			case 3:				//选择某项服务
			       //setbuf(stdin, NULL);
			       choose=getchar();
			       if((ch=getchar())!='\n')//清空缓存
			       {
				       printf("your choice is illegal,please enter again..\n");
				       while((ch=getchar())!='\n'&&ch!=EOF);
				       break;
			       }
			       //printf("choose: %c\n",choose);
			       //if(scanf(" %1c",&choose)!=1)
			       //{printf("your choice is illegal,please enter again...\n");status=3;break;}
			       if(choose=='c'){status=2;break;}
			       if(choose=='r')
			       {status=0;system("clear");break;}
			       if(choose=='#')
			       {status=1;break;}
			       if(choose=='1'||choose=='2'||choose=='3')
			       {status=4;break;}
			       printf("your choice is illegal,please enter again..\n");
			       status=3;
			       break;
			case 4:		//当选择某项服务后
			       if(choose=='1')	//看今天天气
			       {
				       sendbuf[0]=0x02;
				       sendbuf[1]=0x01;
				       sendbuf[32]=0x01;
				       strcpy(&sendbuf[2],name);
				       send(client_sockfd,sendbuf,33,0);
				       if(recv(client_sockfd,recvbuf,MAXLINE,0)==0)
				       {perror("server problem..");status=1;break;}
				       if(recvbuf[0]==0x01&&recvbuf[1]==0x41)
				       {
					       printf("City: %s ",&recvbuf[2]);
					       printf("Today is: %d", recvbuf[32]*16*16+(unsigned char)recvbuf[33]);
					       printf("/%d/%d ",recvbuf[34],recvbuf[35]);
					       printf("Weather information is as follows:\n");
					       printf("Today’s Weather is: ");
					       select_weather(recvbuf[37]);
					       printf(" Temp: %d\n",recvbuf[38]);
				       }
			       }
			       else if(choose=='2')//3天信息
			       {
				       sendbuf[0]=0x02;
				       sendbuf[1]=0x02;
				       sendbuf[32]=0x03;
				       strcpy(&sendbuf[2],name);
				       send(client_sockfd,sendbuf,33,0);
				       if(recv(client_sockfd,recvbuf,MAXLINE,0)==0)
				       {perror("server problem..");status=1;break;}
				       if(recvbuf[0]==0x01&&recvbuf[1]==0x42)
				       {
					       printf("City: %s ",&recvbuf[2]);
					       printf("Today is: %d", recvbuf[32]*16*16+(unsigned char)recvbuf[33]);
					       printf("/%d/%d ",recvbuf[34],recvbuf[35]);
					       printf("Weather information is as follows:\n");
					       printf("The 1th day’s Weather is: ");
					       select_weather(recvbuf[37]);
					       printf("Temp: %d\n",recvbuf[38]);
					       printf("The 2th day’s Weather is:");
					       select_weather(recvbuf[39]);
					       printf("Temp: %d\n",recvbuf[40]);
					       printf("The 3th day’s Weather is: ");
					       select_weather(recvbuf[41]);
					       printf("Temp: %d\n",recvbuf[42]);
				       }
			       }
			       else if(choose=='3')//将来的某一天
			       {
				       int num;
				       printf("Please enter the day number(below 10, e.g. 1 means today):");
				       scanf("%1d",&num);
				       if((ch=getchar())!='\n')//清空缓存
				       {
					       printf("num is illegal..\n");
					       while((ch=getchar())!='\n'&&ch!=EOF);
					       break;
				       }
				       if(num<1||num>10)
				       {printf("number is illegal!\n");break;}
				       sendbuf[0]=0x02;
				       sendbuf[1]=0x01;
				       strcpy(&sendbuf[2],name);
				       sendbuf[32]=num;
				       send(client_sockfd,sendbuf,33,0);
				       if(recv(client_sockfd,recvbuf,MAXLINE,0)==0)
				       {perror("server problem..");status=1;break;}
				       if(recvbuf[0]==0x01&&recvbuf[1]==0x41)
				       {
					       printf("City: %s ",&recvbuf[2]);
					       printf("Today is: %d", recvbuf[32]*16*16+(unsigned char)recvbuf[33]);
					       printf("/%d/%d ",recvbuf[34],recvbuf[35]);
					       printf("Weather information is as follows:\n");
					       printf("The %dth day’s Weather is: ",num);
					       select_weather(recvbuf[37]);
					       printf("Temp: %d\n",recvbuf[38]);
				       }
				       else{
					       printf("don't have info of this day..\n");
				       }

			       }
			       status=3;break;


		}
	}  
	close(client_sockfd);//关闭套接字  
	return 0;  
}  
