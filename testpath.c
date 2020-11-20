#include <stdio.h>
#include <libgen.h>
#include <string.h>


void path_print(char* path){
    char* dir = dirname(path);
    char* file = basename(path);
    printf("Printing Path:\n");
    printf("LEN: %ld\n", strlen(path));
    printf("DIR: %s\n", dir);
    printf("FIL: %s\n\n", file);    
}

void path_list(char* path) {
    // https://stackoverflow.com/a/27860945/9579260
    char* rest = NULL;
    char* token = strtok_r(path, "/", &rest);
    while (token){
        printf(": %s\n", token);
        token = strtok_r(NULL, "/", &rest);
    }
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
    char path[] = "/this/is/my/file/path.txt" ;
    // path_print(path);
    // path_list(path);
    char path2[] = "local/usr//ninad/tst";
    // path_print(path2);
    // path_list(path2);
    int status;
    status = check_double_slash(path);
    status = check_double_slash(path2);
  
    return 0;
}
