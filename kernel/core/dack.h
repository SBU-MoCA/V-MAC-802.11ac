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
#include <linux/version.h>

/* Defines ********************************************************************/
#define RX_WINDOW 450
#define HOLES_MAX 40
#define BUFFER_ROOM 120


/* DACK functions */

/**
 * @brief      Start DACK generation thread
 */
void dack_start(void);
int dackgen(void *data);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
void __sendDACK(struct timer_list *t);
#else
void __sendDACK(unsigned long data);
#endif
int dackdel(void *data);
void prepDACK(u64 enc,u16 round);
void dack_init(void);
void request_DACK(u64 enc, u16 round);
