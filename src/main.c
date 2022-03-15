#include <uart.h>
#include <shell.h>
#include <cpio.h>
#include <malloc.h>

int main(){
    uart_init();
    char *test1 = (char *)simple_malloc(sizeof(char) * 8);
    test1[0] = 'a';
    test1[1] = 'b';
    test1[2] = 'c';
    test1[3] = 'd';
    test1[4] = 'e';
    uart_puts("[*] simple malloc - char array: ");
    uart_puts(test1);
    uart_puts("\n");

    unsigned int *test2 = (unsigned int *)simple_malloc(sizeof(unsigned int) * 4);
    test2[0] = 32;
    test2[1] = 0x87654321;
    test2[2] = 0xdeadbeef;
    test2[3] = 0xaabbccdd;
    char buf[100];
    uitohex(buf, test2[0]);
    uart_puts("[*] simple malloc - unsigned int: 0x");
    uart_puts(buf);
    uart_puts("\n");

    PrintWelcome();
    ShellLoop();
    
    return 0;
}