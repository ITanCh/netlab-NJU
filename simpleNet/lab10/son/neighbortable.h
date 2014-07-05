//文件名: son/neighbortable.h
//
//描述: 这个文件定义邻居表的数据结构和API
//
//创建日期: 2013年

#ifndef NEIGHBORTABLE_H 
#define NEIGHBORTABLE_H
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include "../topology/topology.h"

//邻居表条目定义
//一张邻居表包含n个条目, 其中n是邻居的数量
//每个节点都运行一个简单重叠网络进程SON, 每个SON进程为运行该进程的节点维护一张邻居表.

typedef struct neighborentry {
  int nodeID;	        //邻居的节点ID
  in_addr_t nodeIP;     //邻居的IP地址
  int conn;	            //针对这个邻居的TCP连接套接字描述符
} nbr_entry_t;


//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.
nbr_entry_t* nt_create();

//这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t* nt);

//这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1, 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn);

#endif
