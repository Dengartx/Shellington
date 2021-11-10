#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "save.h"
#include "library.h"
#include "delete.h"

int main(int args,char **argv)
{
FILE *fp;
char single_line[100];


if(strcmp(argv[1],"-l") == 0) {

fp = fopen("file","r");
library(fp);
fclose(fp);
}




else if(strcmp(argv[1],"-d") == 0) {

fp = fopen("file","r");
delete(fp,argv[2]);

}

else if(strcmp(argv[1],"-i") == 0) {



}

else {


int save1 = 1;
fp = fopen("file","a+");
save(fp,argv[1],save1);
fclose(fp);
}
}
