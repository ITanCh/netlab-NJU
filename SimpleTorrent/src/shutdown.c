#include "btdata.h"
#include "util.h"

extern int g_done;
void printBit(char c){
	int i = 7;
	for(;i >= 0;i--){
		printf("%d",(c >> i)&1);
	}
	printf(" ");
}
// ��ȷ�Ĺرտͻ���
void client_shutdown(int sig)
{
  // ����ȫ��ֹͣ������ֹͣ���ӵ�����peer, �Լ���������peer������. Set global stop variable so that we stop trying to connect to peers and
  // �����������peer���ӵ��׽��ֺ����ӵ�����peer���߳�.
  printf("closing...wait...\n");
  //save g_bitmap data
  f_save_to = fopen(resume_file,"w+");
  printf("Begin to save bitmap to temp file ...\n");
  int i = 0;
  for(;i < mapcount;i++){
	  fprintf(f_save_to,"%c",g_bitmap[i]);
	  //printf("%02x ",g_bitmap[i]);
	  printBit(g_bitmap[i]);
  }
  printf("\nWrite temp file done!\n");

  del_all_peer();
  free(g_bitmap);
  free(g_piece_state);
  free(g_tracker_response);
  fclose(g_f);
  g_done = 1;
}
