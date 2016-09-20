/*
Names: Anna Bailey, Daniel Wong
Group: #3
Class: CSE 438 - T&TH @ 4:30pm
Description:   Assignment 1 
File: queueLib.h
*/

#ifndef QUEUELIB_H
#define QUEUELIB_H

typedef struct squeue{
    struct msg *msg_q[10];
    int count; 
    int head;
    int tail;
} *squeuep;
 
/* Structure of message */
typedef struct msg {
    int msg_id;      // global sequence number of message
    int source_id;  
    int dest_id;
    struct timespec start_time; 
    char msg_string[80];  // arbitrary character message with uniformly distributed length of 10 to 80  
} *msgp;

struct squeue* sq_create();
int sq_write(struct msg*, struct squeue);
struct msg* sq_read(struct squeue*);
void sq_delete(struct squeue*);


#endif
