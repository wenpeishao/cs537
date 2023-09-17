#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

//Helper function to search the given term at the specific path, if founded the term print the NAME part
//reference: https://stackoverflow.com/questions/8149569/scan-a-directory-to-find-files-in-c?noredirect=1&lq=1
int open_file(char *file_name, char *path){
    DIR *dp;
    //rd is the pionter to the return val of readdir
    struct dirent *rd;
    char content[1000];
    fprintf(stderr,"now open: %s\n", path);
    //keep reading all file in the path
    if((dp = opendir(path)) == NULL) {
        fprintf(stderr,"cannot open directory: %s\n", path);
        return -1;
    }

    while((rd = readdir(dp)) != NULL ){
        fprintf(stderr,"now checking: %s\n", rd->d_name);
        if(DT_REG == rd->d_type){// It is a reg file start to read
            //When file is a regular file. and name are same with file_name break the loop. find the file.
            FILE *fp = fopen(file_name, "r");
            if(fp != NULL){
                // Loop to read and print the entire content of the file
                while (fgets(content, sizeof(content), fp) != NULL) {
                    printf("%s", content);
                }
                fclose(fp);  // Close the file after reading
                return 0;  // File found and opened successfully
            }
        }
        else{
            continue;
        }   
    }
    closedir(dp);
    return -1;
}