
#include "bencode.h"
#include "util.h"
#include "sha1.h"

// ע��: �������ֻ�ܴ����ļ�ģʽtorrent
torrentmetadata_t* parsetorrentfile(char* filename)
{
	int i;
	be_node* ben_res;
	FILE* f;
	int flen;
	char* data;
	torrentmetadata_t* ret;

	// ���ļ�, ��ȡ���ݲ������ַ���
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

	// �����ڵ�, ����ļ��ṹ�������Ӧ���ֶ�.
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

	// �������torrent��info_hashֵ
	// ע��: SHA1�������صĹ�ϣֵ�洢��һ������������, ����С���ֽ���������˵,
	// ����tracker������peer���صĹ�ϣֵ���бȽ�ʱ, ��Ҫ�Ա��ش洢�Ĺ�ϣֵ
	// �����ֽ���ת��. �������ɷ��͸�tracker������ʱ, Ҳ��Ҫ���ֽ������ת��.
	char* info_loc, *info_end;
	info_loc = strstr(data,"infod");  // ����info��, ����ֵ��һ���ֵ�
	if(info_loc==NULL)printf("info null\n");
	info_loc += 4; // ��ָ��ָ��ֵ��ʼ�ĵط�
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
		// ȥ����β��e
		if(*info_end == 'e')
		{
			--info_end;
		}

	}
	printf("end:%c\n",*info_end);
	char* p;
	int len = 0;
	for(p=info_loc; p<=info_end; p++) len++;

	// ���������ַ�����SHA1��ϣֵ�Ի�ȡinfo_hash
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

	// ��������ȡ��Ӧ����Ϣ
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
		// info��һ���ֵ�, ������һЩ�������ǹ��ĵļ�
		if(!strncmp(ben_res->val.d[i].key,"info",strlen("info")))
		{
			be_dict* idict;
			if(ben_res->val.d[i].val->type != BE_DICT)
			{
				perror("Expected dict, got something else");
				exit(-3);
			}
			idict = ben_res->val.d[i].val->val.d;
			// �������ֵ�ļ�
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

			} // forѭ������
		} // info���������
	}

	// ȷ��������˱�Ҫ���ֶ�

	be_free(ben_res);  

	if(filled < 5)
	{
		printf("Did not fill necessary field\n");
		return NULL;
	}
	else
		return ret;
}
