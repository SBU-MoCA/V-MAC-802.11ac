/*
 *      stress-test.c - User space code to run VMAC tests
 *      Mohammed Elbadry <mohammed.elbadry@stonybrook.edu>
 *      Tejas Menon  <tejas.menon@stonybrook.edu>
 *      
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include "vmac-usrsp.h"

/**
 * DOC: Introdcution
 * This document defines a code to implement VMAC sender or receiver
 */

/**
 * DOC : Using VMAC sender
 * This receiver is a standard C executable. Simply compile and run
 * Eg: gcc stress-test.c vmac.a -pthread -lm
 *
 *  ./a.out -p  --> p signfies this is the sender/producer
 */

/**
 * DOC : Using VMAC receiver
 * This receiver is a standard C executable. Simply compile and run
 * Eg: gcc stress-test.c vmac.a -pthread -lm
 *
 *  ./a.out -c  --> c signfies this is the receiver/consumer
 */

/**
 * DOC : Warning
 * In a standard test run, always run the sender BEFORE
 * you run the receiver  as the sender waits for an interest from the receiver.
 * Do not use both -p and -c arguments while running the code. Use either one
 */

/**
 *
 * DOC :Frame format
 * The sender sends 1024 byte Ethernet  frames with only sequence number inserted as data.
 *
 */

pthread_t thread;
pthread_t sendth;
pthread_t appth;
volatile int running2=0;
volatile int total;
volatile int consumer=0;
volatile int producer=0;
int times=0;
FILE *sptr,*cptr,*fptr;
double loss=0.0;
int c;
struct tm *loc;
unsigned int count =0;
long ms;
time_t s;
struct timespec spec;
int window[1500];
double intTime;
int newret = 0;

FILE *logFile;

double getRate(	uint8_t rix,uint8_t bw, uint8_t sgi, uint8_t stream);
/**
 * vmac_send_interest  - Sends interest packet
 *
 * Creates an interest frame and sends  it. Run as a C thread process
 *
 * Arguments : @tid : Thread id which is a automatically created when calling
 * pthread_create. Do NOT set this manually
 *
 * @param      tid   thread ID.
 *
 * @return     void
 */
void *vmac_send_interest(void* tid)
{
    //double time1,time2,timediff=0;
    int s,bw,r,sgi;
    int counter1;
	char rateStr[1500];
    int i;
    char* dataname="chat";
    uint16_t name_len=strlen(dataname);
    char buffer[1593]="bufferaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    total=0;
    struct vmac_frame frame;
    struct meta_data meta;
    int counter = 0;
    int state = 0;
    meta.bw = 1;
    meta.sgi = 1;
    meta.stream = 1;
    while(1)
    {
        total = 0;
        frame.buf = buffer;
        frame.len = 1500;
        frame.InterestName = dataname;
        frame.name_len = 4;
        meta.type = VMAC_FC_ANN;       
        for(s = 0; s<2 ; s++)// S = 0 is spatial stream 0, S =1 is spatial stream 1 
        {
          for(bw =0; bw<2;bw++) //BW= 0 is 20MHz BW, BW = 1 is 40MHz BW
          {
            for(sgi= 0; sgi<2;sgi++) // sgi = 1 mean short guard interval is enabled, sgi = 0 mean short guard interval disabled.
            {
              for(r =0; r<10;r++) // r is the rate idx passed to the phy
              {
                // Update the meta parameters
                meta.bw = bw;
                meta.sgi = sgi;
                meta.stream = s;
                meta.rate = r;                
	        double rate = getRate(meta.rate, meta.bw, meta.sgi, meta.stream);	
	        sprintf(rateStr, "%d ", total++);
		strcat(rateStr,buffer);		
		frame.buf = rateStr;
		//Perform back to back transmission for a data rate for 15 seconds 
		printf(" Transfer data : Stream = %d bw =%d rate idx = %d sgi = %d\n",meta.stream ,meta.bw,meta.rate,meta.sgi);
		send_vmac(&frame, &meta);
              }
            }
          }
        }
        printf("Sleep for 60s after transfer of data for all data rates\n");    
        sleep(60);     
    }
}

/**
 *  vmac_send_data - VMAC producer
 *
 *  Creates data frames and sends them to receiver(s). Run as a C thread process
 *
 *  Arguments :
 *  @tid : Thread id which is a automatically created when calling pthread_create. In this case
 *  not run as thread. Default value of 0 to be used
 */

