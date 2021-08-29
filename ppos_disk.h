// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.2 -- Julho de 2017

// interface do gerente de disco rígido (block device driver)

#ifndef __DISK_MGR__
#define __DISK_MGR__

#include "ppos.h"
#include "ppos-core-globals.h"
#include "disk.h"
#include <signal.h>

// estruturas de dados e rotinas de inicializacao e acesso
// a um dispositivo de entrada/saida orientado a blocos,
// tipicamente um disco rigido.

// estrura que representa um pedido de escrita ou leitura no disco
typedef struct disk_request_t
{
  struct disk_request_t *prev, *next ;		// ponteiros para usar em filas
  task_t *task;
  int write; // 0 = leitura, 1 = escrita
  int block;
  void *buffer;
} disk_request_t ;

// estrutura que representa um disco no sistema operacional
typedef struct
{
  // completar com os campos necessarios
  task_t dispatcher;
  int numblocks;
  int blocksize;
  int lastBlock;
  int totalSeekTime;
  task_t *taskQueue;
  disk_request_t *requestQueue;
  disk_request_t *currentRequest;
  int signal;
  semaphore_t semaphore;
  struct sigaction action; // estrutura que define um tratador de sinal (deve ser global ou static)
} disk_t ;

// inicializacao do gerente de disco
// retorna -1 em erro ou 0 em sucesso
// numBlocks: tamanho do disco, em blocos
// blockSize: tamanho de cada bloco do disco, em bytes
int disk_mgr_init (int *numBlocks, int *blockSize) ;

// leitura de um bloco, do disco para o buffer
int disk_block_read (int block, void *buffer) ;

// escrita de um bloco, do buffer para o disco
int disk_block_write (int block, void *buffer) ;

#endif
