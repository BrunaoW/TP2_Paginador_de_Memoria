#include <sys/mman.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "pager.h"
#include "mmu.h"

typedef struct frame
{
    pid_t pid;
    int page_number;
    short free_frame;
    short reference_bit;
    short none;
    short wrote;
} frame_t;

typedef struct page_table
{
    int num_pages;
    int *frames;
    int *blocks;
} page_table_t;

typedef struct table_list
{
    pid_t pid;
    int num_pages;
    int *frames;
    int *blocks;
} table_list_t;

int *block_vector;
int free_blocks;
int size_block_vector;

frame_t *frame_vector;
int clock_ptr;
int size_frame_vector;

table_list_t *table_list;
int size_table_list;

void pager_init(int nframes, int nblocks)
{
    int i;
    size_table_list = 1;

    block_vector = (int *)malloc(nblocks * sizeof(int));
    frame_vector = (frame_t *)malloc(nframes * sizeof(frame_t));
    table_list = (table_list_t *)malloc(size_table_list * sizeof(table_list_t));

    free_blocks = nblocks;
    size_block_vector = nblocks;
    for (i = 0; i < nblocks; i++)
        block_vector[i] = 0;

    clock_ptr = 0;
    size_frame_vector = nframes;
    for (i = 0; i < nframes; i++)
    {
        frame_vector[i].pid = -1;
        frame_vector[i].page_number = 0;
        frame_vector[i].free_frame = 0;
        frame_vector[i].reference_bit = 0;
        frame_vector[i].none = 1;
        frame_vector[i].wrote = 0;
    }
}

void pager_create(pid_t pid)
{
    int i, j, num_pages, flag = 0;
    num_pages = (UVM_MAXADDR - UVM_BASEADDR + 1) / sysconf(_SC_PAGESIZE);

    for (i = 0; i < size_table_list; i++)
    {
        if (table_list[i].num_pages == 0)
        {
            table_list[i].pid = pid;
            table_list[i].num_pages = num_pages;
            table_list[i].frames = (int *)malloc(num_pages * sizeof(int));
            table_list[i].blocks = (int *)malloc(num_pages * sizeof(int));

            for (j = 0; j < num_pages; j++)
            {
                table_list[i].frames[j] = -1;
                table_list[i].blocks[j] = -1;
            }
            flag = 1;
            break;
        }
    }

    if (flag == 0)
    {
        table_list = realloc(table_list, (100 + size_table_list) * sizeof(table_list_t));

        table_list[size_table_list].pid = pid;
        table_list[size_table_list].num_pages = num_pages;
        table_list[size_table_list].frames = (int *)malloc(num_pages * sizeof(int));
        table_list[size_table_list].blocks = (int *)malloc(num_pages * sizeof(int));
        for (j = 0; j < num_pages; j++)
        {
            table_list[size_table_list].frames[j] = -1;
            table_list[size_table_list].blocks[j] = -1;
        }
        j = size_table_list + 1;
        size_table_list += 100;
        for (; j < size_table_list; j++)
        {
            table_list[j].num_pages = 0;
        }
    }
}

void *pager_extend(pid_t pid)
{
    if (free_blocks == 0)
        return NULL;

    int i, j, block;

    for (i = 0; i < size_block_vector; i++)
    {
        if (block_vector[i] == 0)
        {
            block_vector[i] = 1;
            free_blocks--;
            block = i;
            break;
        }
    }

    for (i = 0; i < size_table_list; i++)
    {
        if (table_list[i].pid == pid)
        {
            for (j = 0; j < table_list[i].num_pages; j++)
            {
                if (table_list[i].blocks[j] == -1)
                {
                    table_list[i].blocks[j] = block;
                    break;
                }

                if (j == (table_list[i].num_pages) - 1)
                    return NULL;
            }
            break;
        }
    }

    return (void *)(UVM_BASEADDR + (intptr_t)(j * sysconf(_SC_PAGESIZE)));
}

