/*
* Copyright (c) 2017 - 2020, Mohammed Elbadry
*
*
* This file is part of V-MAC (Pub/Sub data-centric Multicast MAC layer)
*
* V-MAC is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 
* 4.0 International License.
* 
* You should have received a copy of the license along with this
* work. If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
* 
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <syslog.h>
#include <pthread.h>
#include <setjmp.h>
#include <sched.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include "uthash.h"

/** Defines **/

#define RATES_NUM 		80

#define SGI			    0x40
#define HT40       		0x80
/* frame types */
#define VMAC_FC_INT 	0x00    /* Interest frame     */
#define VMAC_FC_DATA 	0x01	/* Data frame 		  */
#define VMAC_FC_ANN		0x03	/* Announcement frame */
#define VMAC_FC_INJ		0x05	/* Injected frame 	  */

/* netlink parameters */
#define VMAC_USER 		29	 /* netlink ID to communicate with V-MAC Kernel Module */
#define MAX_PAYLOAD  	0x7D0    /* 2KB max payload per-frame */


/** Structs **/
const static struct {
	double  rate; /* Mbps */
	uint8_t rix; /* Index internally to pass to PHY */
	uint8_t bw;
	uint8_t sgi;
	uint8_t stream;
} rates[RATES_NUM] =
{
	//Spacial stream = 0, BW = 20MHz
	{6.5 , 0 , 0 , 0 , 0 },
	{7.2 , 0 , 0 , 1 , 0 },
	{13  , 1 , 0 , 0 , 0 },
	{14.4, 1 , 0 , 1 , 0 },
	{19.5, 2 , 0 , 0 , 0 },
	{21.7, 2 , 0 , 1 , 0 },
	{26  , 3 , 0 , 0 , 0 },
	{28.9, 3 , 0 , 1 , 0 },
	{39  , 4 , 0 , 0 , 0 },
	{43.3, 4 , 0 , 1 , 0 },
	{52  , 5 , 0 , 0 , 0 },
	{57.8, 5 , 0 , 1 , 0 },
	{58.5, 6 , 0 , 0 , 0 },
	{65  , 6 , 0 , 1 , 0 },
	{65  , 7 , 0 , 0 , 0 },
	{72.2, 7 , 0 , 1 , 0 },
	{78  , 8 , 0 , 0 , 0 },
	{86.7, 8 , 0 , 1 , 0 },
	{1000, 9 , 0 , 0 , 0 },//Need to update the correct data rate
	{1001, 9 , 0 , 1 , 0 },//Need to update the correct data rate
	
	//Spacial stream = 0, BW = 40MHz
	{13.5 , 0 , 1 , 0 , 0 },
	{15   , 0 , 1 , 1 , 0 },
	{27   , 1 , 1 , 0 , 0 },
	{30   , 1 , 1 , 1 , 0 },
	{40.5 , 2 , 1 , 0 , 0 },
	{45   , 2 , 1 , 1 , 0 },
	{54   , 3 , 1 , 0 , 0 },
	{60   , 3 , 1 , 1 , 0 },
	{81   , 4 , 1 , 0 , 0 },
	{90   , 4 , 1 , 1 , 0 },
	{108  , 5 , 1 , 0 , 0 },
	{120  , 5 , 1 , 1 , 0 },
	{121.5, 6 , 1 , 0 , 0 },
	{135  , 6 , 1 , 1 , 0 },
	{135  , 7 , 1 , 0 , 0 },
	{150  , 7 , 1 , 1 , 0 },
	{162  , 8 , 1 , 0 , 0 },
	{180  , 8 , 1 , 1 , 0 },
	{180  , 9 , 1 , 0 , 0 },
	{200  , 9 , 1 , 1 , 0 },
	
	//Spacial stream = 1, BW = 20MHz
	{13   , 0 , 0 , 0 , 1 },
	{14.4 , 0 , 0 , 1 , 1 },
	{26   , 1 , 0 , 0 , 1 },
	{28.9 , 1 , 0 , 1 , 1 },
	{39   , 2 , 0 , 0 , 1 },
	{43.3 , 2 , 0 , 1 , 1 },
	{52   , 3 , 0 , 0 , 1 },
	{57.8 , 3 , 0 , 1 , 1 },
	{78   , 4 , 0 , 0 , 1 },
	{86.7 , 4 , 0 , 1 , 1 },
	{104  , 5 , 0 , 0 , 1 },
	{115.6, 5 , 0 , 1 , 1 },
	{117  , 6 , 0 , 0 , 1 },
	{130.3, 6 , 0 , 1 , 1 },
	{130  , 7 , 0 , 0 , 1 },
	{144.4, 7 , 0 , 1 , 1 },
	{156  , 8 , 0 , 0 , 1 },
	{173.3, 8 , 0 , 1 , 1 },
	{2000 , 9 , 0 , 0 , 1 },//Need to update the correct data rate
	{2001 , 9 , 0 , 1 , 1 },//Need to update the correct data rate

	//Spacial stream = 1, BW = 40MHz
	{27   , 0 , 1 , 0 , 1 },
	{30   , 0 , 1 , 1 , 1 },
	{54   , 1 , 1 , 0 , 1 },
	{60   , 1 , 1 , 1 , 1 },
	{81   , 2 , 1 , 0 , 1 },
	{90   , 2 , 1 , 1 , 1 },
	{108  , 3 , 1 , 0 , 1 },
	{120  , 3 , 1 , 1 , 1 },
	{162  , 4 , 1 , 0 , 1 },
	{180  , 4 , 1 , 1 , 1 },
	{216  , 5 , 1 , 0 , 1 },
	{240  , 5 , 1 , 1 , 1 },
	{243  , 6 , 1 , 0 , 1 },
	{270  , 6 , 1 , 1 , 1 },
	{270  , 7 , 1 , 0 , 1 },
	{300  , 7 , 1 , 1 , 1 },
	{324  , 8 , 1 , 0 , 1 },
	{360  , 8 , 1 , 1 , 1 },
	{360  , 9 , 1 , 0 , 1 },
	{400  , 9 , 1 , 1 , 1 },
};


