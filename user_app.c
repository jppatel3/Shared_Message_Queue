#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h> 

/*
Devices
1 Sender_queue
1 Receiver_queue for each receiver(total 3 receiver queues)

Type of Users
3 Sender
1 Router
3 Receiver

Types of IDs
3 Receivers with Receiver_ID 1,2 and 3
3 Senders with Sender_ID 1,2 and 3
1 Message ID per message to be generated as Global Sequence Number
*/

/* Sender and Receiver Functions*/
void Sender(int *Sender_ID); //Sender_ID should be passed in the function
void Receiver(int *device);  //The Device which needs to be opened should be passed
void Router(void *ptr);	     //Router Function Receives Data, Extracts Receiver Id and Sends data to corresponding receiver
int Global_Message_ID_Gen(); //Global Sequence Generator Function

/*Global Variables*/
/*Devices*/
int Squeue_Send; 
int Squeue_Rec1;
int Squeue_Rec2;
int Squeue_Rec3;
/*Sender IDS*/
static int Sender1 = 1;
static int Sender2 = 2;
static int Sender3 = 3;
/*Receiver IDs*/
int receivers[] = {1,2,3};
int number_of_receivers = 3;
/*Message Id*/
int Golabal_Message_ID;
/*Semaphore*/
sem_t GM_ID;

/*Message Structure*/
struct User_Message{
	int Sender_Id;
	int Receiver_Id;
	int Message_Id;
	uint64_t  Time_Stamp;
	uint64_t  Queue_Time;
	char Message[80];	
};

/* Main Function*/
int main()
{
	/*Initialization of semaphore, Global_Message_ID and Random Generator*/
	sem_init(&GM_ID, 0, 1); 
	Golabal_Message_ID = 0;
	
	/*Opening All Devices*/
	Squeue_Send = open("/dev/squeue", O_RDWR);
	if (Squeue_Send < 0 ){
		printf("Can not open Send device file.\n");		
		return 0;
	}
	Squeue_Rec1 = open("/dev/squeue1", O_RDWR);
	if (Squeue_Rec1 < 0 ){
		printf("Can not open Receiver1 device file.\n");		
		return 0;
	}
	Squeue_Rec2 = open("/dev/squeue2", O_RDWR);
	if (Squeue_Rec1 < 0 ){
		printf("Can not open Receiver2 device file.\n");		
		return 0;
	}
	Squeue_Rec3 = open("/dev/squeue3", O_RDWR);
	if (Squeue_Rec1 < 0 ){
		printf("Can not open Receiver3 device file.\n");		
		return 0;
	}
	
	/*Creating Pthreads*/
	pthread_t thread1, thread2,thread3, thread4, thread5, thread6, thread7;  
	pthread_create (&thread7, NULL,(void *) &Router,NULL);
	pthread_create (&thread4, NULL,(void *) &Receiver,&Squeue_Rec1);
	pthread_create (&thread5, NULL,(void *) &Receiver,&Squeue_Rec2);
	pthread_create (&thread6, NULL,(void *) &Receiver,&Squeue_Rec3);
	pthread_create (&thread1, NULL,(void *) &Sender, &Sender1);
	pthread_create (&thread2, NULL,(void *) &Sender, &Sender2);
	pthread_create (&thread3, NULL,(void *) &Sender, &Sender3);

	/* Sleeping for 10s */
	sleep(10);
	
	/*killing Sender Threads*/
	pthread_cancel(thread1);
	printf("Sender1 thread killed\n");
	pthread_cancel(thread2);
	printf("Sender2 thread killed\n");
	pthread_cancel(thread3);
	printf("Sender3 thread killed\n");
	
	/*killing Router threads*/
	sleep(1);
	pthread_cancel(thread7);
	printf("Router thread killed\n");
	sleep(1);

	/*killing router thread*/
	pthread_cancel(thread4);
	printf("Receiver1 thread killed\n");
	pthread_cancel(thread5);
	printf("Receiver2 thread killed\n");
	pthread_cancel(thread6);
	printf("Receiver3 thread killed\n");

	printf("Total messages sent: %d\n",Golabal_Message_ID);
	
	return 0;
}

