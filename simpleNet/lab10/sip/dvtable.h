//文件名: sip/dvtable.h
//
//描述: 这个文件定义用于距离矢量表的数据结构和函数. 
//
//创建日期: 2013年1月

#ifndef DVTABLE_H
#define DVTABLE_H

#include "../common/pkt.h"

//dv_entry_t结构定义
typedef struct distancevectorentry {
	int nodeID;		    //目标节点ID	
	unsigned int cost;	//到目标节点的代价
} dv_entry_t;

//一个距离矢量表包含(n+1)个dv_t条目, 其中n是这个节点的邻居数, 剩下的一个是这个节点自身. 
typedef struct distancevector {
	int nodeID;		        //源节点ID
	dv_entry_t* dvEntry;	//一个包含N个dv_entry_t的数组, 其中每个成员包含目标节点ID和从该源节点到该目标节点的代价. N是重叠网络中总的节点数.
} dv_t;

//这个函数动态创建距离矢量表.
//距离矢量表包含n+1个条目, 其中n是这个节点的邻居数,剩下1个是这个节点本身.
//距离矢量表中的每个条目是一个dv_t结构,它包含一个源节点ID和一个有N个dv_entry_t结构的数组, 其中N是重叠网络中节点总数.
//每个dv_entry_t包含一个目的节点地址和从该源节点到该目的节点的链路代价.
//距离矢量表也在这个函数中初始化.从这个节点到其邻居的链路代价使用提取自topology.dat文件中的直接链路代价初始化.
//其他链路代价被初始化为INFINITE_COST.
//该函数返回动态创建的距离矢量表.
dv_t* dvtable_create();

//这个函数删除距离矢量表.
//它释放所有为距离矢量表动态分配的内存.
void dvtable_destroy(dv_t* dvtable);

//这个函数设置距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,并且链路代价也被成功设置了,就返回1,否则返回-1.
int dvtable_setcost(dv_t* dvtable,int fromNodeID,int toNodeID, unsigned int cost);

//这个函数返回距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,就返回链路代价,否则返回INFINITE_COST.
unsigned int dvtable_getcost(dv_t* dvtable, int fromNodeID, int toNodeID);

//这个函数打印距离矢量表的内容.
void dvtable_print(dv_t* dvtable);

#endif
