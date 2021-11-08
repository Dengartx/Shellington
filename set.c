#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "set.h"

void set(FILE* fp,char argv[],char path[],int save)
{
char single_line[1000];
while(!feof(fp)) {

fgets(single_line,100,fp);

if(strncmp(single_line,argv,strlen(argv))==0) {
save = 0;
break;
}

}
if(save == 1)
fprintf(fp,"%s-%s\n",argv,path);

save = 1;

}