/*Message Sender function*/
void Sender(int *Sender_ID)
{
	char *p;		//Character Pointer to pass in RD/WR File Function
	struct User_Message *s1;//Pointer to User Message Structure 
	int res;

	while(1)
	{
		s1 = malloc(sizeof *s1);
		p = (char *)s1; 
		s1->Sender_Id = *Sender_ID;
		s1->Receiver_Id = receivers[rand() % number_of_receivers];
		s1->Queue_Time = 0;
		res = -1;
	/*Generating Message ID and Locking the when Modifying it*/
		sem_wait(&GM_ID);
		s1->Message_Id = Global_Message_ID_Gen();
		sem_post(&GM_ID);

		while(res<0){
			res = write(Squeue_Send,p, 1);
			usleep(((rand() % 10) + 1)*1000); // Sleeping randomnly between 1-10ms
		}

	/*Checking if message successfully sent or not*/
		printf("Sender%d  : Receiver:%d Message:%d  \n",s1->Sender_Id,s1->Receiver_Id,s1->Message_Id);
		usleep(((rand() % 10) + 1)*1000); // Sleeping randomnly between 1-10ms
	/*Releasing allocated message memory*/
		free(s1);
	}
	pthread_exit(0);
}

//Message Receiver function 
void Receiver(int *device)
{
	char *p;		//Character Pointer to pass in RD/WR File Function
	int fd = *device; 	//Incoming file descriptor is stored in fd
	struct User_Message *s2;//Pointer to User Message Structure 
	int res;
	while(1){
		s2 = malloc(sizeof *s2);
		p = (char *)s2; 
		res = read(fd,p,1);
		if(res<0){
			//printf("Receiver %d Cannot Read File\n",fd);
		}
		else{
	/*Message is received*/
			printf("Receiver%d: Sender:%d Message:%d Queue_Time:%ld \n", s2->Receiver_Id, s2->Sender_Id, s2->Message_Id,(long)s2->Queue_Time);
		}
		usleep(((rand() % 10) + 1)*1000); // Sleeping randomnly between 1-10ms
	/*Releasing allocated message memory*/
		free(s2);
	}
	pthread_exit(0);
}

//Router Receives->Extracts Receiver's ID->Sends to appropriate queue
void Router(void *ptr)
{
	char *p;		//Character Pointer to pass in RD/WR File Function
	struct User_Message *s; //Pointer to User Message Structure 
	int res = -1;
	
	while(1)
	{
		res =  -1;
	/*Receiveing Message*/
		s = malloc(sizeof *s);
		p = (char *)s; 
		while(res<0)
		{
			res = read(Squeue_Send,p,1);
			// Checking if message Received
			if(res<0){
				usleep(((rand() % 10) + 1)*1000); // Sleeping randomnly between 1-10ms
			}	
		}
		res = -1;
	/*Extracting Receiver's ID*/
		switch(s->Receiver_Id){
			case 1: // Sending to Rec1
			while(res<0){
				res = write(Squeue_Rec1,p, 1);
				usleep(((rand() % 10) + 1)*1000); // Sleeping randomnly between 1-10ms
			}	
			printf("Router	 : Receiver:%d Sender:%d Message:%d Queue_Time: %ld\n",s->Receiver_Id, s->Sender_Id,s->Message_Id,(long)s->Queue_Time);
			break;

			case 2:// Sending to Rec2
			while(res<0){
				res = write(Squeue_Rec2,p, 1);
				usleep(((rand() % 10) + 1)*1000); // Sleeping randomnly between 1-10ms
			}
			printf("Router	 : Receiver:%d Sender:%d Message:%d Queue_Time: %ld\n",s->Receiver_Id, s->Sender_Id,s->Message_Id,(long)s->Queue_Time);
			break;

			case 3:// Sending to Rec3
			while(res<0){
				res = write(Squeue_Rec3,p, 1);
				usleep(((rand() % 10) + 1)*1000); // Sleeping randomnly between 1-10ms
			}			
			printf("Router	 : Receiver:%d Sender:%d Message:%d Queue_Time: %ld\n",s->Receiver_Id, s->Sender_Id,s->Message_Id,(long)s->Queue_Time);
			break;

			default:
			break;
		}
	/*Releasing allocated message memory*/
		free(s);
	}
	pthread_exit(0);		
}

/*Global Sequence Message Id Generation*/
int Global_Message_ID_Gen()
{	
	Golabal_Message_ID = Golabal_Message_ID + 1;
	return Golabal_Message_ID;
}

