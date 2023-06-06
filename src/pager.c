#include "pager.h"
#include "mmu.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef unsigned bit;

typedef struct frame {
    pid_t pid;
    int number;
    bit reference;
} frame_t;

typedef struct frame_table {
    int num_frames;
    int* frames_list;
    int* blocks_list;
} frame_table_t;

typedef struct frame_table_list {
    pid_t pid;
    frame_table_t *table_list;
} frame_table_list_t;

int* disk_blocks;
int blocks_in_use;

frame_t* frames_list;
frame_table_list_t* frame_table_list;

int current_page_clock = 0;

void pager_init(int nframes, int nblocks) {

    // Inicializar quadros de memoria e blocos no disco
    disk_blocks = (int*) malloc(nblocks * sizeof(int));
    frames_list = (frame_t*) malloc(nframes * sizeof(frame_t));
    frame_table_list = (frame_table_list_t*) malloc(sizeof(frame_table_list_t));

    blocks_in_use = 0;

    for (int i = 0; i < nblocks; i++) {
        disk_blocks[i] = 0;
    }

    for (int i = 0; i < nframes; i++) {
        frames_list[i].pid = -1;
        frames_list[i].number = 0;
        frames_list[i].reference = 0;
    }
}

void pager_create(pid_t pid) {
    
}

void *pager_extend(pid_t pid) {
    int *__attribute__((aligned(4096))) ptr;
    unsigned long desiredAddress = 0x60000000;
    ptr = (int *)desiredAddress;
    return ptr;
}

void pager_fault(pid_t pid, void *addr) {
}

int pager_syslog(pid_t pid, void *addr, size_t len) {
    return 0;
}

void pager_destroy(pid_t pid) {
}
