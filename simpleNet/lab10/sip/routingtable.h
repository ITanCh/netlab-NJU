//文件名: sip/routingtable.h
//
//描述: 这个文件定义用于路由表的数据结构和函数. 
//一个路由表是一个包含MAX_ROUTINGTABLE_SLOTS个槽条目的哈希表.  
//
//创建日期: 2013年1月

#ifndef ROUTINGTABLE_H
#define ROUTINGTABLE_H

//routingtable_entry_t是包含在路由表中的路由条目.
typedef struct routingtable_entry {
	int destNodeID;		//目标节点ID
	int nextNodeID;		//报文应该转发给的下一跳节点ID
	struct routingtable_entry* next;	//指向在同一个路由表槽中的下一个routingtable_entry_t
} routingtable_entry_t;

//一个路由表是一个包含MAX_ROUTINGTABLE_SLOTS个槽的哈希表. 每个槽是一个路由条目的链表.
typedef struct routingtable {
	routingtable_entry_t* hash[MAX_ROUTINGTABLE_SLOTS];
} routingtable_t;

//makehash()是由路由表使用的哈希函数.
//它将输入的目的节点ID作为哈希键,并返回针对这个目的节点ID的槽号作为哈希值.
int makehash(int node); 

//这个函数动态创建路由表.表中的所有条目都被初始化为NULL指针.
//然后对有直接链路的邻居,使用邻居本身作为下一跳节点创建路由条目,并插入到路由表中.
//该函数返回动态创建的路由表结构.
routingtable_t* routingtable_create();

//这个函数删除路由表.
//所有为路由表动态分配的数据结构将被释放.
void routingtable_destroy(routingtable_t* routingtable);

//这个函数使用给定的目的节点ID和下一跳节点ID更新路由表.
//如果给定目的节点的路由条目已经存在, 就更新已存在的路由条目.如果不存在, 就添加一条.
//路由表中的每个槽包含一个路由条目链表, 这是因为可能有冲突的哈希值存在(不同的哈希键, 即目的节点ID不同, 可能有相同的哈希值, 即槽号相同).
//为在哈希表中添加一个路由条目:
//首先使用哈希函数makehash()获得这个路由条目应被保存的槽号.
//然后将路由条目附加到该槽的链表中.
void routingtable_setnextnode(routingtable_t* routingtable, int destNodeID, int nextNodeID);

//这个函数在路由表中查找指定的目标节点ID.
//为找到一个目的节点的路由条目, 你应该首先使用哈希函数makehash()获得槽号,
//然后遍历该槽中的链表以搜索路由条目.如果发现destNodeID, 就返回针对这个目的节点的下一跳节点ID, 否则返回-1.
int routingtable_getnextnode(routingtable_t* routingtable, int destNodeID);

//这个函数打印路由表的内容
void routingtable_print(routingtable_t* routingtable);

#endif