void pager_fault(pid_t pid, void *vaddr)
{
    int i, index, index2, page_num, curr_frame, new_frame, curr_block, new_block,
        move_disk_pid, move_disk_pnum, mem_no_none;
    void *addr;

    for (i = 0; i < size_table_list; i++)
    {
        if (table_list[i].pid == pid)
        {
            index = i;
            break;
        }
    }

    page_num = ((((intptr_t)vaddr) - UVM_BASEADDR) / (sysconf(_SC_PAGESIZE)));

    mem_no_none = 1;
    for (i = 0; i < size_frame_vector; i++)
    {
        if (frame_vector[i].none == 1)
        {
            mem_no_none = 0;
            break;
        }
    }
    if (table_list[index].frames[page_num] != -1 && table_list[index].frames[page_num] != -2)
    {
        curr_frame = table_list[index].frames[page_num];
        mmu_chprot(pid, vaddr, PROT_READ | PROT_WRITE);
        frame_vector[curr_frame].reference_bit = 1;
        frame_vector[curr_frame].wrote = 1;
    }
    else
    {
        if (mem_no_none)
        {
            for (i = 0; i < size_frame_vector; i++)
            {
                addr = (void *)(UVM_BASEADDR + (intptr_t)(frame_vector[i].page_number * sysconf(_SC_PAGESIZE)));
                mmu_chprot(frame_vector[i].pid, addr, PROT_NONE);
                frame_vector[i].none = 1;
            }
        }
        new_frame = -1;
        while (new_frame == -1)
        {
            new_frame = -1;
            if (frame_vector[clock_ptr].reference_bit == 0)
            {
                new_frame = clock_ptr;
                if (frame_vector[clock_ptr].free_frame == 1)
                {
                    move_disk_pid = frame_vector[clock_ptr].pid;
                    move_disk_pnum = frame_vector[clock_ptr].page_number;
                    for (i = 0; i < size_table_list; i++)
                    {
                        if (table_list[i].pid == move_disk_pid)
                        {
                            index2 = i;
                        }
                    }

                    curr_block = table_list[index2].blocks[move_disk_pnum];
                    mmu_nonresident(pid, (void *)(UVM_BASEADDR + (intptr_t)(move_disk_pnum * sysconf(_SC_PAGESIZE))));
                    if (frame_vector[clock_ptr].wrote == 1)
                    {
                        mmu_disk_write(clock_ptr, curr_block);
                        table_list[index2].frames[move_disk_pnum] = -2;
                    }
                    else
                    {
                        table_list[index2].frames[move_disk_pnum] = -1;
                    }
                }
                frame_vector[clock_ptr].pid = pid;
                frame_vector[clock_ptr].page_number = page_num;
                frame_vector[clock_ptr].free_frame = 1;
                frame_vector[clock_ptr].reference_bit = 1;
                frame_vector[clock_ptr].none = 0;
                if (table_list[index].frames[page_num] == -2)
                {
                    new_block = table_list[index].blocks[page_num];
                    mmu_disk_read(new_block, new_frame);
                    frame_vector[clock_ptr].wrote = 1;
                }
                else
                {
                    mmu_zero_fill(new_frame);
                    frame_vector[clock_ptr].wrote = 0;
                }
                table_list[index].frames[page_num] = new_frame;
                mmu_resident(pid, vaddr, new_frame, PROT_READ);
            }
            else
            {
                frame_vector[clock_ptr].reference_bit = 0;
            }
            clock_ptr++;
            clock_ptr %= size_frame_vector;
        }
    }
}

int pager_syslog(pid_t pid, void *addr, size_t len)
{
    int i, j, index, frame_limit, flag;
    char *message = (char *)malloc(len + 1);

    for (i = 0; i < size_table_list; i++)
    {
        if (table_list[i].pid == pid)
        {
            index = i;
            break;
        }
    }

    for (i = 0; i < table_list[index].num_pages; i++)
    {
        if (table_list[index].frames[i] == -1)
        {
            frame_limit = i;
            break;
        }
    }

    for (i = 0; i < len; i++)
    {
        flag = 1;
        for (j = 0; j < frame_limit; j++)
        {
            if (((intptr_t)addr + i - UVM_BASEADDR) / (sysconf(_SC_PAGESIZE)) == table_list[index].frames[j])
            {
                flag = 0;
                break;
            }
        }

        if (flag)
            return -1;

        int pag = ((((intptr_t)addr + i)) - UVM_BASEADDR) / (sysconf(_SC_PAGESIZE));
        int frame_index = table_list[index].frames[pag];
        message[i] = pmem[(frame_index * sysconf(_SC_PAGESIZE)) + i];
        printf("%02x", (unsigned)message[i]);
        if (i == len - 1)
            printf("\n");
    }

    return 0;
}

void pager_destroy(pid_t pid)
{
    int i;
    for (i = 0; i < size_table_list; i++)
    {
        if (table_list[i].pid == pid)
        {
            table_list[i].pid = 0;
            table_list[i].frames = 0;
            table_list[i].blocks = 0;
            free_blocks++;
        }
    }
}