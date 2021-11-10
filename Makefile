


main: save.o library.o delete.o main.o 
	gcc -o main main.c save.c delete.c library.c


