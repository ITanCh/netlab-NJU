//�ļ���: sip/sip.h
//
//����: ����ļ���������SIP���̵����ݽṹ�ͺ���  
//
//��������: 2013��

#ifndef NETWORK_H
#define NETWORK_H

//SIP����ʹ������������ӵ�����SON���̵Ķ˿�SON_PORT
//�ɹ�ʱ��������������, ���򷵻�-1
int connectToSON();

//����߳�ÿ��ROUTEUPDATE_INTERVALʱ��ͷ���һ��·�ɸ��±���
//�ڱ�ʵ����, ����߳�ֻ�㲥�յ�·�ɸ��±��ĸ������ھ�, 
//����ͨ������SIP�����ײ��е�dest_nodeIDΪBROADCAST_NODEID�����͹㲥
void* routeupdate_daemon(void* arg);

//����̴߳�������SON���̵Ľ��뱨��
//��ͨ������son_recvpkt()��������SON���̵ı���
//�ڱ�ʵ����, ����߳��ڽ��յ����ĺ�, ֻ����ʾ�����ѽ��յ�����Ϣ, ����������
void* pkthandler(void* arg); 

//���������ֹSIP����, ��SIP�����յ��ź�SIGINTʱ������������ 
//���ر���������, �ͷ����ж�̬������ڴ�
void sip_stop();
#endif
