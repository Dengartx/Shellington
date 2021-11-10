#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "save.h"

void save(FILE* fp,char argv[],int save1)
{
char single_line[1000];
int d = -1;
while(!feof(fp)) {

fgets(single_line,100,fp);

d++;
if(strncmp(&single_line[2],argv,strlen(argv))==0) {
save1 = 0;
break;
}

}
if(save1 == 1)
fprintf(fp,"%d %s\n",d,argv);
save1 = 1;

}





