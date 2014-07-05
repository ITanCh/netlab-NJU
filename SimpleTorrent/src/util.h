
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#include "btdata.h"
#include "bencode.h"

#ifndef UTIL_H
#define UTIL_H

#define MAXLINE 16416

int is_bigendian();
void printBit(char c);

// ��һ���������׽��ֽ������ݵĺ���
int recvline(int fd, char **line);
int recvlinef(int fd, char *format, ...);

// ���ӵ���һ̨����, ����sockfd
int connect_to_host(char* ip, int port);

// ����ָ���˿�, ���ؼ����׽���
int make_listen_port(int port);

// �����ļ��ĳ���, ��λΪ�ֽ�
int file_len(FILE* fname);

// ��torrent�ļ�����ȡ����
torrentmetadata_t* parsetorrentfile(char* filename);

// ��Tracker��Ӧ����ȡ���õ�����
tracker_response* preprocess_tracker_response(int sockfd);

// ��Tracker��Ӧ����ȡpeer������Ϣ
tracker_data* get_tracker_data(char* data, int len);
void get_peers(tracker_data* td, be_node* peer_list); // ���溯���ĸ�������
void get_peer_data(peerdata* peer, be_node* ben_res); // ���溯���ĸ�������

// ����һ�����͸�Tracker��HTTP����, ���ظ��ַ���
char* make_tracker_request(int event, int* mlen);

// ��������peer�������ĸ�������
int reverse_byte_orderi(int i);
int make_big_endian(int i);
int make_host_orderi(int i);

// ctrl-c�źŵĴ�����
void client_shutdown(int sig);

// ��announce url����ȡ�����Ͷ˿�����
announce_url_t* parse_announce_url(char* announce);

//p2p
void * PToP(void *arg);
void * waitPeer(void *arg);
int recvme(int sockfd,char * line,int len);
int sendme(int sockfd,char *line,int len);
int isSet(char c,int place);
void addPeer(peer_t* p);
void del_all_peer();
#endif
