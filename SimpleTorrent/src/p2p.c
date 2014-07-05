#include "btdata.h"
#include "util.h"
#include <pthread.h>

/*
 * connnect to a peer
 * 
 */
typedef struct Handshake_
{
	char len;
	char name[19];
	char reserve[8];
	char sha[20];
	char id[20];
}Hand;

typedef struct Head_
{
	int len;
	char id;
}Head;
#define HEAD_SIZE 5	//由于字节对齐，导致sizeof(head)为8

typedef struct Request_
{
	int index;
	int offset;
	int len;
}Request;

typedef struct ReqState_
{
	int index;
	int offset;
	int len;
}ReqState;

typedef struct SaveState_
{
	int index;
	int offset;
}SaveState;

void sendRequest(peer_t* p,ReqState* state);
int needThis(char *map);
int havePiece(int index);
void sendPiece(int sockfd,int index,int offset,int len);
void savePiece(int index,int offset,char* h,int len,SaveState* s);
void setbit(char *c ,int y);
int getIndex(peer_t * peer);
void showPiece(int index);
void publicCancel(int index);
void publicRequest(int index);

void *waitPeer(void *arg)
{
	int sockfd , newsockfd;
	int rc;
	struct sockaddr_in peer_addr;
	int peer_len=sizeof(peer_addr);
	sockfd=make_listen_port(g_peerport);	//写完了才看见这个函数，后悔啊。。。。

	printf("start to wait peet to connect me\n");
	while(!g_done)
	{
		newsockfd = accept(sockfd,(struct sockaddr *)&peer_addr, (socklen_t *)&peer_len);
		if(newsockfd<0)
		{
			perror("accept error\n");
			exit(1);
		}
		pthread_mutex_lock(&all_peer_lock);
		int pi;
		for(pi=0;pi<MAXPEER;pi++)
		{
			if(myPeer[pi].sockfd==newsockfd)break;
		}
		if(pi<MAXPEER)
		{
			pthread_mutex_unlock(&all_peer_lock);
			continue;	//重复的sockfd
		}
		for(pi=0;pi<MAXPEER;pi++)
		{
			if(myPeer[pi].sockfd==-1)break;
		}
		if(pi>=MAXPEER)
		{
			pthread_mutex_unlock(&all_peer_lock);
			shutdown(newsockfd,SHUT_RDWR);
			close(newsockfd);
			continue;		//peer 已满
		}
		myPeer[pi].sockfd=newsockfd;
		pthread_mutex_unlock(&all_peer_lock);

		myPeer[pi].choking=0;
		myPeer[pi].interested=1;
		myPeer[pi].choked=0;
		myPeer[pi].have_interest=1;
		if(myPeer[pi].bitmap!=NULL)
			free(myPeer[pi].bitmap);
		myPeer[pi].bitmap=NULL;
		myPeer[pi].next=NULL;
		myPeer[pi].state=0;

		pthread_t thr;
		rc=pthread_create(&thr,NULL,PToP,(void*)(myPeer+pi));	//二流合一
		if(rc)
			printf("wait peer create thread error");
	}
	return NULL;
}

