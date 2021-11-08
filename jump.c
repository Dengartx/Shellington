#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "jump.h"

void jump(FILE* fp,char argv[])
{
char single_line[1000];
char s[2] = "-";
char* token;
while(!feof(fp)) {

fgets(single_line,100,fp);

if(strncmp(single_line,argv,strlen(argv))==0) {
token = strtok(single_line,s);
token = strtok(NULL,s);
printf("Path: %s\n",token);
return;
}

}

}
