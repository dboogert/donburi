#include <stdlib.h>

void* testB()
{
    return malloc(10);
}


void* testA()
{
    return testB();
}


void testC()
{
    int* p = new int[1204];
}

int main(int argc, char** argv)
{
    free( testA() );

    testC();
}