void *PToP(void *arg)
{
	Head* head;
	peer_t * peer=(peer_t *)arg;
	peer->bitmap=malloc(mapcount);		//记录该peer的bitfield
	memset(peer->bitmap,0,mapcount);

	int sockfd=peer->sockfd;
	int i;
	char rcvline[MAXLINE];
	char sendline[MAXLINE];
	char templine[MAXLINE];
	char c;
	memset(rcvline,0,MAXLINE);
	memset(sendline,0,MAXLINE);

	//啥话不说，先来一发
	Hand* hk=(Hand*)sendline;
	hk->len=0x13;
	strcpy(hk->name,"BitTorrent protocol");
	memset(hk->reserve,0,8);
	int hash[5];
	for(i=0;i<5;i++)
		hash[i]=reverse_byte_orderi(g_infohash[i]);
	memcpy(hk->sha,hash,20);
	memcpy(hk->id,g_my_id,20);
	sendme(sockfd,sendline,sizeof(Hand));

	if(recvme(sockfd,rcvline,sizeof(Hand))<=0)
	{
		printf("握手失败\n");
		{
			shutdown(sockfd,SHUT_RDWR);
			close(sockfd);
			peer->sockfd=-1;
			return NULL;
		}
	}
	hk=(Hand*)rcvline;
	/*if(memcmp(hash,hk->sha,20)!=0){
		printf("hash 不匹配\n");
		shutdown(sockfd,SHUT_RDWR);
		close(sockfd);
		peer->sockfd=-1;
		return NULL;
	}*/
	memcpy(peer->name,hk->id,20);		

	//bitfield
	pthread_mutex_lock(&g_isSeed_lock);
	i=g_isSeed;
	pthread_mutex_unlock(&g_isSeed_lock);
	if(i==2)
	{
		memset(sendline,0,MAXLINE);
		head=(Head*)sendline;
		head->len=reverse_byte_orderi(mapcount+1);
		head->id=0x05;
		pthread_mutex_lock(&g_bitmap_lock);
		memcpy(&sendline[HEAD_SIZE],g_bitmap,mapcount);
		pthread_mutex_unlock(&g_bitmap_lock);
		sendme(sockfd,sendline,HEAD_SIZE+mapcount);
	}

	Request* req;
	int temp;
	ReqState reqState;
	reqState.index=-1;
	reqState.offset=-1;
	reqState.len=-1;
	SaveState saveState;
	saveState.index=-1;
	saveState.offset=0;

	int cancel=-1;
	int errcount=0;
	while(!g_done)
	{
		req=NULL;
		memset(rcvline,0,MAXLINE);
		memset(sendline,0,MAXLINE);
		memset(templine,0,MAXLINE);
		head=NULL;
		i=recvme(sockfd,rcvline,sizeof(int));
		if(peer->state||i<=0)
		{
			errcount++;
			if(errcount>1)
			{
				printf("接受信息错误%d\n",peer->state);
				shutdown(sockfd,SHUT_RDWR);
				close(sockfd);
				peer->sockfd=-1;
				return NULL;
			}
			continue;
		}
		errcount=0;
		head=(Head*)rcvline;
		if(reverse_byte_orderi(head->len)<=0)continue;	//keep alive	
		if(recvme(sockfd,rcvline+sizeof(int),1)<0)continue;
		int load_len=reverse_byte_orderi(head->len)-1;
		//printf("状态:%02X\n",head->id);
		switch(head->id)
		{
			case 0x00:			//choke
				printf("被阻塞!\n");
				head=(Head*)sendline;
				head->len=reverse_byte_orderi(1);
				head->id=0x02;
				peer->have_interest=1;
				sendme(sockfd,sendline,HEAD_SIZE);
				break;
			case 0x01:			//unchoke
				if(!peer->have_interest)break;
				peer->choked=0;
				sendRequest(peer,&reqState);	//send request
				break;
			case 0x02:		//interest
				if(peer->choking)break;
				cancel=-1;
				peer->interested=1;
				head=(Head*)sendline;
				head->len=reverse_byte_orderi(1);
				head->id=0x01;		//unchoke
				sendme(sockfd,sendline,HEAD_SIZE);
				//等待对方的request
				break;
			case 0x03:		//not interested
				cancel=-1;
				break;
			case 0x04:		//have
				if(recvme(sockfd,templine,load_len)<=0)
					continue;
				memcpy(&temp,templine,sizeof(int));
				temp=reverse_byte_orderi(temp);
				pthread_mutex_lock(&g_bitmap_lock);
				c=g_bitmap[temp/8];
				pthread_mutex_unlock(&g_bitmap_lock);
				if(isSet(c,8-temp%8))continue;
				head=(Head*)sendline;
				head->len=reverse_byte_orderi(1);
				head->id=0x02;
				sendme(sockfd,sendline,HEAD_SIZE);
				break;
			case 0x05:		//bitfield
				if(recvme(sockfd,templine,load_len)<=0)
					continue;
				if(load_len!=mapcount)
				{
					shutdown(sockfd,SHUT_RDWR);
					close(sockfd);
					peer->sockfd=-1;
					return NULL;
				}
				memcpy(peer->bitmap,templine,mapcount);
				i=needThis(peer->bitmap);	//判断是否有需要该用户,需要记录该用户提供了那些分片
				//printf("是否需要该用户%d\n",i);
				if(i==0)
				{
					shutdown(sockfd,SHUT_RDWR);
					close(sockfd);
					peer->sockfd=-1;
					return NULL;
				}
				else if(i==1)break;		//不感兴趣
				//send interest
				head=(Head*)sendline;
				head->len=reverse_byte_orderi(1);
				head->id=0x02;
				peer->have_interest=1;
				sendme(sockfd,sendline,HEAD_SIZE);
				break;
			case 0x06:		//request
				if(recvme(sockfd,templine,load_len)<=0)
					continue;
				req=(Request *)templine;
				if(reverse_byte_orderi(req->len)>131072)	//2^17
				{
					shutdown(sockfd,SHUT_RDWR);
					close(sockfd);
					peer->sockfd=-1;
					return NULL;
				}
				if(cancel==reverse_byte_orderi(req->index)){
					cancel=-1;
					continue;
				}
				sendPiece(sockfd,req->index,req->offset,req->len);	//发送子分片
				break;
			case 0x07:		//piece
				if(recvme(sockfd,templine,load_len)<=0)
					continue;
				//printf("load len :%d\n",i);
				req=(Request *)templine;
				/*for(i=0;i<8;i++)
				  {
				  for(temp=0;temp<8;temp++)
				  printf("%02X ",templine[i*8+temp]);
				  printf("\n");
				  }*/
				savePiece(reverse_byte_orderi(req->index),reverse_byte_orderi(req->offset),templine+2*sizeof(int),load_len-2*sizeof(int),&saveState);	//存储分片:w
				sendRequest(peer,&reqState);
				break;
			case 0x08:		//cancel
				if(recvme(sockfd,templine,load_len)<=0)continue;
				req=(Request*)templine;
				cancel=reverse_byte_orderi(req->index);
				break;
			default:
				;//printf("没有该状态%02X\n",head->id);return NULL;

		}
	}
	shutdown(sockfd,SHUT_RDWR);
	close(sockfd);
	peer->sockfd=-1;
	return NULL;
}


