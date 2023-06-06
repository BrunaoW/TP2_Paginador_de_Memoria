#include "pager.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

#include "mmu.h"

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
int size_disk_blocks = 0;

int blocks_in_use;

frame_t* frames_list;
int size_frames_list = 0;

frame_table_list_t* frame_table_list;
int size_frame_table_list = 0;

int current_page_clock = 0;

void pager_init(int nframes, int nblocks) {
    size_frame_table_list = 1;

    // Inicializar quadros de memoria e blocos no disco
    disk_blocks = (int*) malloc(nblocks * sizeof(int));
    frames_list = (frame_t*) malloc(nframes * sizeof(frame_t));
    frame_table_list = (frame_table_list_t*) malloc(sizeof(frame_table_list_t));

    blocks_in_use = 0;
    size_disk_blocks = nblocks;

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
	int num_frames = 0;
	num_frames = (UVM_MAXADDR - UVM_BASEADDR + 1) / sysconf(_SC_PAGESIZE);
    bit found_empty_page = 0;
    
    for (int i = 0; i < size_frame_table_list; i++)
    {
        if (frame_table_list[i].table_list != NULL) {
            continue;
        }

        frame_table_list[i].pid = pid;
        
        frame_table_list[i].table_list = (frame_table_t*) malloc(sizeof(frame_table_t));
        frame_table_list[i].table_list->num_frames = num_frames;

        frame_table_list[i].table_list->blocks_list = (int*) malloc(num_frames * sizeof(int));
        frame_table_list[i].table_list->frames_list = (int*) malloc(num_frames * sizeof(int));
    
        for (int j = 0; j < num_frames; j++) {
            frame_table_list[i].table_list->frames_list[j] = -1; 
            frame_table_list[i].table_list->blocks_list[j] = -1;
        }
        found_empty_page = 1;
        break;
    }
    
    if (found_empty_page) {
		frame_table_list = realloc(frame_table_list, (100 + size_frame_table_list) * sizeof(frame_table_list_t));

		frame_table_list[size_frame_table_list].pid = pid;
		frame_table_list[size_frame_table_list].table_list = (frame_table_t*) malloc (sizeof(frame_table_t));
		frame_table_list[size_frame_table_list].table_list->frames_list = (int*) malloc (num_frames * sizeof(int));
		frame_table_list[size_frame_table_list].table_list->num_frames = num_frames;
		frame_table_list[size_frame_table_list].table_list->blocks_list = (int*) malloc (num_frames * sizeof(int));
		for(int j = 0; j < num_frames; j++)
		{
			frame_table_list[size_frame_table_list].table_list->blocks_list[j] = -1;
			frame_table_list[size_frame_table_list].table_list->frames_list[j] = -1;
		}

		size_frame_table_list += 100;
		for(int j = size_frame_table_list + 1; j < size_frame_table_list; j++)
		{
			frame_table_list[j].table_list = NULL;
		}
    }
}

void *pager_extend(pid_t pid) {
    if ((size_disk_blocks - blocks_in_use) == 0) {
        return NULL;
    }

    int block_pos, j;

    for (int i = 0; i < size_disk_blocks; i++) {
        if (disk_blocks[i] == 0) {
            disk_blocks[i] = 1;
            blocks_in_use++;
            block_pos = i;
            break;
        }
    }

    for (int i = 0; i < size_frame_table_list; i++) {
		if(frame_table_list[i].pid != pid) {
            continue;
        }

        for (j = 0; j < frame_table_list[i].table_list->num_frames; j++)
        {
            if (frame_table_list[i].table_list->blocks_list[j] == -1) {
                frame_table_list[i].table_list->blocks_list[j] = block_pos;
                break;
            }

            if ((frame_table_list[i].table_list->num_frames - 1) == j) {
                return NULL;
            }
        }
        
    }

    return (void*) (UVM_BASEADDR + (intptr_t) (j * sysconf(_SC_PAGESIZE)));
}

void pager_fault(pid_t pid, void *addr) {
	void *temp_addr;
    int i, index;

	for (i = 0; i < size_frame_table_list; i++) {
		if (frame_table_list[i].pid == pid) {
			index = i;
			break;
		}
	}

	int page_num = ((((intptr_t) addr) - UVM_BASEADDR) / (sysconf(_SC_PAGESIZE)));
    int current_frame, temp_frame, temp_disk_block;

	if (frame_table_list[index].table_list->frames_list[page_num] != -1 && 
       frame_table_list[index].table_list->frames_list[page_num] != -2
    ) {
		current_frame = frame_table_list[index].table_list->frames_list[page_num];
		mmu_chprot(pid, addr, PROT_READ | PROT_WRITE);
		frames_list[current_frame].reference = 1;
	} else {
        for (i = 0; i < size_frames_list; i++) {
            temp_addr = (void*) (UVM_BASEADDR + (intptr_t) (frames_list[i].number * sysconf(_SC_PAGESIZE)));
            mmu_chprot(frames_list[i].pid, temp_addr, PROT_NONE);
        }
		temp_frame = -1;
		while (temp_frame == -1) {
			temp_frame = -1;
			if (frames_list[current_page_clock].reference == 0) {
				temp_frame = current_page_clock;
				frames_list[current_page_clock].pid = pid;
				frames_list[current_page_clock].number = page_num;
				frames_list[current_page_clock].reference = 1;

				if(frame_table_list[index].table_list->frames_list[page_num] == -2) {
					temp_disk_block = frame_table_list[index].table_list->blocks_list[page_num];
					mmu_disk_read(temp_disk_block, temp_frame);
				}
				else
				{
					mmu_zero_fill(temp_frame);
				}
				frame_table_list[index].table_list->frames_list[page_num] = temp_frame;
				mmu_resident(pid, addr, temp_frame, PROT_READ);
			}
			else
			{
				frames_list[current_page_clock].reference = 0;
			}
			current_page_clock++;
			current_page_clock %= size_frames_list;
		}
	}
}

int pager_syslog(pid_t pid, void *addr, size_t len) {
    return 0;
}

void pager_destroy(pid_t pid) {
}
