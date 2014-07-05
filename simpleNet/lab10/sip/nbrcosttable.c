
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "nbrcosttable.h"
#include "../common/constants.h"
#include "../topology/topology.h"

//这个函数动态创建邻居代价表并使用邻居节点ID和直接链路代价初始化该表.
//邻居的节点ID和直接链路代价提取自文件topology.dat. 

int nct_nbCount=0;
nbr_cost_entry_t* nbrcosttable_create()
{
	getTopoData();
	int nbn=topology_getNbrNum();	//can optimize getTopoData()
	nct_nbCount=nbn;
	int *nbList=topology_getNbrArray();
	int *costList=topology_getNbrCost();
	if(nbn<=0)return NULL;
	nbr_cost_entry_t *entryList=malloc(nbn*sizeof(nbr_cost_entry_t));

	int i=0;
	for(;i<nbn;i++)
	{
		entryList[i].nodeID=nbList[i];
		entryList[i].cost=costList[i];
	}
	return entryList;
}

//这个函数删除邻居代价表.
//它释放所有用于邻居代价表的动态分配内存.
void nbrcosttable_destroy(nbr_cost_entry_t* nct)
{
	if(nct==NULL)return;
	free(nct);
	nct=NULL;
}

//这个函数用于获取邻居的直接链路代价.
//如果邻居节点在表中发现,就返回直接链路代价.否则返回INFINITE_COST.
unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID)
{
	int i=0;
	for(;i<nct_nbCount;i++)
	{
		if(nct[i].nodeID==nodeID)
			return nct[i].cost;
	}
	return INFINITE_COST;
}

//这个函数打印邻居代价表的内容.
void nbrcosttable_print(nbr_cost_entry_t* nct)
{
	int i=0;
	for(;i<nct_nbCount;i++)
		printf("nodeID: %d         cost: %d\n",nct[i].nodeID,nct[i].cost);
}