/*发送请求函数，一次一般发送５个请求*/
void sendRequest(peer_t *peer,ReqState* state)
{
	int i=0;
	int r;
	char sendbuf[MAXLINE];
	//pthread_mutex_lock(&g_isSeed_lock);		//做种后不需要再存分片
	//i=g_isSeed;
	//pthread_mutex_unlock(&g_isSeed_lock);
	//if(i==2)return;

	//int size=0;
	while(i<1)
	{
		memset(sendbuf,0,MAXLINE);
		if(state->index<0)		//还未分配给该连接需要发送分片
		{
			state->index=getIndex(peer);		//选择分片，该连接负责该分片，直到该分片完成
			if(state->index<0)
			{
				//printf("没有请求可以发送\n");
				break;			//没有需要发送请求的分片
			}
			if(state->index==g_num_pieces-1)	//最后一片，需要特殊处理
				state->len=g_filelen-(g_num_pieces-1)*g_torrentmeta->piece_len;
			else
				state->len=g_torrentmeta->piece_len;
			state->offset=0;
			printf("请求分片：index:%d len:%d\n",state->index,state->len);
		}

		Head* h=(Head*)sendbuf;
		h->len=reverse_byte_orderi(13);
		h->id=0x06;
		Request * req=(Request*)(sendbuf+HEAD_SIZE);
		req->index=reverse_byte_orderi(state->index);
		req->offset=reverse_byte_orderi(state->offset);
		req->len=16384;		//16k
		if(state->offset+req->len>g_piece_state[state->index].len)	//最后一片
			req->len=g_piece_state[state->index].len-state->offset;
		printf("send request sock:%d :index: %d  offset: %d  len:%d\n",peer->sockfd,state->index,state->offset,req->len);
		state->offset+=req->len;
		req->len=reverse_byte_orderi(req->len);
		//printf("%d\n",req->len);
		r=sendme(peer->sockfd,sendbuf,17);
		if(r<=0)
			printf("req send error\n");
		if(state->offset>=g_piece_state[state->index].len)	//该分片请求已经发完
			state->index=-1;
		i++;
	}
}

