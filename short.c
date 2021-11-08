#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "set.h"
#include "jump.h"

int main(int args,char **argv)
{
FILE *fp;
char single_line[100];
if(strcmp(argv[1],"set") == 0) {

char w[100];

 printf("%s\n",getcwd(w,sizeof(w)));
int save = 1;
fp = fopen("file","a+");
set(fp,argv[2],w,save);
fclose(fp);
}
if(strcmp(argv[1],"jump") == 0) {

fp = fopen("file","r");
jump(fp,argv[2]);
fclose(fp);

}


}


