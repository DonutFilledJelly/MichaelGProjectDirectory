#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#define NUM_THREADS 8

typedef struct Task{
	int a, b;
} Task;

Task taskQueue[256];//size of the queue.
int taskCount = 0;
pthread_mutex_t mutexQueue;
pthread_cond_t condQueue;
int pausecheck = 0;
int stopallth = 1;

void executeTask(Task* task){//the task being done, which is currently just mutiplying
	int result = task->a * task->b;
	printf("%d x %d = %d\n", task->a, task->b, result);
}

void submitTask(Task task){
	pthread_mutex_lock(&mutexQueue);
	taskQueue[taskCount] = task;//keeping track of new tasks
	taskCount++;
	pthread_mutex_unlock(&mutexQueue);
}



void* start_thread(void* args){//each thread
	while(stopallth == 1){
		Task task;
		pthread_mutex_lock(&mutexQueue);//locks to check current item in queue
		while(taskCount == 0){//makes sure that there is a task left
			//pthread_cond_wait(&condQueue, &mutexQueue);
			if(stopallth == 0){
				printf("Path Closed\n");
				pthread_mutex_unlock(&mutexQueue);
				return 0;
			}
		}
		
			task = taskQueue[0];
			int i; 
			//mutex lock start
			//mess with queue
			for(i = 0; i < taskCount - 1; i++){
				taskQueue[i] = taskQueue[i+1];
			}
			taskCount--;

			pthread_mutex_unlock(&mutexQueue);
			executeTask(&task);//executes task
		
	}
	printf("Path closed\n");

}

int main(){
	pthread_t threads[NUM_THREADS];
	pthread_mutex_init(&mutexQueue, NULL);
	pthread_cond_init(&condQueue, NULL);
	srand(time(NULL));
	for(int i=0; i < 100; i++){//creating random tasks to do. Set to 100 and picks randomly from 100
		Task t = {
			.a = rand() % 100,
			.b = rand() % 100
		};
		submitTask(t);
	}
	for(int i = 0; i < NUM_THREADS; i++){//creates all the threads
		printf("IN MAIN: Creating thread %d.\n");
		if(pthread_create(&threads[i], NULL, start_thread, NULL) != 0){
			perror("Failed to make thread");
		}
	}
	int stopint;
	stopint = 0;
	while(stopint == 0){//while loop keeps going until stop command is given
		char usersinput[] = "";
		scanf("%s", &usersinput);
		printf("Yo\n");
		if(strcmp (usersinput, "STOP") == 0){//if given stop, stop while loop
			stopint = 1;
			printf("Stop been called\n");
		}
		if(strcmp (usersinput, "PAUSE") == 0){//if given pause, cause a mutex lock
			pthread_mutex_lock(&mutexQueue);
			while(strcmp (usersinput, "RESUME") != 0){//only go out of mutex lock if resume is given
				scanf("%s", usersinput);
			}
			pthread_mutex_unlock(&mutexQueue);

		}
		printf("%s", usersinput);
		printf("test\n");
	}
	printf("Before ending\n");
	stopallth = 0;//gives the command to stop threads
	for(int i = 0; i < NUM_THREADS; i++){//waits for threads to finish current task
		if(pthread_join(threads[i], NULL)!=0){
			perror("Failed to join thread");
		}
	}
	printf("Ending\n");
	pthread_mutex_destroy(&mutexQueue);
	pthread_cond_destroy(&condQueue);//frees memory
	return 0;

}