/**
 * @brief      vmac frame information, buffer and interest name with their lengths, respectively.
 */
struct vmac_frame
{
	char* buf;
	uint16_t len;
	char* InterestName;
	uint16_t name_len;
};


/**
 * @brief      meta data to provide guidance to V-MAC configurations
 */
struct meta_data
{
	uint8_t type;
	uint16_t seq;
	uint8_t rate;
	uint8_t bw;
	uint8_t sgi;
	uint8_t stream;
	uint64_t enc;
};

/**
 ** ABI Be careful when changing to adjust kernel/userspace information as well.
**/
struct control{
    char type[1];
    char enc[8];
    char seq[2];
    char rate;
    char bw;
    char sgi;
    char stream;
};

/* Struct to hash interest name to 64-bit encoding */
struct hash{
	uint64_t id;
	char *name;
	UT_hash_handle hh;
};


/**
 * @brief      V-MAC userspace's library internal use configurations.
 */
struct vmac_lib_priv
{
	struct hash* names;
	struct sockaddr_nl src_addr,dest_addr;
	/* TX structs */
	struct nlmsghdr *nlh;
	struct iovec iov;
	struct msghdr msg;

	/* RX structs */
	struct nlmsghdr *nlh2;
	struct iovec iov2;
	struct msghdr msg2;
	
	uint64_t digest64; 
	uint8_t fixed_rate;
	void (*cb)();
	char msgy[2000]; /* buffer to store frame */
	int sock_fd;
	pthread_t thread;
	char key[16];	
};

struct vmac_lib_priv vmac_priv;

/* Prototype functions */
uint64_t siphash24(const char *in, unsigned long inlen, const char k[16]);
int send_vmac(struct vmac_frame *frame, struct meta_data *meta);
void add_name(char*InterestName, uint16_t name_len);
void del_name(char *InterestName, uint16_t name_len);
int vmac_register(void (*cf));
