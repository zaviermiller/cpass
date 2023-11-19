#include <stdio.h>
int main(){
    int i = 1;
    int j = 2;
    int a = 6;
    int b = 7;
    int c = 8;
    int d = 9;
    if(i < j){
        int temp = j + 1;
        j = 6;
    }
    i = j;
    d = c;
    printf("i: %d\n j: %d\n a: %d\n b: %d\n c: %d\n d: %d\n", i, j, a, b, c, d);
    return 0;
}
