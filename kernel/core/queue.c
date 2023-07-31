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
struct vmac_queue uqueue; /* upper layer frames queueu */
struct vmac_queue mqueue; /* v-mac frames queue (i.e. DACK/retransmission/etc...) */
struct vmac_queue rx_upper; 
struct vmac_queues_status questatus;


/**
  * @brief    Starts 2 queue threads
  * - queuethread: handles passing management frames from core to hardware and
  *   non-management frames to hardware in that order (i.e. handles 2 queues).
  * - rqueuethread: handles passing received management frames from
  *   low-drivers to V-MAC core for processing
  */ 
void queue_start(void)
{
    if (kthread_run(&queuethread, (void*)0, "tx1"))
    {
        printk(KERN_INFO"VMAC: tx thread created\n");
    }

    if (kthread_run(&rqueuethread, (void*)0, "rx"))
    {
        printk(KERN_INFO"VMAC: rx thread created\n");
    }
}

/**
 * @brief    Initializes queues, and locks appropriate for queues
 */
void queue_init(void)
{
    INIT_LIST_HEAD(&uqueue.list);
    INIT_LIST_HEAD(&mqueue.list);
    INIT_LIST_HEAD(&rx_upper.list);
    
    spin_lock_init(&questatus.flock);
    spin_lock_init(&questatus.mlock);
    spin_lock_init(&questatus.rlock);
}

/**
 * @brief    quues from from userspace netlink to upper tx queue for vmacupperq to handle frame after
 *
 * @param      skb2    The socket buffer
 * @param      enc    The encode
 * @param[in]  type    The type
 * @param      seq    The sequence
 * @param      rate    The rate (255==frame rate adaptation)
 * 
 * Pseudo Code
 * 
 * @code{.unparsed}
 *  call vmac_tx directly and casting types correctly
 * @endcode
 * 
 * @NOTE: Was here due to existing queuing mechanism used for constrained devices, not needed on Pi4 nor nvidia platforms.
 *  
 */
void enq_usrqueue(struct sk_buff *skb2, char *enc, u8 type, char *seq, char *rate)
{
    vmac_tx(skb2, (*(uint64_t*)(enc)), type, (*(uint8_t*) rate), (*(uint16_t*)(seq)));
}


/**
 * @brief    queues frames after vmac_tx to be queued before vmac_low_tx for
 * transmission -- frame is handled by queuethread thread after this function.
 *
 * @param      skb    The socket buffer
 * @param[in]  seq    The sequence
 * @param[in]  rate    The rate
 * 
 * Pseudo Code
 * 
 * @code{.unparsed}
 *  allocate struct for queue entry (physical memory i.e. kmalloc)
 *  insert skb into struct
 *  lock flock
 *  add struct to tail of uqueue list
 *  unlock flock
 * @endcode
 */
void enq_uqueue(struct sk_buff *skb, u16 seq, u8 rate)
{
    struct vmac_queue *entry = kmalloc(sizeof(struct vmac_queue), GFP_KERNEL);
    if (entry)
    {
        entry->frame = skb;
        entry->rate = rate;
        entry->seq = seq;
        spin_lock(&questatus.flock);
        list_add_tail(&(entry->list),&(uqueue.list)); 
        spin_unlock(&questatus.flock);
    }
}

/**
 * @brief    retransmission frame to be queued in management queue for higher
 * priority.
 *
 * @param      skb    The socket buffer (i.e. frame)
 * @param[in]  rate    The rate
 * 
 * Pseudo Code
 * 
 * @code{.unparsed}
 *  allocate struct for queue entry (physical memory i.e. kmalloc)
 *  insert skb into struct
 *  lock mlock
 *  add struct to tail of mqueue list
 *  unlock mlock
 * @endcode
 */
void retrx(struct sk_buff* skb, u8 rate)
{
    struct vmac_queue *entry = kmalloc(sizeof(struct vmac_queue), GFP_KERNEL);
    if (entry)
    {
        entry->frame = skb;
        entry->rate = rate;
        spin_lock(&questatus.mlock);
        list_add_tail(&(entry->list), &(mqueue.list)); 
        spin_unlock(&questatus.mlock);
    }
}

/**
 * @brief    adds DACK frame to management queue to be transmitted --frame gets handled by queueuthread thread after this function
 *
 * @param      skb    The socket buffer containing DACK
 * 
 * Pseudo Code
 * 
 * @code{.unparsed}
 *  allocate struct for queue entry (physical memory i.e. kmalloc)
 *  insert skb into struct
 *  set data rate for DACK to 2Mbps.
 *  lock mlock
 *  add struct to tail of mqueue list
 *  unlock mlock
 * @endcode
 */
void add_DACK(struct sk_buff *skb)
{
    struct vmac_queue *entry = (struct vmac_queue*) kmalloc(sizeof(struct vmac_queue), GFP_KERNEL);
    if (entry)
    {
        entry->frame = skb;
        entry->rate = 1;        
        spin_lock(&questatus.mlock);
        list_add_tail(&(entry->list), &(mqueue.list));
        spin_unlock(&questatus.mlock);    
    }
    
}

