#include<stdio.h>
#include"lib/user/syscall.h"

int main(int argc, char** argv)
{
        
    printf("%d %d\n", fibonacci(atoi(argv[1])), max_of_four_int(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4])));
    return 0;
}
