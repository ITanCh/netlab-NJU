
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "routingtable.h"

//makehash()是由路由表使用的哈希函数.
//它将输入的目的节点ID作为哈希键,并返回针对这个目的节点ID的槽号作为哈希值.
int makehash(int node)
{
	int value = node % MAX_ROUTINGTABLE_SLOTS;
	return value;
}

//这个函数动态创建路由表.表中的所有条目都被初始化为NULL指针.
//然后对有直接链路的邻居,使用邻居本身作为下一跳节点创建路由条目,并插入到路由表中.
//该函数返回动态创建的路由表结构.
routingtable_t* routingtable_create()
{
	printf("RoutingTable Create: set NULL\n");
	routingtable_t *routingtable;
	routingtable = (routingtable_t*)malloc(sizeof(routingtable_t));
	int i = 0;
	for(; i < MAX_ROUTINGTABLE_SLOTS;i++)
		routingtable->hash[i] = NULL;

	printf("RoutingTable Create: init neighbours\n");

	//praparations
	int *nbr_array = topology_getNbrArray();
	int nbr_num = topology_getNbrNum();
	int index = 0;

	printf("neighbours number: %d\n",nbr_num);
	//insert neighbours to the routing table
	for(i = 0;i < nbr_num;i++)
	{
		routingtable_entry_t *entry = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));

		//nbr_array[i] is neighbour's nodeID
		entry->destNodeID = nbr_array[i];
		entry->nextNodeID = nbr_array[i];
		entry->next = NULL;
		printf("new routing table entry: destnode:%d\tnextnode:%d\n",entry->destNodeID,entry->nextNodeID);
		index = makehash(entry->destNodeID);
		if(routingtable->hash[index] == NULL)
		{
			 routingtable->hash[index] = entry;
		}
		else
		{
			routingtable_entry_t *slot = routingtable->hash[index];
			while(slot->next != NULL)
				slot = slot->next;
			slot->next = entry;
		}	
	}
	printf("Routing Table Create: insertion Done\n");
	return routingtable;
}

//这个函数删除路由表.
//所有为路由表动态分配的数据结构将被释放.
void routingtable_destroy(routingtable_t* routingtable)
{
	int i = 0;
	for(; i < MAX_ROUTINGTABLE_SLOTS; i++)
	{
		free(routingtable->hash[i]);
	}
	free(routingtable);
	printf("destroy routingtable done\n");
	return;
}

//这个函数使用给定的目的节点ID和下一跳节点ID更新路由表.
//如果给定目的节点的路由条目已经存在, 就更新已存在的路由条目.如果不存在, 就添加一条.
//路由表中的每个槽包含一个路由条目链表, 这是因为可能有冲突的哈希值存在(不同的哈希键, 即目的节点ID不同, 可能有相同的哈希值, 即槽号相同).
//为在哈希表中添加一个路由条目:
//首先使用哈希函数makehash()获得这个路由条目应被保存的槽号.
//然后将路由条目附加到该槽的链表中.
void routingtable_setnextnode(routingtable_t* routingtable, int destNodeID, int nextNodeID)
{
	int index = makehash(destNodeID);
	if(routingtable->hash[index] == NULL)
	{
		printf("SetNextNode: Slot %d is NULL, insert new entry\n",index);
		routingtable_entry_t *new_entry = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
		new_entry->next = NULL;
		new_entry->destNodeID = destNodeID;
		new_entry->nextNodeID = nextNodeID;
		routingtable->hash[index] = new_entry;
	}
	else
	{
		routingtable_entry_t *entry = routingtable->hash[index];
		routingtable_entry_t *pre_entry = entry;
		while(entry != NULL)
		{
			pre_entry = entry;
			if(entry->destNodeID == destNodeID)
			{
				entry->nextNodeID = nextNodeID;
				return ;
			}
			entry = entry->next;
		}
		if(entry == NULL)
		{
			printf("SetNextNodeL: insert new entry at Slot %d tail\n",index);
			routingtable_entry_t *new_entry = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
			new_entry->next = NULL;
			new_entry->destNodeID = destNodeID;
			new_entry->nextNodeID = nextNodeID;
			pre_entry->next = new_entry;
		}
	}
	return;
}

//这个函数在路由表中查找指定的目标节点ID.
//为找到一个目的节点的路由条目, 你应该首先使用哈希函数makehash()获得槽号,
//然后遍历该槽中的链表以搜索路由条目.如果发现destNodeID, 就返回针对这个目的节点的下一跳节点ID, 否则返回-1.
int routingtable_getnextnode(routingtable_t* routingtable, int destNodeID)
{
	int index = makehash(destNodeID);
	routingtable_entry_t *entry = routingtable->hash[index];
	while(entry != NULL)
	{
		if(entry->destNodeID == destNodeID)
			return entry->nextNodeID;
		entry  = entry->next;
	}
	return -1;
}

//这个函数打印路由表的内容
void routingtable_print(routingtable_t* routingtable)
{
	printf("================== ROUTING TABLE ==================\n");
	int i = 0;
	routingtable_entry_t *temp;
	for(;i < MAX_ROUTINGTABLE_SLOTS;i++)
	{
		temp = routingtable->hash[i];
		printf("slots: %d\n",i);
		while(temp != NULL)
		{
			printf("\tdest_nodeID: %d    next_nodeID: %d\n",temp->destNodeID,temp->nextNodeID);
			temp = temp->next;
		}
	}
	return;
}