/*判断是否需要该peer*/
int needThis(char *map)
{
	int i;
	pthread_mutex_lock(&g_isSeed_lock);
	i=g_isSeed;
	pthread_mutex_unlock(&g_isSeed_lock);
	pthread_mutex_lock(&g_bitmap_lock);
	if(i==2)
	{
		if(memcmp(g_bitmap,map,mapcount)==0)		//两者都为做种者,可以断开连接
		{
			pthread_mutex_unlock(&g_bitmap_lock);
			return 0;
		}
		else
		{
			pthread_mutex_unlock(&g_bitmap_lock);
			return 1;					//对对方不感兴趣
		}
	}
	if(memcmp(g_bitmap,map,mapcount)==0)	//皆为吸血者，但是没有需要的
	{
		pthread_mutex_unlock(&g_bitmap_lock);
		return 1;	
	}
	pthread_mutex_unlock(&g_bitmap_lock);
	//感兴趣,统计信息
	if(g_piece_state==NULL)
	{
		printf("piece state error\n");
		return 0;
	}
	for(i=0;i<g_num_pieces;i++)
	{
		int c_no=i/8;
		char c=map[c_no];
		int bit_no=8-i%8;
		if(isSet(c,bit_no))
		{
			pthread_mutex_lock(&g_bitmap_lock);
			g_piece_state[i].count++;
			pthread_mutex_unlock(&g_bitmap_lock);
		}
	}
	return 2;			
}

int isSet(char c,int place)
{
	int b=1&(c>>(place-1));
	return b;
}

void setbit(char *x,int y) 
{
	(*x)|=(1<<(y-1)); //将X的第Y位置1
}

int havePiece(int index)
{
	int i;
	pthread_mutex_lock(&g_isSeed_lock);
	i=g_isSeed;
	pthread_mutex_unlock(&g_isSeed_lock);
	if(i==2)return 1;
	pthread_mutex_lock(&g_bitmap_lock);
	g_piece_state[index].count++;
	if(g_piece_state[index].isDone)
	{
		pthread_mutex_unlock(&g_bitmap_lock);
		return 1;
	}
	pthread_mutex_unlock(&g_bitmap_lock);
	return 0;
}

void sendPiece(int sockfd,int index,int offset,int len)
{
	int i=reverse_byte_orderi(index);
	int o=reverse_byte_orderi(offset);
	int l=reverse_byte_orderi(len);
	int size=l+HEAD_SIZE+2*sizeof(int);
	char sendline[MAXLINE];	
	Head *head=(Head*)sendline;
	head->len=reverse_byte_orderi(size-sizeof(int));
	head->id=0x07;
	Request *req=(Request*)(sendline+HEAD_SIZE);
	req->index=index;
	req->offset=offset;
	pthread_mutex_lock(&g_f_lock);
	fseek(g_f,i*g_torrentmeta->piece_len+o,SEEK_SET);
	fread(sendline+HEAD_SIZE+2*sizeof(int),l,1,g_f);
	pthread_mutex_unlock(&g_f_lock);
	//memcpy(sendline+HEAD_SIZE+2*sizeof(int),g_filedata+i*g_torrentmeta->piece_len+o,l);
	printf("send piece:sock:%d  index %d  offset %d len %d\n",sockfd,i,o,l);
	int r=sendme(sockfd,sendline,size);
	g_uploaded+=l;
	if(r<=0)return;
}

