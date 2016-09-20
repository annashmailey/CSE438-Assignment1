/*
Names: Anna Bailey, Daniel Wong
Group: #3
Class: CSE 438 - T&TH @ 4:30pm
Description:   Assignment 1 
File: queueLib.c
*/
 
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "queueLib.h"
 
 
/* sq_create - to create a shared queue of message with a specific length */
struct squeue* sq_create()
{
    struct squeue *cq = malloc(sizeof(struct squeue*));
	int i;
	for(i = 0; i < 10; i = i+1)
	{
		struct msg *q_msg;
		cq->msg_q[i] = q_msg;
	}
	cq->count = 0; 
    cq->head = 0;
    cq->tail = 9;
    return cq;
}
 
/* sq_write - to enqueuer a message. -1 is returned if the queue is full */
int sq_write(struct msg *msg_addr, struct squeue *wq)
{
    if(wq->count < 10)
    {
        wq->tail++;
	wq->tail = wq->tail%10;
	printf("tail is %d\n",wq->tail);
	printf("msg_id = %d\n", msg_addr->msg_id);
        wq->msg_q[wq->tail] = msg_addr;
        wq->count++;
    }
    else
	{
	printf("queue is full\n");
        return -1; 
	}
    return 0;
}
 
/* sq_read - to dequeuer a message. -1 is returned if the queue is empty */
struct msg* sq_read(struct squeue *rq)
{
    struct msg *ret;
	printf("rq addr = %p\n",(void *)rq);
    if(rq->count > 0)
    {
        ret = (rq->msg_q[rq->head]);
	printf("ret->msg_id is %d. Head is %d\n",ret->msg_id, rq->head);
        rq->head++;
	 rq->head = rq->head % 10;
        rq->count = rq->count - 1;
	printf("reading from queue\n count is %d\n", rq->count);
    }
    else
    {
        ret = (struct msg*) malloc(sizeof(struct msg));
	ret->msg_id = -1;
	printf("queue is empty\n");
    }	
    return ret;
}
 
/* sq_delete - to delete a shared queue */
void sq_delete(struct squeue *dq)
{
    free(dq->msg_q);
    free(dq);
}
