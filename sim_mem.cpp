#include "sim_mem.h"

char main_memory[MEMORY_SIZE];

// constructor
sim_mem::sim_mem(char exe_file_name[], char swap_file_name[], int text_size, int data_size, int bss_size,
                 int heap_stack_size, int page_size) {
    this->text_size = text_size;
    this->data_size = data_size;
    this->heap_stack_size = heap_stack_size;
    this->bss_size = bss_size;
    this->page_size = page_size;
    memset(main_memory, '0', MEMORY_SIZE);
    offset = -1;
    in = -1;
    out = -1;
    arr_size[0] = text_size / page_size;
    arr_size[1] = data_size / page_size;
    arr_size[2] = bss_size / page_size;
    arr_size[3] = heap_stack_size / page_size;

    // open swap file
    swapfile_fd = open(swap_file_name, O_RDWR | O_CREAT | O_EXCL, 0644);
    if (swapfile_fd == -1) {
        // File already exists, delete it and create a new one
        if (unlink(swap_file_name) == -1) {
            perror("ERR");
            exit(EXIT_FAILURE);
        }

        swapfile_fd = open(swap_file_name, O_RDWR | O_CREAT, 0644);
        if (swapfile_fd == -1) {
            perror("ERR");
            exit(EXIT_FAILURE);
        }
    }

    // open exe file
    program_fd = open(exe_file_name, O_RDWR, 0);
    if (program_fd == -1) {
        perror("ERR");
        exit(EXIT_FAILURE);
    }
    size_swap_file = data_size + bss_size + heap_stack_size;
    swap_empty = new bool[size_swap_file / page_size];
    for (int i = 0; i < size_swap_file; ++i) {
        write(swapfile_fd, "0", sizeof(char));
        if (i < size_swap_file / page_size) { swap_empty[i] = false; }
    }
    page_table = initialization();
}


//A function that initializes all pages
page_descriptor **sim_mem::initialization() {
    page_descriptor **temp_tab;
    temp_tab = new page_descriptor *[EX_TAB];
    temp_tab[0] = new page_descriptor[text_size / page_size];
    temp_tab[1] = new page_descriptor[data_size / page_size];
    temp_tab[2] = new page_descriptor[bss_size / page_size];
    temp_tab[3] = new page_descriptor[heap_stack_size / page_size];

    for (int i = 0; i < EX_TAB; ++i) {
        for (int j = 0; j < arr_size[i]; ++j) {
            temp_tab[i][j].valid = false;
            temp_tab[i][j].dirty = false;
            temp_tab[i][j].frame = -1;
            temp_tab[i][j].time_use = -1;
            temp_tab[i][j].swap_index = -1;
        }
    }
    return temp_tab;
}

// load
char sim_mem::load(int address) {
    address_calculation(address);
    if (invalid_add) {
        cout << "ERR" << endl;
        return 0;
    }
    index_time++;
    //if the page not in the ram
    if (!page_table[out][in].valid) {
        // if the page not dirty
        if (!page_table[out][in].dirty) {
            if (out == H_S) {
                cout << "ERR" << endl;
                return 0;
            }
            if (out == BSS) { loading_dynamic(); }
            else {
                loading_exe(out);
            }
        } else {
            //if the page dirty
            loading_swap();
        }
    }
    page_table[out][in].dirty = true;
    page_table[out][in].valid = true;
    page_table[out][in].time_use = index_time;
    return main_memory[page_table[out][in].frame * page_size + offset];
}

// store
void sim_mem::store(int address, char value) {
    address_calculation(address);
    if (invalid_add) { return; }
    index_time++;
    if (out == TXT) {    // If you try to write on the text
        cout << "ERR" << endl;
        return;
    }
    if (!page_table[out][in].valid) {    // if the page not in the main memory
        if (!page_table[out][in].dirty) {      // if is not dirty
            if (out == DATA) {                  // if you try to write on the data
                loading_exe(out);
            } else {                         // if the out is bss or h/s initialize zero
                loading_dynamic();
            }
        } else {
            loading_swap();
        }
    }   // if the page in the main
    main_memory[page_table[out][in].frame * page_size + offset] = value;
    page_table[out][in].dirty = true;
    page_table[out][in].valid = true;
    page_table[out][in].time_use = index_time;

}

// A function that loads a page from exe
void sim_mem::loading_exe(int temp) {
    int cursor = 0;
    for (int i = 0; i < temp; ++i) {
        cursor += arr_size[i];
    }
    if (indexRam >= MEMORY_SIZE / page_size) { ram_full = true; }
    if (ram_full) { indexRam = lru(); }
    page_table[out][in].frame = indexRam;
    lseek(program_fd, (cursor + in) * page_size, SEEK_SET);
    read(program_fd, &main_memory[indexRam * page_size], sizeof(char) * page_size);
    indexRam++;
}

//A function that loads a page for bss or heap_stack
void sim_mem::loading_dynamic() {
    if (indexRam >= MEMORY_SIZE / page_size) { ram_full = true; }
    if (ram_full) { indexRam = lru(); }
    for (int p = 0; p < page_size; ++p) {
        main_memory[indexRam * page_size + p] = '0';
    }
    page_table[out][in].frame = indexRam;
    indexRam++;
}