void *vmac_send_data(void* tid)
{
    
    char* dataname="chat";
    char sel='a';
    char msgy[1024];
    int i = 0, j = 0;
    uint16_t name_len = strlen(dataname);
    uint16_t len = 1023;  
    struct vmac_frame frame;
    struct meta_data meta;
    running2 = 1;
    printf("Sleeping for 15 seconds\n");
    sleep(15);
    printf("Sending no.%d\n", times++);
    memset(msgy,sel,1023);
    msgy[1023] = '\0';
    meta.type = VMAC_FC_DATA;
    meta.rate = 60.0;
    frame.buf = msgy;
    frame.len = len;
    frame.InterestName = dataname;
    frame.name_len = 4;
    for(i = 0; i < 50000; i++)
    {
        meta.seq = i;

        send_vmac(&frame, &meta);
    }
    running2 = 0;
}

/**
 * recv_frame - VMAC recv frame function
 *
 * @param      frame  struct containing frame buffer, interestname (if available), and their lengths respectively.
 * @param      meta   The meta meta information about frame currently: type, seq, encoding, and rate,
 */
void callbacktest(struct vmac_frame *frame, struct meta_data *meta)
{
    uint8_t type = meta->type; 
    uint64_t enc = meta->enc;
    double goodput;
    char* buff = frame->buf;
    uint16_t len = frame->len;
    uint16_t seq = meta->seq;
    double frameSize = 0.008928; /* in megabits  1116 bytes after V-MAC and 802.11 headers*/
    uint16_t interestNameLen = frame->name_len;
    double timediff;
    double waittime = 15; /* 15 seconds waiting/sleep for all interests to come in */
    clock_gettime(CLOCK_REALTIME,&spec);
    s = spec.tv_sec;
    ms = round(spec.tv_nsec / 1.0e6);
    int ret;
	char *ptr;
	if (ms > 999) 
    {
        s++;
        ms = 0;
    }
    timediff = spec.tv_sec;
    timediff += spec.tv_nsec / 1.0e9;
    timediff = timediff - intTime;
    if (type == VMAC_FC_INT && producer == 1 && running2 == 0)
    {
    //    pthread_create(&sendth, NULL, vmac_send_data, (void*)0);
        printf("type:%u and seq=%d and count=%u @%"PRIdMAX".%03ld\n", type, seq, count, (intmax_t)s, ms);
    }
    else if (type == VMAC_FC_DATA && consumer)
    {
        total++;
        loss = ((double)(500 - total) / 500) * (double) 100;
        goodput = (double)(total * frameSize) / (timediff - waittime);
        printf("type:%u | seq=%d | loss=%f | goodput=%f |T= %f\n", type, seq, loss, goodput, timediff - waittime);
        printf("content= %s \n length =%d\n", frame->buf, frame->len);
    }
	else if (type == VMAC_FC_ANN && producer == 1 && running2 == 0)
    {
        printf("Announcement Frame Received! \n" );
		//printf("Content= %s \n length =%d\n", frame->buf, frame->len);
		newret = strtod(frame->buf, &ptr);
		if(ret > newret)// First Data rate
		{
			fprintf(logFile, "NEW TRANSMISSION\n");
		}
		newret = ret;
		fprintf(logFile, "%%d\n",ret);
    }
    free(frame);
    free(meta);
}

/**
 *  run_vmac  - Decides if sender or receiver.
 *
 *  Decides if VMAC sender of receiver
 *
 *  Arguments :
 *  @weare: 0 - Sender, 1 - Receiver
 *
 */
void run_vmac(int weare)
{

    uint8_t type;
    uint16_t len,name_len;
    uint8_t flags=0;
    pthread_t consumerth;
    char dataname[1600];
    char choice;

    choice = weare;
    if (choice == 0)
    {
        printf("We are producer\n");    
        producer = 1;
    }
    else if (choice == 1)
    {
        printf("We are consumer\n");
        running2 = 1;
        producer = 0;
        consumer = 1;
        pthread_create(&consumerth,NULL,vmac_send_interest,(void*)0);
    }
}

/**
 * main - Main function
 *
 * Function registers, calls run_vmac
 *
 * Arguments
 * p or c (Look at DOC Using VMAC sender or VMAC receiver
 */

int  main(int argc, char *argv[]){
    int weare=0;
    void (*ptr)()=&callbacktest;
    vmac_register(ptr);
    if(argc < 2 ) 
    { 
        return -1;
    }

    if (strncmp(argv[1], "p", sizeof(argv[1])) == 0) 
    {
		logFile = fopen("logFile", "w");
        weare = 0;
    } 
    else  
    {
        weare=1;
    } 

    run_vmac(weare);
    while (1) 
    { 
        sleep(1);
    }
    return 1 ;  
}

double getRate(	uint8_t rix,uint8_t bw, uint8_t sgi, uint8_t stream)
{			
	for( int i = 0; i< RATES_NUM; i++)
	{
		if (rates[i].rix == rix && rates[i].bw == bw && rates[i].sgi == sgi  && rates[i].stream == stream)
		{
			return rates[i].rate;
		 
		}
	}
}
