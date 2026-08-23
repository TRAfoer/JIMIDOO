#include <stdio.h>

#include <nds.h>

int main(void)
{
    consoleDemoInit();
    printf("PussiFight\n");

    while (1) {
        swiWaitForVBlank();
    }
}
