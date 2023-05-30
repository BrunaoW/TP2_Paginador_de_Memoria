#include "pager.h"

#define MAX_SIZE 100

typedef struct
{
    pid_t data[MAX_SIZE];
    int size;
} PidList;

void addPid(PidList *list, pid_t pid)
{
    if (list->size < MAX_SIZE)
    {
        list->data[list->size] = pid;
        list->size++;
    }
    else
    {
        printf("Full of elements.\n");
    }
}

void removePid(PidList *list, pid_t pid)
{
    int index = -1;
    for (int i = 0; i < list->size; i++)
    {
        if (list->data[i] == pid)
        {
            index = i;
            break;
        }
    }

    if (index != -1)
    {
        for (int i = index; i < list->size - 1; i++)
        {
            list->data[i] = list->data[i + 1];
        }
        list->size--;
    }
    else
    {
        printf("PID not found in the list.\n");
    }
}

PidList pidList;
int _nframes;
int _nblocks;

void pager_init(int nframes, int nblocks)
{
    pidList.size = 0;
    _nframes = nframes;
    _nblocks = nblocks;
}

void pager_create(pid_t pid)
{
    addPid(&pidList, pid);
}

void *pager_extend(pid_t pid)
{
}

void pager_fault(pid_t pid, void *addr)
{
}

int pager_syslog(pid_t pid, void *addr, size_t len)
{
    return 0;
}

void pager_destroy(pid_t pid)
{
    removePid(&pidList, pid);
}
