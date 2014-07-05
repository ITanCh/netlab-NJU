
#include "bencode.h"
#include "util.h"
#include "sha1.h"

// 注意: 这个函数只能处理单文件模式torrent
torrentmetadata_t* parsetorrentfile(char* filename)
{
	int i;
	be_node* ben_res;
	FILE* f;
	int flen;
	char* data;
	torrentmetadata_t* ret;

	// 打开文件, 获取数据并解码字符串
	f = fopen(filename,"r");
	if(f==NULL)
	{
		printf("torrent not exist!\n");
		exit(-1);
	}
	flen = file_len(f);
	data = (char*)malloc(flen*sizeof(char));
	fread((void*)data,sizeof(char),flen,f);
	fclose(f);
	ben_res = be_decoden(data,flen);

	// 遍历节点, 检查文件结构并填充相应的字段.
	if(ben_res->type != BE_DICT)
	{
		perror("Torrent file isn't a dictionary");
		exit(-13);
	}

	ret = (torrentmetadata_t*)malloc(sizeof(torrentmetadata_t));
	if(ret == NULL)
	{
		perror("Could not allocate torrent meta data");
		exit(-13);
	}

	// 计算这个torrent的info_hash值
	// 注意: SHA1函数返回的哈希值存储在一个整数数组中, 对于小端字节序主机来说,
	// 在与tracker或其他peer返回的哈希值进行比较时, 需要对本地存储的哈希值
	// 进行字节序转换. 当你生成发送给tracker的请求时, 也需要对字节序进行转换.
	char* info_loc, *info_end;
	info_loc = strstr(data,"infod");  // 查找info键, 它的值是一个字典
	if(info_loc==NULL)printf("info null\n");
	info_loc += 4; // 将指针指向值开始的地方
	info_end = data+flen-6;
	int getnode=0;
	while(*info_end!='\0')
	{
		if(*info_end=='n'&&*(info_end+1)=='o'&&*(info_end+2)=='d'&&*(info_end+3)=='e')
		{
			getnode=1;
			break;
		}
		info_end--;
	}
	if(getnode)
	{
		printf("nodes\n");
		while(*info_end!='e')
			--info_end;
	}
	else
	{
		info_end = data+flen-1;
		// 去掉结尾的e
		if(*info_end == 'e')
		{
			--info_end;
		}

	}
	printf("end:%c\n",*info_end);
	char* p;
	int len = 0;
	for(p=info_loc; p<=info_end; p++) len++;

	// 计算上面字符串的SHA1哈希值以获取info_hash
	SHA1Context sha;
	SHA1Reset(&sha);
	SHA1Input(&sha,(const unsigned char*)info_loc,len);
	if(!SHA1Result(&sha))
	{
		printf("FAILURE\n");
	}

	memcpy(ret->info_hash,sha.Message_Digest,20);
	printf("SHA1:\n");
	for(i=0; i<5; i++)
	{
		printf("%08X ",ret->info_hash[i]);
	}
	printf("\n");

	// 检查键并提取对应的信息
	int filled=0;
	int flag=0;
	for(i=0; ben_res->val.d[i].val != NULL; i++)
	{
		int j;
		if(!strncmp(ben_res->val.d[i].key,"announce",strlen("announce"))&&flag<1)
		{
			ret->announce = (char*)malloc(strlen(ben_res->val.d[i].val->val.s)*sizeof(char));
			memcpy(ret->announce,ben_res->val.d[i].val->val.s,strlen(ben_res->val.d[i].val->val.s));
			filled++;
			flag++;
		}
		// info是一个字典, 它还有一些其他我们关心的键
		if(!strncmp(ben_res->val.d[i].key,"info",strlen("info")))
		{
			be_dict* idict;
			if(ben_res->val.d[i].val->type != BE_DICT)
			{
				perror("Expected dict, got something else");
				exit(-3);
			}
			idict = ben_res->val.d[i].val->val.d;
			// 检查这个字典的键
			for(j=0; idict[j].key != NULL; j++)
			{ 
				if(!strncmp(idict[j].key,"length",strlen("length")))
				{
					ret->length = idict[j].val->val.i;
					filled++;
				}
				if(!strncmp(idict[j].key,"name",strlen("name")))
				{
					ret->name = (char*)malloc(strlen(idict[j].val->val.s)*sizeof(char));
					memcpy(ret->name,idict[j].val->val.s,strlen(idict[j].val->val.s));
					filled++;
				}
				if(!strncmp(idict[j].key,"piece length",strlen("piece length")))
				{
					ret->piece_len = idict[j].val->val.i;
					filled++;
				}
				if(!strncmp(idict[j].key,"pieces",strlen("pieces")))
				{
					int num_pieces = ret->length/ret->piece_len;
					if(ret->length % ret->piece_len != 0)
						num_pieces++;
					ret->pieces = (char*)malloc(num_pieces*20);
					memcpy(ret->pieces,idict[j].val->val.s,num_pieces*20);
					ret->num_pieces = num_pieces;
					filled++;
				}

			} // for循环结束
		} // info键处理结束
	}

	// 确认已填充了必要的字段

	be_free(ben_res);  

	if(filled < 5)
	{
		printf("Did not fill necessary field\n");
		return NULL;
	}
	else
		return ret;
}
