#include <uart.h>
#include <shell.h>
#include <cpio.h>
#include <malloc.h>
#include <string.h>
#include <fdt.h>
#include <irq.h>
#include <allocator.h>
#include <math.h>


int main(unsigned long dtb_base){
    uart_init();
    allocator_init();    
    // buddy_debug();
    // chunk_debug();
    kmalloc_debug();

    char buf[15];
    // uart_getc();
    // enable_timer_irq();
    // enable_AUX_MU_IER_r();
    // enable_irq(); // DAIF set to 0b0000
    
    uart_puts("[*] DTB_BASE: 0x");
    uitohex(buf, (unsigned int)dtb_base);
    uart_puts(buf);
    uart_puts("\n");
    fdt_traverse((fdt_header *)dtb_base, initramfs_callback);

    // char *test1 = (char *)simple_malloc(sizeof(char) * 8);
    // memcpy(test1, "abcdef", 6);
    // uart_puts("[*] simple malloc - char array: ");
    // uart_puts(test1);
    // uart_puts("\n");

    // unsigned int *test2 = (unsigned int *)simple_malloc(sizeof(unsigned int) * 4);
    // test2[0] = 32;
    // test2[1] = 0x87654321;
    // test2[2] = 0xdeadbeef;
    // test2[3] = 0xaabbccdd;
    
    // uitohex(buf, test2[0]);
    // uart_puts("[*] simple malloc - unsigned int: 0x");
    // uart_puts(buf);
    // uart_puts("\n");

    PrintWelcome();

    ShellLoop();
    
    return 0;
}