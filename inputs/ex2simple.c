#include <stdio.h>
int main(){
    int i = 1;
    int j = 2;
    int c = 8;
    int d = 9;
    if(i < j){
        j = 6;
    }
    i = j;
    d = c;
    printf("i: %d\n j: %d\n c: %d\n d: %d\n", i, j, c, d);
    return 0;
}
