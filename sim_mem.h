#ifndef EX4_SIM_MEM_H
#define EX4_SIM_MEM_H

#include <string>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <bitset>
#include <cmath>

#define MEMORY_SIZE 200
#define EX_TAB 4
#define H_S 3
#define BSS 2
#define DATA 1
#define TXT 0

extern char main_memory[MEMORY_SIZE];
using namespace std;

//Structure of the byte table
typedef struct page_descriptor {
    bool valid;
    int frame;
    bool dirty;
    int swap_index;
    int time_use;
} page_descriptor;


class sim_mem {
    int swapfile_fd; //swap file fd
    int program_fd; //executable file fd
    bool *swap_empty;
    bool ram_full = false,invalid_add= false;
    int text_size;
    int data_size;
    int bss_size;
    int heap_stack_size;
    int page_size;
    int index_time = 0;
    int indexRam = 0;
    int arr_size[EX_TAB]{}, offset, in, out, size_swap_file;
    page_descriptor **page_table; //pointer to page table
public:
    sim_mem(char exe_file_name[], char swap_file_name[], int text_size,
            int data_size, int bss_size, int heap_stack_size,
            int page_size);

    ~sim_mem();

    char load(int address);

    void store(int address, char value);

    void loading_exe(int offset);

    void loading_swap();

    void loading_dynamic();

    int lru();

    void print_memory();

    void print_swap();

    void print_page_table();

    page_descriptor **initialization();

    void address_calculation(int logical_add);
};

#endif //EX4_SIM_MEM_H
