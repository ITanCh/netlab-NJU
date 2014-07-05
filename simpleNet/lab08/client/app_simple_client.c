//�ļ���: client/app_simple_client.c
//
//����: ���Ǽ򵥰汾�Ŀͻ��˳������. �ͻ�������ͨ���ڿͻ��˺ͷ�����֮�䴴��TCP����,�����ص������. 
//Ȼ��������stcp_client_init()��ʼ��STCP�ͻ���. ��ͨ�����ε���stcp_client_sock()��stcp_client_connect()���������׽��ֲ����ӵ�������.
//��Ȼ��ͨ�����������ӷ���һ�ζ̵��ַ�����������. ����һ��ʱ���, �ͻ��˵���stcp_client_disconnect()�Ͽ���������������.
//���,�ͻ��˵���stcp_client_close()�ر��׽���. �ص������ͨ������son_stop()ֹͣ.

//��������: 2013��

//����: ��

//���: STCP�ͻ���״̬

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../common/constants.h"
#include "stcp_client.h"

//������������, һ��ʹ�ÿͻ��˶˿ں�87�ͷ������˿ں�88. ��һ��ʹ�ÿͻ��˶˿ں�89�ͷ������˿ں�90.
#define CLIENTPORT1 87
#define SERVERPORT1 88
#define CLIENTPORT2 89
#define SERVERPORT2 90

//�ڷ����ַ�����, �ȴ�5��, Ȼ��ر�����
#define WAITTIME 5

//�������ͨ���ڿͻ��ͷ�����֮�䴴��TCP�����������ص������. ������TCP�׽���������, STCP��ʹ�ø����������Ͷ�. ���TCP����ʧ��, ����-1.
int son_start() {
	char SERV_ADDR[15]="127.0.0.1";//"114.212.191.33";
	int sockfd;  
	struct sockaddr_in remote_addr; //�������������ַ�ṹ��  
	memset(&remote_addr,0,sizeof(remote_addr)); //���ݳ�ʼ��--����  
	remote_addr.sin_family=AF_INET; //����ΪIPͨ��  
	remote_addr.sin_addr.s_addr=inet_addr(SERV_ADDR);//������IP114.212.191.33
	//��ַ��ת��TCP�л�� 
	remote_addr.sin_port=htons(SON_PORT); //�������˿ں�  
	//			/*�����ͻ����׽���--IPv4Э�飬��������ͨ�ţ�TCPЭ��*/  
	if((sockfd=socket(PF_INET,SOCK_STREAM,0))<0)  
	{

		perror("son socket");  
		return -1;  

	}  

	/*���׽��ְ󶨵��������������ַ��*/  
	if(connect(sockfd,(struct sockaddr *)&remote_addr,sizeof(struct sockaddr))<0)  
	{

		perror("son connect");  
		return -1;  

	}  
	return sockfd;
}

//�������ͨ���رտͻ��ͷ�����֮���TCP������ֹͣ�ص������
void son_stop(int son_conn) {
	close(son_conn);
}

int main() {
	//���ڶ����ʵ����������
	srand(time(NULL));

	//�����ص�����㲢��ȡ�ص������TCP�׽���������	
	int son_conn = son_start();
	if(son_conn<0) {
		printf("fail to start overlay network\n");
		exit(1);
	}

	//��ʼ��stcp�ͻ���
	stcp_client_init(son_conn);

	//�ڶ˿�87�ϴ���STCP�ͻ����׽���, �����ӵ�STCP�������˿�88
	int sockfd = stcp_client_sock(CLIENTPORT1);
	if(sockfd<0) {
		printf("fail to create stcp client sock");
		exit(1);
	}
	if(stcp_client_connect(sockfd,SERVERPORT1)<0) {
		printf("fail to connect to stcp server\n");
		exit(1);
	}
	printf("client connected to server, client port:%d, server port %d\n",CLIENTPORT1,SERVERPORT1);
	
	//�ڶ˿�89�ϴ���STCP�ͻ����׽���, �����ӵ�STCP�������˿�90
	int sockfd2 = stcp_client_sock(CLIENTPORT2);
	if(sockfd2<0) {
		printf("fail to create stcp client sock");
		exit(1);
	}
	if(stcp_client_connect(sockfd2,SERVERPORT2)<0) {
		printf("fail to connect to stcp server\n");
		exit(1);
	}
	printf("client connected to server, client port:%d, server port %d\n",CLIENTPORT2, SERVERPORT2);

	//ͨ����һ�����ӷ����ַ���
	char mydata[6] = "hello";
	int i;
	for(i=0;i<5;i++){
		stcp_client_send(sockfd, mydata, 6);
		printf("send string:%s to connection 1\n",mydata);	
	}
	//ͨ���ڶ������ӷ����ַ���
    char mydata2[7] = "byebye";
	for(i=0;i<5;i++){
      	stcp_client_send(sockfd2, mydata2, 7);
		printf("send string:%s to connection 2\n",mydata2);	
      	}

	//�ȴ�һ��ʱ��, Ȼ��ر�����
	sleep(WAITTIME);

	if(stcp_client_disconnect(sockfd)<0) {
		printf("fail to disconnect from stcp server\n");
		exit(1);
	}
	if(stcp_client_close(sockfd)<0) {
		printf("fail to close stcp client\n");
		exit(1);
	}
	
	if(stcp_client_disconnect(sockfd2)<0) {
		printf("fail to disconnect from stcp server\n");
		exit(1);
	}
	if(stcp_client_close(sockfd2)<0) {
		printf("fail to close stcp client\n");
		exit(1);
	}

	//ֹͣ�ص������
	son_stop(son_conn);
}
