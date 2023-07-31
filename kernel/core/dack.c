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
#include "vmac.h"
#include <linux/rhashtable.h>
struct vmac_queue dqueue;
struct vmac_queue dackfree;
spinlock_t dackfreelok;
u16 dackreqnum;


/**
 * @brief      Adds a DACK creation request for appropriate encoding and round number
 *
 * @param[in]  enc    encoding of interest name
 * @param[in]  round  round i.e. 5 * latest sequence of data frame received
 */
void request_DACK(u64 enc, u16 round)
{
    struct vmac_queue* item;
    //prepDACK(enc, round);
    prepDACK(enc, round);
    /*item = kmalloc(sizeof(struct vmac_queue), GFP_KERNEL);
    if (item)
    {
        item->enc = enc;
        item->seq = round;       
        spin_lock(&dackfreelok);
        dackreqnum++;
        list_add_tail( &(item->list), &(dqueue.list));
        spin_unlock(&dackfreelok);
    } */
}

/**-------------------------------------------------------------------------*//**
 * V-MAC send DACK timer function
 *
 * @param      data  = pointer to struct dack_info
 * @return     N/A this is a timer function Pseudo Code
 * @code{.unparsed}
 * read dack_info structure from data pointer passed from pointer.
 * if structure does exists i.e. not NULL
 *  Allocate memory for queue entry and init free signal to 1 i.e. kmalloc
 *  if DACK spinlock exist within structure
 *   Try Locking DACK spinlock
 *    if send signal in structure is 1
 *     set send signal to 0
 *     if DACK frame exists within structure (sanity check)
 *      prepare entry queue with proper information (data rate=1 i.e. 2mbps by default)
 *      lock management queue
 *      insert DACK entry structure at end of linked list queue
 *      unlock management queue
 *    unlock DACK spinlock
 *   else
 *    set free signal to 0
 *    delete allocated memory for queue entry (hence is it not used) i.e. kfree
 *    Increment send DACK function timer by 2 jiffies (this indicates that DACK-
 *    -entry is being updated or reviewed due to another DACK reception or overlapping rounds)
 *    return
 *  if free signal is set
 *   delete allocated memory for queue entry (hence is it not used) i.e. kfree
 * @endcode 
 * @note: There is some redundancy, to be Fixed before release.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
void __sendDACK(struct timer_list *t)
{
    struct encoding_rx* vmacr = from_timer(vmacr, t, dack_timer);
    struct dack_info* dac_info = &vmacr->dac_info;
#else
void __sendDACK(unsigned long data)
{
    struct dack_info* dac_info = (struct dack_info*) data;
#endif
    struct sk_buff* ptr;
    if(dac_info)
    {
        if(!spin_trylock(dac_info->dacklok))
        {
            mod_timer(dac_info->dack_timer, jiffies + msecs_to_jiffies(100));
            return;
        }
        if (dac_info->send == 1)
        {
            dac_info->send = 0;
            ptr = dac_info->dack;
            dac_info->dack = NULL;
            spin_unlock(dac_info->dacklok);            
            printk(KERN_INFO "MO: SENDING DACK\n");            
            //add_DACK(ptr);
            vmac_send_hack(ptr);
        }
        else 
            spin_unlock(dac_info->dacklok);
        }
}


/**
 * @brief      Prepare DACK frame
 *
 * @param[in]  enc    encoding value
 * @param[in]  round  The round header field value (i.e. burst number)
 *
 * @code{.unparsed}
 * init variables
 * set DACK header encoding header to encoding parameter passed to function
 * set type of frame to VMAC_HDR_DACK
 * ------------------Calculating holes values i.e. right edge and left edges----------------
 * for i either 0 (i.e. sequence number 0) or latest sequence received - RX_WINDOW
 *  if ith frame is not received
 *   tmp=i
 *   while (tmp-1)th frame is lost, tmp >= 0, latest frame received -RX_WINDOW <tmp-1 (i.e. prevent wrap around)
 *    decrement tmp
 *    increment loss
 *   set left edge to tmp
 *   set holes struct left edge value to tmp
 *   while ith element is lost and i is less than latest frame received - 1
 *    increment i
 *    increment loss
 *   set right edge to i
 *   set holes struct right edge vlaue to i
 *   increment holes_num variable by 1
 *  allocate memory for headers and holes for this DACK
 *  push headers into memory allocated within buffer
 *  add it to queue or swap old DACK with new DACK
 * -----------------------------------------------------------------------------------------
 * @encode
 */
