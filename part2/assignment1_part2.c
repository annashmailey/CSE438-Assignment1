#include <stdio.h>
//#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <semaphore.h>
#include <string.h>

#define BASE_PERIOD 1000 //in ms
int msg_count = 0; //# of messages created
int msg_drop_count = 0;
int msg_received_count = 0; //# of messages received by receiver
int msg_forwarded_count = 0; //# of messages forwarded through deamon
int finished = 0; //used to end thread loops. Changed to 1, when the final bus_out is cleared. 
struct timespec accum_time[2000];// accum time for each finished thread, used for avg and stdev

static sem_t mutex;

struct arg_struct_sender {
    int priority;
	struct timespec period;
	struct squeue *bus_in;
};

struct arg_struct_receiver {
    int priority;
	struct timespec period;
	struct squeue *bus_out;
};

struct arg_struct_daemon {
    int priority;
	struct timespec period;
	struct squeue *bus_in;
	struct squeue *bus_out1;
	struct squeue *bus_out2;
	struct squeue *bus_out3;
};

void *sender_thread(void* arg)
{
	struct arg_struct_sender *args = (struct arg_struct_sender *)arg;
	struct timespec period = args->period;
	struct squeue *bus_in_q = args->bus_in;
	struct timespec next, st;
	int i,j, num_msg, dest_id, msg_length, sender_id;
	struct msg *add_msg;
	
	clock_gettime(CLOCK_MONOTONIC, &next);
	while (!finished)	
	{
		sem_wait(&mutex);
		if(msg_count < 2000)
		{
			//randomize number of messages to be made
			num_msg = rand()%4;
			num_msg++;
			if(num_msg + msg_count > 2000)
				num_msg = 2000-msg_count;
			printf("num_msg = %d\n",num_msg);
			for(i = 0; i < num_msg; i++)
			{
				//create message
				dest_id = rand() % 3; //randomize dest_id
				msg_length = rand() % 70;
				msg_length += 10; //randomize msg_length
				add_msg = (struct msg *) malloc(sizeof(struct msg));  
				add_msg->dest_id = dest_id;
				add_msg->msg_string[0] = 'z'; //create message
				for(j = 1; j < msg_length; j++)
				{
					add_msg->msg_string[j] = 'z';
				}
				add_msg->source_id = sender_id;
				msg_count++;
				add_msg->msg_id = msg_count;
				clock_gettime(CLOCK_MONOTONIC, &st);
				add_msg->start_time = st;
				//write to queue
				if(sq_write(add_msg, bus_in_q) == -1)
				{
					msg_drop_count++;
					free(add_msg);
				}

				/*error checking
				if(msg_count % 100 == 0)
					printf("%d Messages Sent\n", msg_count);
				if(msg_drop_count % 100 == 0)
					printf("%d Messages dropped\n", msg_drop_count);*/
			}
		}
		next.tv_nsec = next.tv_nsec + period.tv_nsec;
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, 0);
		sem_post(&mutex);
	}
	pthread_exit(0);
}

void *receiver_thread(void* arg)
{
	struct arg_struct_receiver *args = (struct arg_struct_receiver *)arg;
	struct timespec period = args->period;
	struct squeue *bus_out = args->bus_out;
	struct timespec next, et;
	int num_msg, dest_id, msg_length, sender_id, i, loop;
	struct msg *rem_msg;
	unsigned long avg_time, stdev, ac_time;
	unsigned long var = 0;
	
	clock_gettime(CLOCK_MONOTONIC, &next);
	while(!finished)
	{	
		sem_wait(&mutex);
		loop = 0;
		while(loop!=-1)
		{
		rem_msg = sq_read(bus_out);
		loop = rem_msg->msg_id;
		if(rem_msg->msg_id != -1)
		{
			clock_gettime(CLOCK_MONOTONIC, &et);
			accum_time[rem_msg->msg_id].tv_nsec = et.tv_nsec - rem_msg->start_time.tv_nsec;
			free(rem_msg);
			msg_received_count++;
		}
		else
		{
			free(rem_msg);
			//queue is empty: check if 20k messages
			if((msg_drop_count + msg_received_count) >= 2000)
			{
				if(finished == 0)
				{
					finished++;
					for(i = 0; i < msg_received_count; i++) //calculate sum of accum_time of all messages.
					{
						avg_time += accum_time[i].tv_nsec;
					}
					avg_time = avg_time / 2000; //divide by number of messages.
					avg_time = avg_time / 1000; // convert to ms
					//calculate stdev
				/*	for(i = 0; i<msg_received_count; i++)
					{
						ac_time = accum_time[i].tv_nsec/1000;
						var = var + pow((ac_time - avg_time),2);
					}
					var = var/msg_received_count;
					stdev = sqrt(var);*/
					printf("Program finished.\n");
				//	printf("Total number of messages sent = %d.\n", msg_count);
				//	printf("Total number of messages forwarded through daemon = %d\n", msg_forwarded_count);
					//printf("Total number of messages received = %d\n", msg_received_count);
					printf("Total number of messages dropped = %d.\n", msg_drop_count);
					printf("Average time to complete message = %lums.\n", avg_time);
				//	printf("Standard deviation = %lf\n",stdev);
				}
			}
		}
		}
		next.tv_nsec = next.tv_nsec + period.tv_nsec;
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, 0);
		sem_post(&mutex);
	}
	pthread_exit(0);
}

