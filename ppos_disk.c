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
        printf("sem_disp: %d\n", disk.semaphore.value);
        sem_down(&disk.semaphore);
        if (disk.currentRequest != NULL && disk.signal) {
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
        printf("sem_disp: %d\n", disk.semaphore.value);
        task_suspend(taskExec, &disk.taskQueue);//disk.taskQueue
        sem_up(&disk.semaphore);
        //print("exec: %d diskdisp: %d disp: %d\n", taskExec->id, disk.dispatcher.id, taskDisp->id);
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

    //printf("sem: %d\n", disk.semaphore.value);

    sem_down(&disk.semaphore);

    queue_append((queue_t**)&disk.requestQueue, (queue_t*)request);
    task_t* taskQueue = disk.taskQueue;
    task_t* dispatcher = &disk.dispatcher;
    int state = dispatcher->state;
    
    sem_up(&disk.semaphore);

    if (state == 't') { //PPOS_TASK_STATE_TERMINATED
        return -1;
    }
    //print("resume state: %c", state);
    if (state == 's' || state == 'r') { //PPOS_TASK_STATE_SUSPENDED, PPOS_TASK_STATE_READY
        task_resume(dispatcher);//task_suspend(&disk.dispatcher, &readyQueue);  //task_resume não pode ser usado em tarefas suspensas por semáforos, libere os semáforos antes de chamar
    }

    //print("switch\n");
    //task_suspend(task, &sleepQueue);
    //printf("sem: %d active: %d\n", disk.semaphore.value, disk.semaphore.active);
    //disk.semaphore.value = 0;
    //printf("sem_down taskQueue\n");
    task_suspend(task, &taskQueue); //disk.taskQueue
    //printf("sem_up taskQueue\n");
    task_switch(taskDisp);
    //print("return\n");
    //while (1) {};
    return 0;
}

int disk_block_read (int block, void *buffer) {
    return disk_request_exec(0, block, buffer);
}

int disk_block_write (int block, void *buffer) {
    return disk_request_exec(1, block, buffer);
}