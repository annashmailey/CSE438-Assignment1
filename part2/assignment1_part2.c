#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

struct msg {
    int msg_id;      // global sequence number of message
    int source_id;  
    int dest_id;
    struct timespec start_time; 
    char msg_string[80];  // arbitrary character message with uniformly distributed length of 10 to 80  
} *msgp;

#define BASE_PERIOD 1000 //in ms
int msg_count = 0; //# of messages created
int msg_drop_count = 0;
int msg_received_count = 0; //# of messages received by receiver
int msg_forwarded_count = 0; //# of messages forwarded through deamon
int finished = 0; //used to end thread loops. Changed to 1, when the final bus_out is cleared. 
struct timespec accum_time[2000];// accum time for each finished thread, used for avg and stdev

int fd_in, fd_out1, fd_out2, fd_out3;

static sem_t mutex;

struct arg_struct_sender {
    	int priority;
	struct timespec period;
};

struct arg_struct_receiver {
    	int priority;
	struct timespec period;
	int q_num; //specifies which output q to write to 1-3
};

struct arg_struct_daemon {
    	int priority;
	struct timespec period;
};

void *sender_thread(void* arg)
{
	struct arg_struct_sender *args = (struct arg_struct_sender *)arg;
	struct timespec period = args->period;
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
				if(write(fd_in,(char*)add_msg, sizeof(struct msg))<0)
				{
					msg_drop_count++;
				}
				free(add_msg);
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
	int q_num = args->q_num;
	struct timespec next, et;
	int num_msg, dest_id, msg_length, sender_id, i, loop;
	struct msg rem_msg;
	unsigned long avg_time, stdev, ac_time;
	unsigned long var = 0;
	
	clock_gettime(CLOCK_MONOTONIC, &next);
	while(!finished)
	{	
		sem_wait(&mutex);
		loop = 0;
		while(loop >= 0)
		{
		if(q_num == 1)
			loop = read(fd_out1, (char*)&rem_msg, sizeof(struct msg));
		if(q_num == 2)
			loop = read(fd_out2, (char*)&rem_msg, sizeof(struct msg));
		if(q_num == 3)
			loop = read(fd_out3, (char*)&rem_msg, sizeof(struct msg));
		if(loop >= 0)
		{
			clock_gettime(CLOCK_MONOTONIC, &et);
			accum_time[rem_msg.msg_id].tv_nsec = et.tv_nsec - rem_msg.start_time.tv_nsec;
			msg_received_count++;
		}
		else
		{
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
	struct timespec next;
	struct msg fwd_msg;
	int dest_id;
	int loop;
	
	clock_gettime(CLOCK_MONOTONIC, &next);
	while(!finished)
	{
		sem_wait(&mutex);
		//Remove messages from bus_in_q, loop receives output from sq_read, meaning it is -1 when the queue is empty.
		loop = 0;
		while(loop >= 0)
		{
			loop = read(fd_in,(char*)&fwd_msg,sizeof(struct msg));
			//Forward messages to appropriate bus_out_q
			dest_id = fwd_msg.dest_id;
			switch (dest_id)
			{
				case 0:
					if(write(fd_out1,(char*)&fwd_msg,sizeof(struct msg)) < 0)
					{
						msg_drop_count++;
					}
					else
						msg_forwarded_count++;
					break;
				case 1:
					if(write(fd_out2,(char*)&fwd_msg,sizeof(struct msg)) < 0)
					{
						msg_drop_count++;
					}
					else
						msg_forwarded_count++;
					break;
				case 2:
					if(write(fd_out3,(char*)&fwd_msg,sizeof(struct msg)) < 0)
					{
						msg_drop_count++;
					}
					else
						msg_forwarded_count++;
					break;
				default:
					printf("could not find dest_id");
					break;
			}
		}
		next.tv_nsec = next.tv_nsec + period.tv_nsec;
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, 0);
		sem_post(&mutex);
	}
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

	//open queue devices
	fd_in = open("/dev/bus_in_q", O_WRONLY);
	fd_out1 = open("/dev/bus_out_q1", O_WRONLY);
	fd_out2 = open("/dev/bus_out_q2", O_WRONLY);
	fd_out3 = open("/dev/bus_out_q3", O_WRONLY);
	//printf("%d\n%d\n%d\n%d\n",fd_in,fd_out1,fd_out2,fd_out3);
	if( (fd_in<0) || (fd_out1<0) || (fd_out2<0) || (fd_out3<0))
	{
		printf("Could not open devices.\n");
		return -1;
	}

	//create arg struct
	for(i = 0; i < 8; i++)
	{
		switch(i)
		{
			case 0 ... 3: 
			//sender arguments
				args_s[i].period.tv_nsec = period_multiplier[i] * BASE_PERIOD * 1000;
				args_s[i].priority = thread_priority[i];
				break;
		
			case 4:
			//daemon arguments
				args_d.period.tv_nsec = period_multiplier[i] * BASE_PERIOD * 1000;
				args_d.priority = thread_priority[i];
				break;
			//receiver arguments	
			case 5:
				args_r[i-5].period.tv_nsec = period_multiplier[i] * BASE_PERIOD * 1000;
				args_r[i-5].priority = thread_priority[i];
				args_r[i-5].q_num = 1;
				break;
			case 6:
				args_r[i-5].period.tv_nsec = period_multiplier[i] * BASE_PERIOD * 1000;
				args_r[i-5].priority = thread_priority[i];
				args_r[i-5].q_num = 2;
				break;
			case 7:
				args_r[i-5].period.tv_nsec = period_multiplier[i] * BASE_PERIOD * 1000;
				args_r[i-5].priority = thread_priority[i];
				args_r[i-5].q_num = 3;
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
	sem_destroy(&mutex);
	close(fd_in);
	close(fd_out1);
	close(fd_out2);
	close(fd_out3);
	return 0;
}
