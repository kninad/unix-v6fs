#include <stdio.h>
#include <libgen.h>
#include <string.h>


void path_print(char* path){
    printf("Ppath: %s \n", path);
    char copy[100];
    strcpy(copy, path);
    char* dir = dirname(path);
    char* file = basename(path);
    printf("LEN: %ld\n", strlen(path));
    printf("DIR: %s\n", dir);
    printf("FIL: %s\n\n", file);    
}

void path_list(char* path) {
    // https://stackoverflow.com/a/27860945/9579260
    char copy[100];
    strcpy(copy, path);
    char* rest = NULL;
    char* token = strtok_r(copy, "/", &rest);
    char new_path[80];
    printf("Printing Path: %s \n", path);
    while (token){
        printf(": %s\n", token);        
        if(token) {
            strcpy(new_path, token);
        }
        token = strtok_r(NULL, "/", &rest);
    }
    printf("Ppath: %s\n\n", path);
    printf("New path: %s\n\n", new_path);
}

int check_double_slash(char* path) {    
    char prev = path[0];
    for(int i = 1; i < strlen(path); ++i){
        if(path[i] == prev && prev == '/'){
            printf("Invalid path!  Should not contain consecutive \\ \n");
            return 1;
        }
        prev = path[i];
    }
    return 0;
}

int main(void)
{   
    char buffer[100];
    char path[] = "/this/is/my/file/path.txt" ;
    // path_print(path);
    // path_list(path);
    char path2[] = "local/n2";
    strcpy(buffer, path2);
    path_list(buffer);
    strcpy(buffer, path2);
    path_print(buffer);    
    
    // int status;
    // status = check_double_slash(path);
    // status = check_double_slash(path2);
    
    int buffer2[16/4] = {0};
    for(int i = 0; i < 16/4; ++i){
        printf("%d ", buffer2[i]);
    }
    printf("\n");

    int status;
    status = strcmp(path, ".");
    return 0;
}
