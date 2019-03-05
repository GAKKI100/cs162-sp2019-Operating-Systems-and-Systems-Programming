#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void count(char *name, FILE *file){
    int line_count = 0;
    int word_count = 0;
    int byte_count = 0;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int in_word = 0;
 
    while((read = getline(&line, &len, file)) != -1){
        size_t size = strlen(line);
        for(int i = 0; i < size; i++){
            byte_count += 1;
            char ch = line[i];
            if(ch == '\n'){
                line_count += 1;
            }

            if(ch == ' ' || ch == '\n'){
                in_word = 0;
            }else if(!in_word){
                word_count += 1;
                in_word = 1;
            }
        }       
    }
    if(name){
        printf("%d  %d  %d  %s\n", line_count, word_count, byte_count, name);
    }else{
        printf("%d  %d  %d\n", line_count, word_count, byte_count);
    }
    free(line);
} 

int main(int argc, char *argv[]){
    char* name = NULL;
    FILE* file = NULL;
 
    if(argc == 1){
        file = stdin;
        count(name, file);
    }else if(argc == 2){
        name = argv[1];
        file = fopen(name, "r");
        if(!file){
             printf("cannot open this file!");
             exit(1);
        }
        count(name, file);
    }
    if(argc == 2){
        fclose(file);
    }
    return 0;
}