void savePiece(int index,int offset,char* h,int len,SaveState* state)
{
	int i;
	pthread_mutex_lock(&g_isSeed_lock);		//做种后不需要再存分片
	i=g_isSeed;
	pthread_mutex_unlock(&g_isSeed_lock);
	if(i==2)return;

	if(state->index<0||offset==0)
	{
		if(offset!=0)return;
		state->index=index;
		state->offset=offset;
	}
	if(len>16384)
	{
		printf("收取数据过大:%d\n",len);
		pthread_mutex_lock(&g_bitmap_lock);
		g_piece_state[state->index].sockfd=-1;
		pthread_mutex_unlock(&g_bitmap_lock);
		state->index=-1;
		return;
	}
	if(index==state->index&&offset==state->offset)
	{
		printf("save piece: index: %d  offset: %d  len: %d\n",index,offset,len);
		if(index*g_torrentmeta->piece_len+offset+len>g_filelen)
		{
			printf("长度越界%d\n",index*g_torrentmeta->piece_len+offset+len);
			return;
		}
		pthread_mutex_lock(&g_f_lock);
		fseek(g_f,index*g_torrentmeta->piece_len+offset,SEEK_SET);
		if((i=fwrite(h,len,1,g_f))!=1)
		{
			printf("写入文件错误%d\n",i);
			pthread_mutex_unlock(&g_f_lock);
			exit(-1);
		}
		g_downloaded+=len;
		g_left-=1;
		pthread_mutex_unlock(&g_f_lock);
		//memcpy(g_filedata+index*g_torrentmeta->piece_len+offset,h,len);

		state->offset+=len;
		if(state->offset >= g_piece_state[index].len)	//该片已经完成
		{
			printf("==完成分片:%d==\n",state->index);
			state->index=-1;
			pthread_mutex_lock(&g_bitmap_lock);
			setbit(&g_bitmap[index/8],8-index%8);
			g_piece_state[index].isDone=1;
			pthread_mutex_unlock(&g_bitmap_lock);
			pthread_mutex_lock(&g_f_lock);
			fflush(g_f);
			pthread_mutex_unlock(&g_f_lock);
			showPiece(index);	//告诉所有人我有了该分片
			//检查是否所有的分片都已经完成
			int i=0;
			int count=0;
			pthread_mutex_lock(&g_bitmap_lock);
			for(i=0;i<g_num_pieces;i++)
			{
				if(!g_piece_state[i].isDone)
				{
					count++;
					printf("0");
				}
				else printf("1");
			}
			printf("\n");
			pthread_mutex_unlock(&g_bitmap_lock);
			if(count > 0 && count <= g_num_pieces/100 && count <= 20)	//1/100进入最后阶段
			{
				if(end_game_mode == 0){
					end_game_mode = 1;
					pthread_mutex_lock(&g_bitmap_lock);
					//最后阶段，向所有peer发送未完成的分片请求
					printf("Enter Last_Game Mode ...\n");
					for(i = 0;i < g_num_pieces;i++){
						printf("[Last_Game] Public request for Piece:%d \n",i);
						if(!g_piece_state[i].isDone){
							publicRequest(i);
						}
					}
					pthread_mutex_unlock(&g_bitmap_lock);
				}else{
					//最后阶段模式下，当收到一个完整的块，向其他peer发送cancel消息
					publicCancel(index);
				}
			}
			if(count==0)		//全部完成
			{
				printf("!!!!接收完毕，开始做种!!!!\n");
				pthread_mutex_lock(&g_isSeed_lock);
				g_isSeed=2;
				pthread_mutex_unlock(&g_isSeed_lock);
			}
		}
	}
	else		//子该分片顺序错误,放弃该分片上的数据
	{
		if(state->index!=index)return;		//其它分片	可以忽略
		pthread_mutex_lock(&g_bitmap_lock);
		g_piece_state[state->index].sockfd=-1;
		pthread_mutex_unlock(&g_bitmap_lock);
		state->index=-1;
		printf("子分片乱序\n");
	}
}

