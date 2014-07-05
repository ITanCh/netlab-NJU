#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include "util.h"
#include "btdata.h"
#include "bencode.h"

//#define MAXLINE 4096 
// pthread数据

peer_t myPeer[MAXPEER];	//最多5个peer

void init()
{
	g_done = 0;
	g_tracker_response = NULL;
	end_game_mode = 0;
	pthread_mutex_init(&all_peer_lock, NULL);
	pthread_mutex_init(&g_f_lock,NULL);
	pthread_mutex_init(&g_bitmap_lock,NULL);
	pthread_mutex_init(&g_isSeed_lock,NULL);
}

void addPeer(peer_t* p)
{
	pthread_mutex_lock(&all_peer_lock);
	if(all_peer==NULL)
		all_peer=p;
	else
	{
		peer_t *h=all_peer;
		while(h->next!=NULL)
			h=h->next;
		h->next=p;
	}
	pthread_mutex_unlock(&all_peer_lock);
}

void del_all_peer()
{
	pthread_mutex_lock(&all_peer_lock);
	int i=0;
	while(i<5)
	{
		close(myPeer[i].sockfd);
		myPeer[i].state=1;
		i++;
	}
	sleep(2);
	pthread_mutex_unlock(&all_peer_lock);
}

void init_piece_state()
{
	int i;
	for(i=0;i<g_num_pieces;i++)
	{
		g_piece_state[i].sockfd=-1;
		g_piece_state[i].count=0;
		char c=g_bitmap[i/8];
		int no=8-i%8;
		//printf("init_piece:%d\n",isSet(c,no)); if(isSet(c,no))
		if(isSet(c,no))
			g_piece_state[i].isDone=1;
		else
		{
			g_piece_state[i].isDone=0;
			g_piece_state[i].sockfd=-1;
		}
		printf("%d", g_piece_state[i].isDone);
		if(i==g_num_pieces-1)	//最后一片，需要特殊处理
			g_piece_state[i].len=g_filelen-(g_num_pieces-1)*g_torrentmeta->piece_len;
		else
			g_piece_state[i].len=g_torrentmeta->piece_len;
	}
}

