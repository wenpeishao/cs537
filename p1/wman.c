#include <stdio.h>
#include <string.h>
#include <dirent.h>
/*
    Helper func that used to loacte the file at directory and open the file;
    return 0 if successfully open the file 
    return -1 if can't find the file
*/
int open_file(char *file_name[], DIR *path){
    //rd is the pionter to the return val of readdir
    struct dirent *rd;
    int found = -1;
    while((rd = readdir(path)) != NULL ){
        //When file is a regular file. and name are same with file_name break the loop. find the file.
        if(rd->d_type == DT_REG && strcmp(rd->d_name,file_name)==0){
            found = 0;
            FILE *fp = fopen(file_name, "r");
            if (fp == NULL) {
                printf("cannot open file\n");
                exit(1);
            }
        }else if(rd->d_type == DT_DIR){

        }
    }
    return found;

}
int main(int argc, char *argv[])
{   
    //one arguemnt need to find the file at the hole directory
    if(argc == 2){
        
    }
    //two arguement need to loate a directory
    else if(argc > 2){
        int pageNumber = (int)argv[1];
        if(pageNumber < 1 && pageNumber > 9){
            printf("invalid section\n");
            exit(1);
        }
        //path that need to get fixed
        char *path[100] = "./man_pages/man";
        //add the arguement from user input to the path
        char page = (char)pageNumber;
        //appends the page string to the path string make the path we are looking for at the man_pages directory
        strcat(path, page);


    }
    // no arguemnt
    else{
        printf("What manual page do you want?\nFor example, try 'wman wman'\n" );
        return 0;
    }
 
    return 0;
}