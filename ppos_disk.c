#include "ppos_disk.h"
#include "disk.h"
#include <signal.h>

static disk_t disk;

void disk_mgr_dispatcher () {
    while (1) {
        
        task_yield();
    }
}

void disk_mgr_handler(int signum) {
    printf("sinal: %d", signum);
}

int disk_mgr_init (int *numBlocks, int *blockSize) {
    if (disk_cmd (DISK_CMD_INIT, 0, 0) < 0) {
        return -1;
    }

    disk.numblocks = disk_cmd (DISK_CMD_DISKSIZE, 0, 0) ;
    disk.blocksize = (DISK_CMD_BLOCKSIZE, 0, 0) ;

    if (disk.numblocks < 0 || disk.blocksize < 0) {
        return -1;
    }

    *numBlocks = disk.numblocks;
    *blockSize = disk.blocksize;

    disk.action.sa_handler = disk_mgr_handler ;
    sigemptyset (&disk.action.sa_mask) ;
    disk.action.sa_flags = 0 ;
    if (sigaction (SIGUSR1, &disk.action, 0) < 0)
    {
        perror ("Erro em sigusr1: ") ;
        return -1;
    }
    return -1;
}

int disk_block_read (int block, void *buffer) {
    return -1;
}

int disk_block_write (int block, void *buffer) {
    return -1;
}