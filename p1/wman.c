#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
/*
    Helper func that used to loacte the file at directory and open the file;
    return 0 if successfully open the file 
    return -1 if can't find the file
    reference: https://stackoverflow.com/questions/8149569/scan-a-directory-to-find-files-in-c?noredirect=1&lq=1
*/
void open_file(char *file_name, DIR *path){
    //rd is the pionter to the return val of readdir
    struct dirent *rd;
    struct stat statbuf;
    char content[1000];
    //keep reading all file in the path
    while((rd = readdir(path)) != NULL ){
        lstat(rd->d_name,&statbuf);
        if(S_ISDIR(statbuf.st_mode)){
            //avoid to search through the "." and ".." directory
            if(strcmp(".",rd->d_name) == 0 || 
            strcmp("..",rd->d_name)==0){
                continue;
            }
            open_file(file_name, opendir(rd->d_name));
        }
        else{
            //When file is a regular file. and name are same with file_name break the loop. find the file.
            if(strcmp(rd->d_name,file_name)==0){
                //Open the file
                FILE *fp = fopen(file_name, "r");
            if (fp == NULL) {
                printf("cannot open file\n");
                exit(1);
            }
            //Read through the file and print it out
            if(fgets(content, sizeof(content), fp)!=NULL){
                printf("%s", content);
                }
            }
        }   
    }
    return;
}
int main(int argc, char *argv[]){   
    //one arguemnt need to find the file at the hole directory
    if(argc == 2){
        //path that starts search
        char path [100] = "./man_pages";
        char *file_name = argv[1];
        open_file(file_name, opendir(path));
    }
    //two arguement need to loate a directory
    else if(argc > 2){
        int pageNumber = (int)argv[1];
        if(pageNumber < 1 && pageNumber > 9){
            printf("invalid section\n");
            return(1);
        }
        //path that need to get fixed
        char path[100] = "./man_pages/man";
        //add the arguement from user input to the path
        char page = (char)pageNumber;
        //appends the page string to the path string make the path we are looking for at the man_pages directory
        strcat(path, page);
        char *file_name = argv[2];
    }
    //no arguement tan
    else if(argc < 2){
        printf("What manual page do you want?\nFor example, try 'wman wman'\n" );
        return 0;
    }
    return 0;
}