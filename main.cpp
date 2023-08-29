#include "sim_mem.h"

int main() {

    sim_mem a((char *) "exec_file", (char *) "swap_file", 16, 84, 124, 16, 4);
    // write on the text page
    a.store(0, 'c');// ERR1
    a.store(4, 'h');// ERR2
    a.store(8, 'a');// ERR3
    a.store(12, 'n');// ERR4
// load the text page
    for (int i = 0; i <= 12; i += 4) {
        a.load(i);
    }
    // write on the data page
    for (int j = 1024; j <= 1104; j += 4) {
        a.store(j, 'C');
        a.store(j + 1, 'H');
        a.store(j + 2, 'A');
        a.store(j + 3, 'N');
    }
    //write on the bss page
    for (int j = 2048; j <= 2168; j += 4) {
        a.store(j, '1');
        a.store(j + 1, '2');
        a.store(j + 2, '3');
        a.store(j + 3, '4');
    }


    a.print_page_table();
    a.print_memory();
    a.print_swap();
}


