#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "delete.h"

void delete(FILE* fp,char argv[])
{
char single_line[1000];
FILE* fp2 = fopen("File","w");

while(!feof(fp)) {

if(strncmp(single_line,argv,1) != 0) {
fprintf(fp2,"%s",single_line);

}

fgets(single_line,100,fp);
}
fclose(fp);
fclose(fp2);
remove("file");
rename("File","file");
}