void *bus_daemon(void* arg)
{
	struct arg_struct_daemon *args = (struct arg_struct_daemon *)arg;
	struct timespec period = args->period;
	struct squeue *bus_in_q = args->bus_in;
	struct squeue *bus_out_q1 = args->bus_out1;
	struct squeue *bus_out_q2 = args->bus_out2;
	struct squeue *bus_out_q3 = args->bus_out3;
	struct timespec next;
	struct msg *fwd_msg;
	int dest_id;
	int loop;
	
	clock_gettime(CLOCK_MONOTONIC, &next);
	while(!finished)
	{
		sem_wait(&mutex);
		//Remove messages from bus_in_q, loop receives output from sq_read, meaning it is -1 when the queue is empty.
		loop = 0;
		while(loop != -1)
		{
			fwd_msg = sq_read(bus_in_q);
			loop = fwd_msg->msg_id;
			if(fwd_msg->msg_id == -1)
			{	
				free(fwd_msg);
			}
			else
			{
				//Forward messages to appropriate bus_out_q
				dest_id = fwd_msg->dest_id;
				switch (dest_id)
				{
					case 0:
						if(sq_write(fwd_msg, bus_out_q1) == -1)
						{
							msg_drop_count++;
							free(fwd_msg);
						}
						else
							msg_forwarded_count++;
						break;
					case 1:
						if(sq_write(fwd_msg, bus_out_q2) == -1)
						{
							msg_drop_count++;
							free(fwd_msg);
						}
						else
							msg_forwarded_count++;
						break;
					case 2:
						if(sq_write(fwd_msg, bus_out_q3) == -1)
						{
							msg_drop_count++;
							free(fwd_msg);
						}
						else
							msg_forwarded_count++;
						break;
					default:
						printf("could not find dest_id");
						break;
				}
			}
		}
		next.tv_nsec = next.tv_nsec + period.tv_nsec;
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, 0);
		sem_post(&mutex);
	}
	sq_delete(bus_in_q);
	sq_delete(bus_out_q1);
	sq_delete(bus_out_q2);
	sq_delete(bus_out_q3);
	pthread_exit(0);
}

int main( )
{
	int i;
	const int period_multiplier[] = {12, 22, 24, 26, 28, 15, 17, 19};
	const int thread_priority[] = {90, 94, 95, 96, 97, 91, 92, 93};
	
	struct arg_struct_sender args_s[4];
	struct arg_struct_daemon args_d;
	struct arg_struct_receiver args_r[3];
	
	pthread_t sender1;
	pthread_t sender2;
	pthread_t sender3;
	pthread_t sender4;
	pthread_t daemon;
	pthread_t receiver1;
	pthread_t receiver2;
	pthread_t receiver3;
	
	sem_init(&mutex, 0, 1);
	
	struct squeue *bus_in_q = sq_create();
	struct squeue *bus_out_q1 = sq_create();
	struct squeue *bus_out_q2 = sq_create();
	struct squeue *bus_out_q3 = sq_create();
	

	//create arg struct
	for(i = 0; i < 8; i++)
	{
		//printf("Error, i = %d\n", i);
		switch(i)
		{
			case 0 ... 3: 
			//sender arguments
				args_s[i].period.tv_nsec = period_multiplier[i] * BASE_PERIOD * 1000;
				args_s[i].priority = thread_priority[i];
				args_s[i].bus_in = bus_in_q;
				break;
		
			case 4:
			//daemon arguments
				args_d.period.tv_nsec = period_multiplier[i] * BASE_PERIOD * 1000;
				args_d.priority = thread_priority[i];
				args_d.bus_in = bus_in_q;
				args_d.bus_out1 = bus_out_q1;
				args_d.bus_out2 = bus_out_q2;
				args_d.bus_out3 = bus_out_q3;
				break;
			//receiver arguments	
			case 5:
				args_r[i-5].period.tv_nsec = period_multiplier[i] * BASE_PERIOD * 1000;
				args_r[i-5].priority = thread_priority[i];
				args_r[i-5].bus_out = bus_out_q1;
				break;
			case 6:
				args_r[i-5].period.tv_nsec = period_multiplier[i] * BASE_PERIOD * 1000;
				args_r[i-5].priority = thread_priority[i];
				args_r[i-5].bus_out = bus_out_q2;
				break;
			case 7:
				args_r[i-5].period.tv_nsec = period_multiplier[i] * BASE_PERIOD * 1000;
				args_r[i-5].priority = thread_priority[i];
				args_r[i-5].bus_out = bus_out_q3;
				break;
			 //receiever arguments
			default:
				printf(" i error " );
				break;
		}
		
	}
	
	//create pthreads
	pthread_create(&sender1, NULL, &sender_thread, (void *)&args_s[0]);
	pthread_create(&sender2, NULL, &sender_thread, (void *)&args_s[1]);
	pthread_create(&sender3, NULL, &sender_thread, (void *)&args_s[2]);
	pthread_create(&sender4, NULL, &sender_thread, (void *)&args_s[3]);
	pthread_create(&daemon, NULL, &bus_daemon, (void *)&args_d);
	pthread_create(&receiver1, NULL, &receiver_thread, (void *)&args_r[0]);
	pthread_create(&receiver2, NULL, &receiver_thread, (void *)&args_r[1]);
	pthread_create(&receiver3, NULL, &receiver_thread, (void *)&args_r[2]);

	pthread_join(sender1, NULL);
	pthread_join(sender2, NULL);
	pthread_join(sender3, NULL);
	pthread_join(sender4, NULL);
	pthread_join(daemon, NULL);
	pthread_join(receiver1, NULL);
	pthread_join(receiver2, NULL);
	pthread_join(receiver3, NULL);
	return 0;
	sem_destroy(&mutex);
}