/**
 * @brief      checks management and data queue for transmission, management gets priority.
 *
 * @param      data  The data
 *
 * @return     0
 * 
 * Pseudo Code
 * 
 * @code{.unparsed}
 * while true
 *  if management frames queue counter is not 0
 *      iterate over management frames queue
 *          If there is no slot in medium to grab
 *              break
 *          End If
 *          store frame from management frames queue to tmp
 *          if tmp structure has rate value 255 (i.e. use default low data rate)
 *              assigned it value for 30Mbps
 *          else 
 *              use rate defined by structure from V-MAC core.
 *          End If
 *          call vmac_low_tx passing frame and data rate of frame
 *          lock management frames queue lock
 *          delete frame from queue
 *          unlock management frames queue lock
 *          free tmp memory
 *      End Iteration
 *  End If
 *  set index to 0
 *  If questatus upper layer frames counter is not 0
 *      Iterate over upper layer frames queue
 *          If there is no slot in medium to grab
 *              break
 *          End If
 *          store frame from management frames queue to tmp
 *          if tmp structure has rate value 255 (i.e. use default low data rate)
 *              assigned it value for 30 Mbps rate
 *          Else 
 *              use rate defined by structure from V-MAC core.
 *          End If
 *          call vmac_low_tx passing frame and data rate of frame
 *          lock upper layer frames queue lock
 *          delete frame from queue
 *          unlock upper layer frames queue lock
 *          free tmp
 *      End Iteration
 *  End If    
 * End While     
 * @endcode
 */
int queuethread(void *data)
{
    struct list_head *pos, *q;
    struct vmac_queue *tmp;
    struct ieee80211_local *local = NULL; 
    u8 ratetmp;
    msleep(500); /* This is necessary to wait for hardware and local to be initialized before checking DMA */
    while(1)
    {
        usleep_range(100, 400);
        if(local == NULL)
        {
            local = hw_to_local(getvhw());
        }
        pos = NULL;
        q = NULL;
        list_for_each_safe(pos, q, &mqueue.list)
        {
            if (local->ops->get_stats(&local->hw, NULL) == -1)
            {
                break;
            }    
            #ifdef DEBUG_MO
                printk(KERN_INFO "sending DACK/retrx \n", questatus.hrd_q);
            #endif
            tmp = list_entry(pos, struct vmac_queue, list);
            ratetmp = 13 + 0x40 + 0x80; /* 30Mbps for DACK by default and retrx for now */
            vmac_low_tx(tmp->frame, ratetmp);
            tmp->frame = NULL;
            spin_lock(&questatus.mlock);
            list_del_init(pos);
            spin_unlock(&questatus.mlock);
            kfree(tmp);
        }
        
        /* uqueue Iteration for frames coming from upper layer */
        list_for_each_safe(pos, q, &uqueue.list)
        {

            if(local->ops->get_stats(&local->hw, NULL) == -1)
            {
                break;
            }
            tmp = list_entry(pos, struct vmac_queue, list);
            if (tmp->rate == 255)
            {
                ratetmp = 13 + 0x40 + 0x80; /* 30 Mbps */
            }
            else 
            {
                ratetmp = tmp->rate;
            }
            vmac_low_tx(tmp->frame, ratetmp);                        
            spin_lock(&questatus.flock);
            list_del_init(pos);
            spin_unlock(&questatus.flock);
            tmp->frame = NULL;
            kfree(tmp);
        }
    }
}

/**
 * @brief    Adds a management frame to rx queue reception to be processed in
 * order (Note: managment frames are computing intensive)
 *
 * @param      skb    The socket buffer of the frame
 * 
 * Pseudo Code
 * 
 * @code{.unparsed}
 *  allocate struct for queue entry (physical memory i.e. kmalloc)
 *  insert skb into struct
 *  lock received management queue
 *  add struct to tail of list
 *  unlock received management queue
 * @endcode
 */
void add_mgmt(struct sk_buff* skb)
{
    struct vmac_queue* entry = (struct vmac_queue*)kmalloc(sizeof(struct vmac_queue), GFP_KERNEL);
    if (entry)
    {        
        entry->frame=skb;
        spin_lock(&questatus.rlock);
        list_add_tail(&(entry->list),&(rx_upper.list));
        spin_unlock(&questatus.rlock);
    }
}

/**
 * @brief    rqueue thread handles moving management frames from low-level
 * driver to V-MAC core. This is necessary as management frames consume
 * computing resources
 *
 * @param      data    The data
 *
 * @return    0
 *
 * Pseudo Code
 *
 * @code{.unparsed}
 *  while true
 *      sleep between 10-200 microseconds
 *      Iterate over received management frames queue
 *          store iterated entry in tmp
 *          call vmac_rx passing frame within tmp struct
 *          lock received management queue lock
 *          delete entry from received management queue lock
 *          unlock received management queue lock
 *          Free tmp
 *      End Iteration
 *  End While
 * @endcode
 */
int rqueuethread(void* data)
{
    struct list_head *pos, *q;
    struct vmac_queue* tmp;
    while(1)
    {
        usleep_range(10,200);
        pos = NULL;
        q = NULL;
        list_for_each_safe(pos, q, &rx_upper.list)
        {
            tmp=list_entry(pos,struct vmac_queue,list);
            vmac_rx(tmp->frame);
            spin_lock(&questatus.rlock);
            list_del_init(pos); 
            spin_unlock(&questatus.rlock);
            tmp->frame = NULL;
            kfree(tmp);                
        }
    }
}
