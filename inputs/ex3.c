#include <stdio.h>
int main(){
    int i = 1;
    int j = 2;
    int a = 6;
    int b = 7;
    int c = 8;
    int d = 9;
    if(i < 2){
        d = i + c;
        a = 10;
        if(d > 0){
            d = 0;
        }
        else{
            d = b;
        }
    }
    a = i;
    c = d;
    printf("i: %d\n j: %d\n a: %d\n b: %d\n c: %d\n d: %d\n", i, j, a, b, c, d);
    return 0;
}
