#include <syscall.h>
#include <sched.h>
#include <read.h>
#include <uart.h>
#include <list.h>
#include <irq.h>
#include <cpio.h>
#include <malloc.h>
#include <string.h>
#include <timer.h>

extern Thread *run_thread_head;

/* 
 * Return value is x0
 * https://developer.arm.com/documentation/102374/0101/Procedure-Call-Standard
 */


/* Get current process’s id. */
void sys_getpid(TrapFrame *trapFrame){
    disable_irq();
    int pid = do_getpid();
    trapFrame->x[0] = pid;
    enable_irq();
}
int do_getpid(){
    return get_current()->id;
}

/* Return the number of bytes read by reading size byte into the user-supplied buffer buf. */
void sys_uart_read(TrapFrame *trapFrame){
    char *buf = (char *)trapFrame->x[0];
    unsigned int size = trapFrame->x[1];
    enable_irq();
    int idx = async_readnbyte(buf, size);
    disable_irq();
    trapFrame->x[0] = idx;
    enable_irq();
}

/* Return the number of bytes written after writing size byte from the user-supplied buffer buf. */
void sys_uart_write(TrapFrame *trapFrame){
    const char *buf = (char *)trapFrame->x[0];
    unsigned int size = trapFrame->x[1];
    unsigned int i;
    enable_irq();
    for(i = 0; i < size; i++){
        async_uart_putc(buf[i]);
    }
    disable_irq();
    trapFrame->x[0] = i;
    enable_irq();
}
void sys_exec(TrapFrame *trapFrame){
    disable_irq();
    const char *name = (const char *)trapFrame->x[0];
    char **const argv = (char **const)trapFrame->x[1];
    int success = do_exec(trapFrame, name, argv);
    trapFrame->x[0] = success;
    enable_irq();
}
int do_exec(TrapFrame *trapFrame, const char *name, char *const argv[]){
    
    /* check if the file info exist */
    file_info fileInfo = cpio_find_file_info(name);
    if(fileInfo.filename == NULL) return -1;

    /* check if the file can create in new memory */
    void *thread_code_addr = load_program(&fileInfo); 
    if(thread_code_addr == NULL) return -1;
    
    /* current thread will change the pc to new code addr */
    Thread *curr_thread = get_current();
    curr_thread->code_addr = thread_code_addr;
    curr_thread->code_size = fileInfo.filename_size;

    /* set current trapFrame elr_el1(begin of code) and sp_el0(begin of user stack)*/
    trapFrame->elr_el1 = (unsigned long)curr_thread->code_addr;
    trapFrame->sp_el0 = (unsigned long)curr_thread->ustack_addr + STACK_SIZE;

    return 0;
}

int kernel_exec(char *name){
    /* check if the file info exist */
    file_info fileInfo = cpio_find_file_info(name);
    if(fileInfo.filename == NULL) return -1;

    /* check if the file can create in new memory */
    void *thread_code_addr = load_program(&fileInfo); 
    if(thread_code_addr == NULL) return -1;

    Thread *new_thread = thread_create(thread_code_addr);
    new_thread->code_addr = thread_code_addr;
    new_thread->code_size = fileInfo.filename_size;
    print_string(UITOHEX, "new_thread->code_addr: 0x", (unsigned long long)new_thread->code_addr, 1);

    // set_period_timer_irq();
    add_timer(timeout_print, 1, "[*] Time irq\n");
    enable_irq();
    asm volatile(
        "mov x0, 0x0\n\t"
        "msr spsr_el1, x0\n\t"
        "msr tpidr_el1, %0\n\t"
        "msr elr_el1, %1\n\t"
        "msr sp_el0, %2\n\t"
        "mov sp, %3\n\t"
        "eret\n\t"
        ::"r"(new_thread),
        "r"(new_thread->code_addr),
        "r"(new_thread->ustack_addr + STACK_SIZE),
        "r"(new_thread->kstack_addr + STACK_SIZE)
        : "x0"
    );
    return 0; 
}

void *load_program(file_info *fileInfo){
    void *thread_code_addr = kmalloc(fileInfo->datasize);
    if(thread_code_addr == NULL) return NULL;

    memcpy(thread_code_addr, fileInfo->data, fileInfo->datasize);

    return thread_code_addr;
}


void sys_fork(TrapFrame *trapFrame){
    int child_pid = do_fork(trapFrame);
    trapFrame->x[0] = child_pid;
}

int do_fork(TrapFrame *trapFrame){
    Thread *curr_thread = get_current();

    void *thread_code_addr = kmalloc(curr_thread->code_size);
    if(thread_code_addr == NULL) return -1;

    Thread *new_thread = thread_create(curr_thread->code_addr);
    new_thread->code_addr = thread_code_addr;
    new_thread->code_size = curr_thread->code_size;


    /* copy the code */
    memcpy((char *)new_thread->code_addr, (char *)curr_thread->code_addr, new_thread->code_size);
    /* copy user stack */
    memcpy((char *)new_thread->ustack_addr, (char *)curr_thread->ustack_addr, STACK_SIZE);
    /* copy trap frame (kernel stack) */
    TrapFrame *new_trapFrame = (TrapFrame *)((char *)new_thread->kstack_addr + STACK_SIZE - sizeof(TrapFrame));
    memcpy((char*)new_trapFrame, (char *)trapFrame, sizeof(TrapFrame));
    /* copy context */
    memcpy((char *)&new_thread->ctx, (char *)&curr_thread->ctx, sizeof(CpuContext));
    
    /* return pid = 0 (child) */
    new_trapFrame->x[0] = 0;
    /* set new code return to after eret */
    new_trapFrame->elr_el1 = (unsigned long)new_thread->code_addr + 
                            (trapFrame->elr_el1 - (unsigned long)curr_thread->code_addr);
    /* set new code return to after eret */
    new_trapFrame->sp_el0 = ((unsigned long)new_thread->ustack_addr + STACK_SIZE) -
                            (((unsigned long)curr_thread->ustack_addr + STACK_SIZE) - trapFrame->sp_el0);
   
    /* after context switch, child proc will load all reg from kernel stack, and return to el0 */
    new_thread->ctx.lr = (unsigned long)after_fork;
    new_thread->ctx.sp = (unsigned long)new_trapFrame;

    return new_thread->id;
}

void sys_exit(TrapFrame *trapFrame){
    do_exit();
}
/* Terminate the current process. */
void do_exit(){
    disable_irq();
    Thread *exit_thread = get_current();
    exit_thread->state = EXIT;
    
    enable_irq();
    schedule();
}

void sys_mbox_call(TrapFrame *trapFrame){

}
void sys_kill(TrapFrame *trapFrame){

}