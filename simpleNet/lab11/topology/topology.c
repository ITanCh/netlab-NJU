//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数 
//
//创建日期: 2013年

#include "topology.h"

int gethostname(char *hostname,size_t size);

int * allIdList;
int allCount = 0;

int * nbIdList;
in_addr_t * nbIpList;
int * costList;
int nbCount = 0;

int bigCount = 0;
int smallCount = 0;

int isRead=0;
//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname) 
{
	struct hostent *h = gethostbyname(hostname);
	if(h == NULL)
		return -1;
	else{
		struct in_addr* addr = (struct in_addr*)h->h_addr_list[0];
		int nid = topology_getNodeIDfromip(addr);
		//printf("node ID:%d \n",nid);
		return nid;
	}
}

//get ip
in_addr_t topology_getNodeIPfromname(char *name)
{
	struct hostent *h = gethostbyname(name);
	struct in_addr* addr=(struct in_addr*)h->h_addr_list[0];
	return addr->s_addr;
}
//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr)
{
	//char id=ntohl(addr->s_addr);
	int id = (addr->s_addr)>>24;
	//printf("ID from IP:%d\n",id);
	return id;
}

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID()
{
	char name[32];
	gethostname(name,sizeof(name));
	//printf("hostname:%s \n",name);
	return topology_getNodeIDfromname(name);
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum()
{
	return nbCount;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum()
{ 
	return allCount;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID. 
int* topology_getNodeArray()
{
	return allIdList;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.  
int* topology_getNbrArray()
{
	return nbIdList;
}

in_addr_t * topology_getNbrIpArray()
{
	return nbIpList;
}

//get the big id neighbor 
int getBigCount()
{
	return bigCount;
}

//get the small id neightbor
int getSmallCount()
{
	return smallCount;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
	return 0;
}
//a optimized function to get cost
int * topology_getNbrCost()
{
	return costList;
}

//read topology.dat
void getTopoData()
{
	if(isRead)return;	//reduce redundant read 
	isRead=1;
	FILE *pFile;
	pFile = fopen("../topology/topology.dat","r");
	if(pFile == NULL)
	{
		printf("open file error\n");
		exit(-1);
	}
	char buf[128];
	int lineCount=0;
	while (fgets(buf,sizeof(buf),pFile)!= NULL) {
		lineCount++;
	}
	if(lineCount<=0)
	{
		printf("no topology in file\n");
		exit(-1);
	}

	//init topo data list
	allIdList=malloc(lineCount*2*sizeof(int));
	nbIdList=malloc(lineCount*sizeof(int));
	nbIpList=malloc(lineCount*sizeof(in_addr_t));
	costList=malloc(lineCount*sizeof(int));

	//get data from file
	char host1[32],host2[32];
	int cost;
	int myId=topology_getMyNodeID();
	nbCount=0;
	fseek(pFile,0,SEEK_SET);
	while(fscanf(pFile,"%s %s %d", host1, host2, &cost) > 0)	
	{
		int id1=topology_getNodeIDfromname(host1);
		int id2=topology_getNodeIDfromname(host2);
		int i=0;
		//find if this node has heen counted
		for(;i<allCount;i++)
		{
			if(allIdList[i]==id1)break;
		}
		if(i==allCount)allIdList[allCount++]=id1;

		i=0;
		for(;i<allCount;i++)
		{
			if(allIdList[i]==id2)break;
		}
		if(i==allCount)allIdList[allCount++]=id2;
		
		//neighbor 
		if(id1==myId)
		{
			nbIdList[nbCount]=id2;
			nbIpList[nbCount]=topology_getNodeIPfromname(host2);
			costList[nbCount]=cost;
			if(id2>id1)bigCount++;
			else if(id1>id2)smallCount++;
			nbCount++;
		}
		else if(id2==myId)
		{
			nbIdList[nbCount]=id1;
			nbIpList[nbCount]=topology_getNodeIPfromname(host1);
			costList[nbCount]=cost;
			if(id2>id1)smallCount++;
			else if(id1>id2)bigCount++;
			nbCount++;
		}
	}
}