//A function that loads a page from exe
void sim_mem::loading_swap() {
    if (indexRam >= MEMORY_SIZE / page_size) { ram_full = true; }
    if (ram_full) { indexRam = lru(); }
    int cursor = page_table[out][in].swap_index;
    lseek(swapfile_fd, cursor * page_size, SEEK_SET);
    read(swapfile_fd, &main_memory[indexRam * page_size], sizeof(char) * page_size);
    swap_empty[cursor] = false;
    page_table[out][in].frame = indexRam;
    page_table[out][in].swap_index = -1;
    lseek(swapfile_fd, cursor * page_size, SEEK_SET);
    for (int i = 0; i < page_size; ++i) {
        write(swapfile_fd, "0", sizeof(char));
    }
    indexRam++;
}

/* Main memory management function
* Returns an index to the free space in RAM
 */
int sim_mem::lru() {
    // Finding the page that has not been used for a long time
    int min = INT32_MAX, inx_ram = 0;
    page_descriptor *ptr;
    bool is_text_page;
    for (int i = 0; i < EX_TAB; ++i) {
        for (int j = 0; j < arr_size[i]; ++j) {
            if (page_table[i][j].valid && page_table[i][j].time_use > 0 && page_table[i][j].time_use < min) {
                min = page_table[i][j].time_use;
                ptr = &page_table[i][j];
                if (i == 0) { is_text_page = true; } else { is_text_page = false; }
            }
        }
    }
    //If the minimal page is text we will overwrite it
    if (is_text_page) {
        inx_ram = ptr->frame;
        ptr->frame = -1;
        ptr->valid = false;
        return inx_ram;
    } else {
        //Otherwise we will find the free space in swap and insert the page there
        for (int k = 0; k < size_swap_file / page_size; ++k) {
            if (!swap_empty[k]) {
                lseek(swapfile_fd, k * page_size, SEEK_SET);
                write(swapfile_fd, &main_memory[ptr->frame * page_size], sizeof(char) * page_size);
                ptr->valid = false;
                inx_ram = ptr->frame;
                ptr->frame = -1;
                ptr->swap_index = k;
                swap_empty[k] = true;
                break;
            }
        }
        return inx_ram; //We will return the index of the free space in memory
    }
}


void sim_mem::address_calculation(int logical_add) {
    if (logical_add > 4095) {
        invalid_add = true;
        return;
    }
    bitset<12> binary(logical_add);
    string res = binary.to_string();
    int page_size_in_bit = (int) log2(page_size);
    out = stoi(res.substr(0, 2), nullptr, 2);
    offset = stoi(res.substr(res.length() - page_size_in_bit), nullptr, 2);
    in = stoi(res.substr(2, 10 - page_size_in_bit), nullptr, 2);
    if (in > (arr_size[out] - 1)) {
        invalid_add = true;
        return;
    } else invalid_add = false;
}

// print memory
void sim_mem::print_memory() {
    int i;
    printf("\n Physical memory\n");
    for (i = 0; i < MEMORY_SIZE; i++) {
        printf("[%c]\n", main_memory[i]);
    }
}

// print swap
void sim_mem::print_swap() {
    char *str = (char *) malloc(this->page_size * sizeof(char));
    int i;
    printf("\n Swap memory\n");
    lseek(swapfile_fd, 0, SEEK_SET); // go to the start of the file
    while (read(swapfile_fd, str, this->page_size) == this->page_size) {
        for (i = 0; i < page_size; i++) {
            printf("%d - [%c]\t", i, str[i]);
        }
        printf("\n");
    }
    free(str);
}

// print page table
void sim_mem::print_page_table() {
    int i;
    int num_of_txt_pages = text_size / page_size;
    int num_of_data_pages = data_size / page_size;
    int num_of_bss_pages = bss_size / page_size;
    int num_of_stack_heap_pages = heap_stack_size / page_size;
    printf("Valid\t Dirty\t Frame\t Swap index\n");
    for (i = 0; i < num_of_txt_pages; i++) {
        printf("[%d]\t[%d]\t[%d]\t[%d]\n",
               page_table[0][i].valid,
               page_table[0][i].dirty,
               page_table[0][i].frame,
               page_table[0][i].swap_index);

    }
    printf("Valid\t Dirty\t Frame\t Swap index\n");
    for (i = 0; i < num_of_data_pages; i++) {
        printf("[%d]\t[%d]\t[%d]\t[%d]\n",
               page_table[1][i].valid,
               page_table[1][i].dirty,
               page_table[1][i].frame,
               page_table[1][i].swap_index);

    }
    printf("Valid\t Dirty\t Frame\t Swap index\n");
    for (i = 0; i < num_of_bss_pages; i++) {
        printf("[%d]\t[%d]\t[%d]\t[%d]\n",
               page_table[2][i].valid,
               page_table[2][i].dirty,
               page_table[2][i].frame,
               page_table[2][i].swap_index);
    }
    printf("Valid\t Dirty\t Frame\t Swap index\n");
    for (i = 0; i < num_of_stack_heap_pages; i++) {
        printf("[%d]\t[%d]\t[%d]\t[%d]\n",
               page_table[3][i].valid,
               page_table[3][i].dirty,
               page_table[3][i].frame,
               page_table[3][i].swap_index);
    }

}

sim_mem::~sim_mem() {
    for (int i = 0; i < EX_TAB; ++i) {
        delete[] (page_table[i]);
    }
    delete[](page_table);
    delete[](swap_empty);
    close(program_fd);
    close(swapfile_fd);
}