void prepDACK(u64 enc, u16 round)
{
    struct encoding_rx *vmac;
    struct vmac_DACK ddr;
    struct vmac_hdr vmachdr;
    struct dack_info *dac_info;
    struct sk_buff *skb, *ptr = NULL;
    struct vmac_hole holesy[HOLES_MAX]; /* needs improvement */ 
    long duration, rndm = 0;
    int tmp, skbsize;    
    u8 loss = 0;
    u16 holes = 0, i = 0, le, re, lattmp;
    vmac = find_rx(RX_TABLE,enc); 
    dac_info = &vmac->dac_info;
    vmachdr.enc = enc;
    vmachdr.type = VMAC_HDR_DACK;
    get_random_bytes(&rndm, sizeof(rndm));
    rndm = rndm % 2;
    lattmp = round * 5;
    #ifdef DEBUG_MO
        printk(KERN_INFO "Encoding of DACK = %lld", enc);
    #endif
    /*-Calculating holes values i.e. right edge and left edges*/
    for(i = (lattmp < WINDOW ? 0 : lattmp - WINDOW) ; ( i < lattmp && holes < HOLES_MAX) ; i++)
    {
        if (vmac->window[i % WINDOW] == 0)
        {
            tmp = i;
            while(vmac->window[tmp % WINDOW - 1] == 0 && tmp > 0 && (lattmp < WINDOW ? 0: lattmp - WINDOW) < tmp - 1)
            {
                tmp--;
                loss++;
            }
            le = tmp;
            holesy[holes].le = tmp;
            while(vmac->window[i % WINDOW] == 0 && i < lattmp - 1)
            {
                i++;
                loss++;
            }
            re = i;
            holesy[holes].re = re;
            holes++;
        }
       
    }

    /* allocate skb struct and start placing headers */
    skbsize = sizeof(struct ieee80211_hdr) + sizeof(struct vmac_hdr) + sizeof(struct vmac_DACK) + holes * sizeof(struct vmac_hole) + BUFFER_ROOM;    
    skb = dev_alloc_skb(skbsize);
    if (!skb)
    {
        return;
    }
    #ifdef DEBUG_MO
        printk(KERN_INFO"VMACDACK: Holes= %u, sizeof skb= %d\n",holes, skbsize);
    #endif
    ddr.holes = holes;
    ddr.round = round;
    
    memcpy(skb_put(skb, sizeof(struct vmac_DACK)), &ddr, sizeof(struct vmac_DACK));
    for( i = 0; i < holes; i++)
    {
        #ifdef DEBUG_MO
            printk(KERN_INFO"VMACDACK: LE= %d, RE= %d\n",holesy[i].le, holesy[i].re);
        #endif
        memcpy(skb_put(skb, sizeof(struct vmac_hole)), &holesy[i], sizeof(struct vmac_hole));
    }
    memcpy(skb_push(skb, sizeof(struct vmac_hdr)), &vmachdr, sizeof(struct vmac_hdr));
    

    if (loss <= 5)
    {
    	/* this should always occur*/
    	loss = 5 - loss;
    } 

    if (vmac->alpha>=100){
    /* Safety in case clock loops back */
    vmac->alpha=100;
	}

    duration = 2 * vmac->alpha * (loss + rndm); //should be 5-loss but who keeps counting (ehh got many other bugs to fix first.lmao it's a festival of bugs wohooooo BUGSSSSSSSSSS
    if (duration == 0){
    	duration = 1;
    }
   
    while(!spin_trylock (&vmac->dacklok))
    {
        kfree_skb(skb);
        return;
    }

    if (dac_info->send == 1)
    {
        if (vmac->dac_info.dack)//mo-new:  vmac->dac_info.dack != NULL
        {
            rndm = 1;
            ptr = vmac->dac_info.dack;
        }

        dac_info->send = 1;
        dac_info->dacksheard = 0;
        dac_info->round = ddr.round;
        vmac->DACK = skb;
        vmac->dac_info.dack = skb;
    }
    else 
    {
        dac_info->dack = skb;
        dac_info->send = 1;
        dac_info->dacksheard = 0;
        dac_info->round = ddr.round;
        vmac->DACK = skb;
        
        if (duration > 100)
         {
            duration = 10;//reset to prevent kernel crash
    	}

       	mod_timer(&vmac->dack_timer, jiffies + duration);
    }
    spin_unlock(&vmac->dacklok);
    if (rndm == 1)
    {
        kfree_skb(ptr);
    }
}
