
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "dvtable.h"

int nbn=0;
int alln=0;
//这个函数动态创建距离矢量表.
//距离矢量表包含n+1个条目, 其中n是这个节点的邻居数,剩下1个是这个节点本身.
//距离矢量表中的每个条目是一个dv_t结构,它包含一个源节点ID和一个有N个dv_entry_t结构的数组, 其中N是重叠网络中节点总数.
//每个dv_entry_t包含一个目的节点地址和从该源节点到该目的节点的链路代价.
//距离矢量表也在这个函数中初始化.从这个节点到其邻居的链路代价使用提取自topology.dat文件中的直接链路代价初始化.
//其他链路代价被初始化为INFINITE_COST.
//该函数返回动态创建的距离矢量表.
dv_t* dvtable_create()
{
	getTopoData();
	nbn=topology_getNbrNum();
	int *nbList=topology_getNbrArray();
	int *costList=topology_getNbrCost();
	alln=topology_getNodeNum();
	int *allList=topology_getNodeArray();

	dv_t *dvList=malloc((nbn+1)*sizeof(dv_t));
	if(dvList==NULL)exit(-1);
	int i=0;
	//init nerghbor 
	for(;i<nbn;i++)
	{
		dvList[i].nodeID=nbList[i];
		dvList[i].dvEntry=malloc(alln*sizeof(dv_entry_t));
		int j=0;
		for(;j<alln;j++)
		{
			dvList[i].dvEntry[j].nodeID=allList[j];
			dvList[i].dvEntry[j].cost=INFINITE_COST;
		}
	}
	
	//init this node
	dvList[nbn].nodeID=topology_getMyNodeID();
	dvList[nbn].dvEntry=malloc(alln*sizeof(dv_entry_t));
	for(i=0;i<alln;i++)
	{
		dvList[nbn].dvEntry[i].nodeID=allList[i];
		if(allList[i]==dvList[nbn].nodeID)
		{
			dvList[nbn].dvEntry[i].cost=0;
			continue;
		}
		int j=0;
		for(;j<nbn;j++)
		{
			if(allList[i]==nbList[j])
			{
				dvList[nbn].dvEntry[i].cost=costList[j];
				break;
			}
		}
		if(j==nbn)
			dvList[nbn].dvEntry[i].cost=INFINITE_COST;
	}
	return dvList;
}

//这个函数删除距离矢量表.
//它释放所有为距离矢量表动态分配的内存.
void dvtable_destroy(dv_t* dvtable)
{
	int i=0;
	for(;i<nbn+1;i++)
	{
		free(dvtable[i].dvEntry);
	}
	free(dvtable);
	dvtable=NULL;
}

//这个函数设置距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,并且链路代价也被成功设置了,就返回1,否则返回-1.
int dvtable_setcost(dv_t* dvtable,int fromNodeID,int toNodeID, unsigned int cost)
{
	int i=0;
	int j=0;
	for(;i<nbn+1;i++)
	{
		if(dvtable[i].nodeID==fromNodeID)
		{
			for(j=0;j<alln;j++)
			{
				if(dvtable[i].dvEntry[j].nodeID==toNodeID)
				{
					dvtable[i].dvEntry[j].cost=cost;
					return 1;
				}
			}
			return -1;
		}
	}
	return -1;
}

//这个函数返回距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,就返回链路代价,否则返回INFINITE_COST.
unsigned int dvtable_getcost(dv_t* dvtable, int fromNodeID, int toNodeID)
{
	int i=0;
	int j=0;
	for(;i<nbn+1;i++)
	{
		if(dvtable[i].nodeID==fromNodeID)
		{
			for(j=0;j<alln;j++)
			{
				if(dvtable[i].dvEntry[j].nodeID==toNodeID)
				{
					return dvtable[i].dvEntry[j].cost;
				}
			}
			return INFINITE_COST;
		}
	}
	return INFINITE_COST;
}

//这个函数打印距离矢量表的内容.
void dvtable_print(dv_t* dvtable)
{
	int i=0;
	int j=0;
	for(;i<nbn+1;i++)
	{
		for(j=0;j<alln;j++)
		{
			printf("node %d ====> node %d   cost:%d\n",dvtable[i].nodeID,dvtable[i].dvEntry[j].nodeID,dvtable[i].dvEntry[j].cost);
		}
	}
}
