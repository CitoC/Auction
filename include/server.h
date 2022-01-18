#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include "protocol.h"
#include "debug.h"
#include "linkedList.h"

#define BUFFER_SIZE 1024
#define SA struct sockaddr

// the packet that stores the user info and will be stored into a linked list
typedef struct 
{
    char username[20];
    char password[20];
    // 0 = offline
    // 1 = online
    int login_status;
    int client_fd;
}packet_user_info;

// the auction struct that stores all the aution info
typedef struct 
{
    int id;
    char* item_name;
    int duration;
    int max_bid;
    int num_watcher;
    int highest_bid;
    char* highest_bid_user;
    char* creator;
}auction_t;

typedef struct
{
    int msg_type;
    int msg_len;
    char msg_body[30];
    int client_fd;
}job_t;

// the struct for job thread
typedef struct 
{
    job_t* buf;        /* Buffer array */       
    int n;             /* Maximum number of slots */
    int front;         /* buf[(front+1)%n] is first item */
    int rear;          /* buf[rear%n] is last item */
    sem_t mutex;       /* Protects accesses to buf */
    sem_t slots;       /* Counts available slots */
    sem_t items;       /* Counts available items */
}sbuf_t;



int server_init(int server_port);
void main_thread(int server_port);
void *client_thread(void* clientfd_ptr);
void* job_thread();

// sbuf library
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, job_t item);
job_t sbuf_remove(sbuf_t *sp);
#endif
