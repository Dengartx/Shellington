#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "library.h"

void library(FILE* fp)
{
char single_line[1000];
while(!feof(fp)) {

printf("%s",single_line);
fgets(single_line,100,fp);

}


}