/*根据最少优先策略选择下载分片*/
int getIndex(peer_t * peer)
{
	int i;
	int index=-1;
	int count=-1;
	pthread_mutex_lock(&g_isSeed_lock);
	if(g_isSeed==0)
	{
		g_isSeed=1;
		pthread_mutex_unlock(&g_isSeed_lock);
		int random;
		while(1)
		{
			random=rand()%g_num_pieces;		//第一片随即
			if(isSet(peer->bitmap[random/8],8-random%8))break;
		}
		return random;
	}
	else pthread_mutex_unlock(&g_isSeed_lock);
	pthread_mutex_lock(&g_bitmap_lock);
	for(i=0;i<g_num_pieces;i++)
	{
		char c=peer->bitmap[i/8];
		int bit_no=8-i%8;
		//printf("%d %d %d %d\n",i,g_piece_state[i].isDone,isSet(c,bit_no),g_piece_state[i].sockfd);
		/*if(g_piece_state[i].isDone!=isSet(g_bitmap[i/8],bit_no))
		  {
		  printf("bit field 不一致\n");
		  return -1;
		  }*/
		if(!g_piece_state[i].isDone&&isSet(c,bit_no)&&g_piece_state[i].sockfd<0)
		{
			if(g_piece_state[i].count<count||count<0)
			{
				index=i;
				count=g_piece_state[i].count;
			}
		}
	}
	if(index<0)		//再搜寻一遍，看是否有卡住的分片
	{
		for(i=0;i<g_num_pieces;i++)
		{
			char c=peer->bitmap[i/8];
			int bit_no=8-i%8;
			if(!g_piece_state[i].isDone&&isSet(c,bit_no))
			{
				if(g_piece_state[i].count<count||count<0)
				{
					index=i;
					count=g_piece_state[i].count;
				}
			}
		}
	}
	if(index>=0)g_piece_state[index].sockfd=peer->sockfd;
	pthread_mutex_unlock(&g_bitmap_lock);
	return index;
}

/*告诉所有人我有了某个分片*/
void showPiece(int index)
{
	char temp[9];
	int len=reverse_byte_orderi(5);
	char id=0x04;
	int ind=reverse_byte_orderi(index);
	memcpy(temp,&len,sizeof(int));
	memcpy(temp+sizeof(int),&id,1);
	memcpy(temp+sizeof(int)+1,&ind,sizeof(int));
	pthread_mutex_lock(&all_peer_lock);
	int i;
	for(i=0;i<MAXPEER;i++)
	{
		if(myPeer[i].sockfd>=0)
			sendme(myPeer[i].sockfd,temp,9);
	}
	pthread_mutex_unlock(&all_peer_lock);
}

/*向所有人发送某个分片的Request*/
void publicRequest(int index){
	ReqState* reqstate = malloc(sizeof(ReqState));
	reqstate->index = index;
	reqstate->offset = -1;
	reqstate->len = -1;

	peer_t *p = all_peer;
	while(p != NULL){
		sendRequest(p,reqstate);
		p = p->next;
	}	
}

/*向所有人发送某个分片的cancel*/
void publicCancel(int index){
	char temp[17];
	int len = reverse_byte_orderi(13);
	char id = 0x08;
	int ind = reverse_byte_orderi(index);
	int begin = reverse_byte_orderi(0);
	int length = reverse_byte_orderi(0);
	memcpy(temp,&len,4);
	memcpy(temp+4,&id,1);
	memcpy(temp+5,&ind,4);
	memcpy(temp+9,&begin,4);
	memcpy(temp+13,&length,4);
	pthread_mutex_lock(&all_peer_lock);
	peer_t *p = all_peer;
	while(p != NULL)
	{
		sendme(p->sockfd,temp,17);
		p = p->next;
	}
	pthread_mutex_unlock(&all_peer_lock);
}