int main(int argc, char **argv) 
{
	int sockfd = -1;
	char rcvline[MAXLINE];
	int rc;
	int i;
	g_isSeed=0;

	int resume_file_count = 0;
	// 注意: 你可以根据自己的需要修改这个程序的使用方法
	if(argc < 5)
	{
		printf("Usage: SimpleTorrent <torrent file> <ip of this machine (XXX.XXX.XXX.XXX form)> <port of this machine> [isSeed?]\n");
		printf("\t isseed is optional, 2 indicates this is a seed and won't contact other clients\n");
		printf("\t isseed is optional, 1 indicates this is semi-finished\n");
		printf("\t 0 indicates a downloading client and will connect to other clients and receive connections\n");
		exit(-1);
	}

	init();
	srand(time(NULL));

	int val[5];
	for(i=0; i<5; i++)
	{
		val[i] = rand();
	}
	//设置自己的端口号
	g_peerport=atoi(argv[3]);
	g_isSeed=atoi(argv[4]);
	//生成自己的ID
	memcpy(g_my_id,(char*)val,20);	
	strncpy(g_my_ip,argv[2],strlen(argv[2]));
	g_my_ip[strlen(argv[2])+1] = '\0';

	//获取torrent
	g_torrentmeta = parsetorrentfile(argv[1]);
	memcpy(g_infohash,g_torrentmeta->info_hash,20);
	g_filelen = g_torrentmeta->length;
	g_num_pieces = g_torrentmeta->num_pieces;
	g_left=g_filelen;
	g_uploaded=0;	//欺骗tracker
	g_downloaded=0;


	//创建恢复文件 format: xx.torrent.bf
	strncpy(resume_file,argv[1],strlen(argv[1]));
	strcat(resume_file,".bf");

	//文件相关
	printf("file name : %s\n",g_torrentmeta->name);
	if(g_isSeed==0)
		g_f=fopen(g_torrentmeta->name,"w+");	//change fopen MODE from "w+" to "a+", 防止打开断点文件时清空文件，应该继续写入
	else
		g_f=fopen(g_torrentmeta->name,"r+"); 
	if(g_f==NULL)
	{
		printf("open file error\n");
		exit(1);
	}

	if(g_isSeed==0)
	{
		fseek(g_f,0,SEEK_SET);
		for(i=0,rc=0;i<g_filelen;i++)
		{
			char zero=0x0;
			if(fwrite(&zero,1,1,g_f)!=1)
			{
				printf("文件初始化错误\n");
				exit(-1);
			};
			rc++;
			if(rc>MAXLINE)
			{
				fflush(g_f);
				rc=0;
			}
		}
		fflush(g_f);
	}
	/*g_filedata=NULL;
	  g_filedata = malloc(g_filelen);
	  if(g_filedata==NULL)
	  printf("分配失败size %d\n",g_filelen);*/

	//bitfield
	mapcount=g_num_pieces/8;
	int remind=8-g_num_pieces%8;
	if(remind)mapcount++;
	g_bitmap=malloc(mapcount);
	memset(g_bitmap,0,mapcount);
	if(g_isSeed==2)
	{
		for(i=0;i<mapcount;i++)
			g_bitmap[i]=0xff;
		//printf("========================g_map:%02X\n",g_bitmap[5]);
		g_bitmap[mapcount-1]<<=remind;
	}
	else if(g_isSeed==1)
	{
		//断点续传需要读入文件到bitmap中
		f_resume_from = fopen(resume_file,"r+");
		if(!f_resume_from){
			printf("Can not find last data file\n");
			exit(1);
		}
		resume_file_count = 0;
		printf("%s contents: \n",resume_file);
		while(!feof(f_resume_from)){
			fscanf(f_resume_from,"%c",&g_bitmap[resume_file_count]);
			//printf("%02x ",g_bitmap[resume_file_count]);
			printBit(g_bitmap[resume_file_count]);
			resume_file_count++;
		}
		printf("\nDone!\n");
	}
	g_piece_state=malloc(sizeof(piece_t)*g_num_pieces);
	init_piece_state();

	announce_url_t* announce_info;
	announce_info = parse_announce_url(g_torrentmeta->announce);
	// 提取tracker url中的IP地址
	printf("HOSTNAME: %s\n",announce_info->hostname);
	struct hostent *record;
	record = gethostbyname(announce_info->hostname);
	if (record==NULL)
	{ 
		printf("gethostbyname failed");
		exit(1);
	}
	struct in_addr* address;
	address =(struct in_addr * )record->h_addr_list[0];
	printf("Tracker IP Address: %s\n", inet_ntoa(* address));
	strcpy(g_tracker_ip,inet_ntoa(*address));
	g_tracker_port = announce_info->port;

	if(announce_info!=NULL)
	{
		free(announce_info->hostname);
		free(announce_info);
		announce_info = NULL;
	}

	// 初始化
	// 设置信号句柄
	signal(SIGINT,client_shutdown);

	// 设置监听peer的线程,老师留的线索
	pthread_t wait_thr;
	rc=pthread_create(&wait_thr,NULL,waitPeer,NULL);
	if(rc)
		printf("wait peer thread error\n");

	// 定期联系Tracker服务器
	int firsttime = 1;
	int mlen;
	char* MESG;
	MESG = make_tracker_request(BT_STARTED,&mlen);
	tracker_response* tr=NULL;
	for(i=0;i<MAXPEER;i++)
	{
		myPeer[i].sockfd=-1;
	}
	while(!g_done)
	{
		sockfd=-1;
		if(sockfd <= 0)
		{
			//创建套接字发送报文给Tracker
			printf("Creating socket to tracker...\n");
			sockfd = connect_to_host(g_tracker_ip, g_tracker_port);
		}

		printf("Sending request to tracker...\n");

		if(!firsttime)
		{
			free(MESG);
			// -1 指定不发送event参数
			MESG = make_tracker_request(-1,&mlen);
			printf("MESG: ");
			for(i=0; i<mlen; i++)
				printf("%c",MESG[i]);
			printf("\n");
		}
		send(sockfd, MESG, mlen, 0);
		memset(rcvline,0x0,MAXLINE);

		// 读取并处理来自Tracker的响应
		if(tr!=NULL)
		{
			if(tr->data!=NULL)
			{
				free(tr->data);
				tr->data=NULL;
			}
			free(tr);
			tr=NULL;
		}
		tr = preprocess_tracker_response(sockfd); 

		// 关闭套接字, 以便再次使用
		shutdown(sockfd,SHUT_RDWR);
		close(sockfd);
		if(tr!=NULL||firsttime)
		{
			firsttime = 0;
			printf("Decoding response...\n");
			char* tmp2 = (char*)malloc(tr->size*sizeof(char));
			memcpy(tmp2,tr->data,tr->size*sizeof(char));
			//free(tr->data);
			//tr->data=NULL; printf("Parsing tracker data\n");
			g_tracker_response = get_tracker_data(tmp2,tr->size);

			if(tmp2!=NULL)
			{
				free(tmp2);
				tmp2 = NULL;
			}

			printf("Num Peers: %d\n",g_tracker_response->numpeers);
			for(i=0; i<g_tracker_response->numpeers; i++)
			{
				//printf("Peer id: %d\n",g_tracker_response->peers[i].id);
				//a new connect to this peer will be create 
				if(strcmp(g_tracker_response->peers[i].ip,g_my_ip)==0)
				{
					printf("不必连接自己\n");
					continue;
				}
				sockfd=-1;
				sockfd=connect_to_host(g_tracker_response->peers[i].ip,g_tracker_response->peers[i].port);	
				if(sockfd<=0)
				{
					printf("Cannnot cnonnect to Peer ip: %s|\n",g_tracker_response->peers[i].ip);
					continue;
				}
				printf("Peer ip: %s|\n",g_tracker_response->peers[i].ip);
				printf("Peer port: %d \nsock:%d\n",g_tracker_response->peers[i].port,sockfd);
				pthread_mutex_lock(&all_peer_lock);
				int pi;
				for(pi=0;pi<MAXPEER;pi++)
				{
					if(myPeer[pi].sockfd==sockfd)break;
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
					shutdown(sockfd,SHUT_RDWR);
					close(sockfd);
					break;		//peer 已满
				}
				myPeer[pi].sockfd=sockfd;
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
				//for(idl=0; idl<20; idl++)
				//{
				//	myPeer->name[idl]=g_tracker_response->peers[i].id[idl];
				//	printf("%x ",(unsigned char)g_tracker_response->peers[i].id[idl]);
				//}
				//printf("\n");
				//create new thread for every peer
				pthread_t thread;
				rc=pthread_create(&thread,NULL,PToP,(void*)(myPeer+pi));
				if(rc)
					printf("connect peer create thread error\n");
			}

		}
		// 必须等待td->interval秒, 然后再发出下一个GET请求
		sleep(120);//g_tracker_response->interval);
		//init_piece_state();
		//del_all_peer();
	}

	// 睡眠以等待其他线程关闭它们的套接字, 只有在用户按下ctrl-c时才会到达这里
	sleep(2);
	exit(0);
}


