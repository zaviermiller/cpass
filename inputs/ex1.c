#include <stdio.h>
int main(){
    int i = 0;
    int j = 1;
    int foo = 2;
    int bar = 3;

    j = i + 1;
    foo = bar;
    foo = foo + bar;
    i = j;
    j = foo;
    j += bar;
    printf("j:%d\n i:%d\n foo:%d\n", j, i, foo);
    return 0;
}
