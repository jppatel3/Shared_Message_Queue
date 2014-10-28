Files:
1) Driver.c
Driver.c file is the kernal module. On inserting the kernal module it creates four devices named Squeue, Squeue1, Squeue2 and Squeue3 in dev folder. Each device acts as a FIFO with size of 10. It will not accept data if queue is full and return data if data in queue is empty. 
2) user_app.c
user_app.c is the user level application. The application runs 7 different threads working on four devices created by driver module. Three sender threads sends data to one queue(Squeue) and every receiver has its on Queue(Squeue1, Squeue2, Squeue3). The Router takes data from Squeue and routes the data to appropriate receiver. 

Output:
When you will give make command in console you will see data printed on terminal by receiver and Sender. Printed Data is in following format

Receiver will print following message
Receiver : Sender: MessageID: Queue_Time

Sender will print following message
Sender	 :Receiver: Message_ID:

Router will print following message
Router : Receiver: Sender: Message_ID Queue_time:  

Queue time is the number of cycles for which that particular message was in queue.
At the end you will see total messages sent. Actually last updated Global sequence number is printed as total message number. So sometimes it will not exactly same as the ID of Last message received.

