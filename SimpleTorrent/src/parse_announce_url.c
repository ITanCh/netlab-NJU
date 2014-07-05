#include "util.h"
#include "btdata.h"

announce_url_t* parse_announce_url(char* announce)
{
	char* announce_ind;
	printf("%s\n",announce);
	char port_str[6];  // 端口号最大为5位数字
	int port_len = 0; // 端口号中的字符数
	int port;
	int url_len = 0;
	announce_ind = strstr(announce,"/announce");
	announce_ind--;
	while(announce_ind!=NULL&&*announce_ind!= ':')
	{
		port_len++;
		announce_ind--;
	}
	strncpy(port_str,announce_ind+1,port_len);
	port_str[port_len] = '\0';
	port = atoi(port_str);
	if(port==0)
	{
		port=6768;
		announce_ind = strstr(announce,"/announce");
	}
	printf("port : %d\n",port);
	char* p;
	for(p=announce; p<announce_ind; p++)
	{
		url_len++;   
	}

	announce_url_t* ret;
	ret = (announce_url_t*)malloc(sizeof(announce_url_t));
	if(ret == NULL)
	{
		perror("Could not allocate announce_url_t");
		exit(-73);
	}

	p = announce;
	printf("ANNOUNCE: %s\n",announce);
	if(strstr(announce,"http://") > 0)
	{
		url_len -= 7;
		p += 7;
	}

	ret->hostname = malloc(url_len+1); // +1 for \0
	memset(ret->hostname ,0,url_len+1);
	memcpy(ret->hostname,p,url_len);
	ret->hostname[url_len] = '\0';
	printf("Url:%s len: %d\n",ret->hostname,url_len);
	ret->port = port;

	return ret;
}
