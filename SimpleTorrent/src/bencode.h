/*
 * bencode解码的C语言实现.
 * BitTorrent定义的B编码格式详见实验讲义的附录G和下面的网址:
 *  https://wiki.theory.org/BitTorrentSpecification#Bencoding
 */

/* 使用方法:
 *  - 将B编码数据传递给be_decode()
 *  - 解析返回的结果树
 *  - 调用be_free()释放资源
 */

#ifndef _BENCODE_H
#define _BENCODE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	BE_STR,
	BE_INT,
	BE_LIST,
	BE_DICT,
} be_type;

struct be_dict;
struct be_node;

typedef struct be_dict {
	char *key;
	struct be_node *val;
} be_dict;

typedef struct be_node {
	be_type type;
	union {
		char *s;
		long long i;
		struct be_node **l;
		struct be_dict *d;
	} val;
} be_node;

extern long long be_str_len(be_node *node);
extern be_node *be_decode(const char *bencode);
extern be_node *be_decoden(const char *bencode, long long bencode_len);
extern void be_free(be_node *node);
extern void be_dump(be_node *node);

#ifdef __cplusplus
}
#endif

#endif
