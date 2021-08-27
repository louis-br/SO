#include "ppos_disk.h"

static disk_t disk;

disk_request_t *disk_mgr_scheduler() {
    // FCFS scheduler
    if (disk.requestQueue != NULL) {
        return disk.requestQueue;
    }
    return NULL;
}

void disk_mgr_dispatcher () {
    //print("hello dispatcher\n");
    while (1) {
        //sem_down(&disk.semaphore);
        if (disk.signal && disk.currentRequest != NULL) {
            disk.signal = 0;
            task_resume(disk.currentRequest->task);
            disk_request_t *request = disk.currentRequest;
            disk.currentRequest = NULL;
            free(request);
        }

        if (disk.currentRequest == NULL && disk_cmd(DISK_CMD_STATUS, 0, 0) == DISK_STATUS_IDLE && (disk.requestQueue != NULL)) {
            disk_request_t *request = disk_mgr_scheduler();
            disk.currentRequest = request;
            if (request->write) {
                disk_cmd(DISK_CMD_WRITE, request->block, request->buffer);
            }
            else {
                disk_cmd(DISK_CMD_READ, request->block, request->buffer);
            }
            queue_remove((queue_t**)&disk.requestQueue, (queue_t*)request);
        }
        //sem_up(&disk.semaphore);
        //print("exec: %d diskdisp: %d disp: %d\n", taskExec->id, disk.dispatcher.id, taskDisp->id);
        task_suspend(taskExec, &disk.taskQueue);
        task_switch(taskDisp);
    }
}

void disk_mgr_handler(int signum) {
    disk.signal = 1;
    task_switch(&disk.dispatcher);
    //disk_mgr_dispatcher(1);
    //print("sinal: %d", signum);
}

int disk_mgr_init (int *numBlocks, int *blockSize) {
    task_create(&disk.dispatcher, &disk_mgr_dispatcher, NULL);

    disk.blocksize = 0;
    disk.numblocks = 0;
    disk.requestQueue = NULL;

    //print("init %p\n", disk.requestQueue);

    //sem_create(&disk.semaphore, 0);

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

disk_request_t *disk_request_new(task_t *task, int write, int block, void *buffer) {//, void *buffer) {
    disk_request_t *request = (disk_request_t*)malloc(sizeof(disk_request_t));
    request->task = task;
    request->write = write;
    request->block = block;
    request->buffer = buffer;
    return request;
}

int disk_request_exec(int write, int block, void *buffer) {
    task_t *task = taskExec;
    disk_request_t *request = disk_request_new(task, write, block, buffer);

    //sem_down(&disk.semaphore);

    queue_append((queue_t**)&disk.requestQueue, (queue_t*)request);
    int state = disk.dispatcher.state;
    if (state == 't') { //PPOS_TASK_STATE_TERMINATED
        return -1;
    }
    //print("resume state: %c", state);
    if (state == 's' || state == 'r') { //PPOS_TASK_STATE_SUSPENDED, PPOS_TASK_STATE_READY
        task_resume(&disk.dispatcher);
    }


    //print("switch\n");
    task_suspend(task, &disk.taskQueue);
    //sem_up(&disk.semaphore);
    task_switch(taskDisp);
    //print("return\n");
    return 0;
}

int disk_block_read (int block, void *buffer) {
    return disk_request_exec(0, block, buffer);
}

int disk_block_write (int block, void *buffer) {
    return disk_request_exec(1, block, buffer);
}