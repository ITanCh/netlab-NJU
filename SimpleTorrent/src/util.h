
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

// 从一个已连接套接字接收数据的函数
int recvline(int fd, char **line);
int recvlinef(int fd, char *format, ...);

// 连接到另一台主机, 返回sockfd
int connect_to_host(char* ip, int port);

// 监听指定端口, 返回监听套接字
int make_listen_port(int port);

// 返回文件的长度, 单位为字节
int file_len(FILE* fname);

// 从torrent文件中提取数据
torrentmetadata_t* parsetorrentfile(char* filename);

// 从Tracker响应中提取有用的数据
tracker_response* preprocess_tracker_response(int sockfd);

// 从Tracker响应中提取peer连接信息
tracker_data* get_tracker_data(char* data, int len);
void get_peers(tracker_data* td, be_node* peer_list); // 上面函数的辅助函数
void get_peer_data(peerdata* peer, be_node* ben_res); // 上面函数的辅助函数

// 制作一个发送给Tracker的HTTP请求, 返回该字符串
char* make_tracker_request(int event, int* mlen);

// 处理来自peer的整数的辅助函数
int reverse_byte_orderi(int i);
int make_big_endian(int i);
int make_host_orderi(int i);

// ctrl-c信号的处理函数
void client_shutdown(int sig);

// 从announce url中提取主机和端口数据
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
