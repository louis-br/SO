#include "ppos_disk.h"

static disk_t disk;

//macro que define o escalonador escolhido a partir do nome da função
//mude aqui ou inclua -DDISK_MGR_SCHEDULER=(nome da função do escalonador) nos argumentos do compilador
#ifndef DISK_MGR_SCHEDULER
#define DISK_MGR_SCHEDULER disk_mgr_scheduler
#endif 

void add_seek_time(int new, int last) {
    int diff = new - last;
    if (diff < 0) {
        diff = -diff;
    }
    printf("blocos percorridos: %d + %d = ", disk.totalSeekTime, diff);
    disk.totalSeekTime += diff;
    printf("%d\n", disk.totalSeekTime);
}

disk_request_t *disk_mgr_scheduler() {
    // FCFS scheduler
    if (disk.requestQueue != NULL) {
        add_seek_time(disk.requestQueue->block, disk.lastBlock);
        return disk.requestQueue;
    }
    return NULL;
}

disk_request_t *disk_mgr_scheduler_sstf() {
    // SSTF scheduler
    if (disk.requestQueue == NULL) {
        return NULL;
    }

    int lastBlock = disk.lastBlock;
    int minDiff = disk.numblocks + 1;
    disk_request_t *min = disk.requestQueue;
    disk_request_t *aux = disk.requestQueue;
    do {
        if (min != aux) {
            int block = aux->block;
            int diff = block - lastBlock;
            if (diff < 0) {
                diff = -diff;
            }
            if (diff < minDiff) {
                minDiff = diff;
                min = aux;
            }
        }
        aux = aux->next;
    } while (aux != NULL && aux != disk.requestQueue);
    add_seek_time(min->block, lastBlock);
    return min;
}

disk_request_t *disk_mgr_scheduler_cscan() {
    // CSCAN scheduler
    if (disk.requestQueue == NULL) {
        return NULL;
    }

    int lastBlock = disk.lastBlock;
    int minBlock = disk.numblocks + 1;
    int minGlobalBlock = minBlock;
    disk_request_t *min = disk.requestQueue;
    disk_request_t *minGlobal = disk.requestQueue;
    disk_request_t *aux = disk.requestQueue;
    do {
        int block = aux->block;
        if (block < minGlobalBlock) {
            minGlobalBlock = block;
            minGlobal = aux;
        }
        if (block >= lastBlock && block < minBlock) {
            minBlock = block;
            min = aux;
        }
        aux = aux->next;
    } while (aux != NULL && aux != disk.requestQueue);
    if (min == NULL) {
        min = minGlobal;
    }
    add_seek_time(min->block, lastBlock);
    return min;
}

void disk_mgr_dispatcher () {
    while (1) {
        sem_down(&disk.semaphore);

        if (disk.currentRequest != NULL && disk.signal) {
            disk.signal = 0;
            disk_request_t *request = disk.currentRequest;
            if (request->task->state == 's') { //PPOS_TASK_STATE_SUSPENDED
                task_resume(request->task);
            }
            disk.currentRequest = NULL;
            free(request);
        }

        if (disk.currentRequest == NULL && (disk_cmd(DISK_CMD_STATUS, 0, 0) == DISK_STATUS_IDLE) && (disk.requestQueue != NULL)) {
            disk_request_t *request = DISK_MGR_SCHEDULER();
            int status = -1;
            if (request->write) {
                status = disk_cmd(DISK_CMD_WRITE, request->block, request->buffer);
            }
            else {
                status = disk_cmd(DISK_CMD_READ, request->block, request->buffer);
            }
            if (status >= 0) {
                disk.currentRequest = request;
                disk.lastBlock = request->block;
                queue_remove((queue_t**)&disk.requestQueue, (queue_t*)request);
            }
            #ifdef DEBUG
            printf("request block: %d write: %d status: %d buffer: %s\n", request->block, request->write, status, (char*)request->buffer);
            #endif
        }
        task_suspend(taskExec, &disk.taskQueue);

        sem_up(&disk.semaphore);
        task_switch(taskDisp);
    }
}

void disk_mgr_handler(int signum) {
    sem_down(&disk.semaphore);
    disk.signal = 1;
    sem_up(&disk.semaphore);
    task_switch(&disk.dispatcher);
}

int disk_mgr_init (int *numBlocks, int *blockSize) {
    task_create(&disk.dispatcher, &disk_mgr_dispatcher, NULL);

    disk.numblocks = 0;
    disk.blocksize = 0;
    disk.lastBlock = 0;
    disk.totalSeekTime = 0;
    disk.taskQueue = NULL;
    disk.requestQueue = NULL;
    disk.currentRequest = NULL;
    disk.signal = 0;

    sem_create(&disk.semaphore, 1);

    if (disk_cmd (DISK_CMD_INIT, 0, 0) < 0) {
        return -1;
    }

    disk.numblocks = disk_cmd(DISK_CMD_DISKSIZE, 0, 0);
    disk.blocksize = disk_cmd(DISK_CMD_BLOCKSIZE, 0, 0);

    if (disk.numblocks < 0 || disk.blocksize < 0) {
        return -1;
    }

    *numBlocks = disk.numblocks;
    *blockSize = disk.blocksize;

    disk.action.sa_handler = disk_mgr_handler;
    sigemptyset (&disk.action.sa_mask);
    disk.action.sa_flags = 0;
    if (sigaction (SIGUSR1, &disk.action, 0) < 0)
    {
        perror ("Erro em sigusr1: ");
        return -1;
    }
    return 0;
}

disk_request_t *disk_request_new(task_t *task, int write, int block, void *buffer) {
    disk_request_t *request = (disk_request_t*)malloc(sizeof(disk_request_t));
    if (request == NULL) {
        return NULL;
    }
    request->prev = NULL;
    request->next = NULL;
    request->task = task;
    request->write = write;
    request->block = block;
    request->buffer = buffer;
    return request;
}

int disk_request_exec(int write, int block, void *buffer) {
    task_t *task = taskExec;
    disk_request_t *request = disk_request_new(task, write, block, buffer);
    
    if (request == NULL) {
        return -1;
    }

    sem_down(&disk.semaphore);

    queue_append((queue_t**)&disk.requestQueue, (queue_t*)request);
    task_t **taskQueue = &disk.taskQueue;
    task_t *dispatcher = &disk.dispatcher;
    int state = dispatcher->state;
    
    sem_up(&disk.semaphore);

    if (state == 't') { //PPOS_TASK_STATE_TERMINATED
        return -1;
    }
    if (state == 's' || state == 'r') { //PPOS_TASK_STATE_SUSPENDED, PPOS_TASK_STATE_READY
        task_resume(dispatcher); //task_resume não pode ser usado em tarefas suspensas por semáforos, libere os semáforos antes de chamar
    }

    task_suspend(task, taskQueue);
    task_switch(taskDisp);
    return 0;
}

int disk_block_read (int block, void *buffer) {
    return disk_request_exec(0, block, buffer);
}

int disk_block_write (int block, void *buffer) {
    return disk_request_exec(1, block, buffer);
}