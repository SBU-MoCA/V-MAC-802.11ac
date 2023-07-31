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
/* cleanup */
#define DEBUG_MO

/**
 * @brief      Clean up encoding from encoding table (occurs per encoding timeout)
 *
 * @param[in]  data  The data: pointer to encoding cleanup struct
 * 
 * Pseudo Code
 * 
 * @code{.unparsed}
 * check type of struct (i.e. tx or rx)
 * If type of cleaning is for receiving struct
 *  search for encoding at rx table (sanity check)
 *  If not found
 *      return
 *  End If
 *  remove from hastable
 *  free DACK struct if any left in queue not sent
 *  free rx_struct
 * else (i.e. type must be TX_ENC)
 *  search for encoding at tx table (sanity check)
 *  if not found
 *      return
 *  End If
 *  for i = 0 to either end of retransmission buffer size or latest sequence number transmitted (whichever smaller)
 *      free retransmission buffer frame
 *  End for
 *  remove from hastable
 *  free rx_struct
 *  @endcode
 */

void process (struct enc_cleanup* clean)
{
    struct encoding_rx* vmacr;
    struct encoding_tx* vmact;
    int i;
    if(clean->type == CLEAN_ENC_RX)
    {
        #ifdef DEBUG_MO
            printk(KERN_INFO "CLEAN: starting process \n");
        #endif
        vmacr= find_rx(RX_TABLE, clean->enc);
        if (!vmacr || vmacr == NULL)
        {
            return;
        }
        
        printk(KERN_INFO "CLEAN: removing element\n");        
     /* if(vmacr->dac_info.dack)
        kfree_skb(vmacr->dac_info.dack); mo */
        hash_del(&vmacr->node);
        vfree(vmacr);
    }
    else /* must be CLEAN_ENC_TX*/
    {
        vmact = find_tx(TX_TABLE, clean->enc);
        {

        }
        if (!vmact || vmact == NULL)
        {
            return;
        }
        #ifdef DEBUG_MO
            printk(KERN_INFO "VMAC_CLEAN: tx emptying buffer\n");
        #endif
        for(i = 0; i < (vmact->seq < WINDOW_TX ? vmact->seq : WINDOW_TX); i++)
        {
            if(vmact->retransmission_buffer[i])
               kfree_skb(vmact->retransmission_buffer[i]);
        }
        hash_del(&vmact->node);
        vfree(vmact);
    }
}
/**
 * @brief     clean up for receiving struct called by timer
 *
 * @param      t     time struct passing pointer to receving struct
 * 
 * Pseudo Code
 * @code{.unparsed}
 * Set type of struct to receiving struct
 * Call process function to handle emptying memory within.
 * @endcode
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
void __cleanup_rx(struct timer_list *t)
{
    struct encoding_rx* vmacr = from_timer(vmacr, t, enc_timeout);
    if (vmacr)
    {
        #ifdef DEBUG_VMAC
            printk(KERN_INFO"VMAC_CLEAN: rx cleaning started \n");
        #endif
        vmacr->clean.type = CLEAN_ENC_RX;
        process(&vmacr->clean);
    }
    
}
/**
 * @brief      clean up for transmission struct called by timer
 *
 * @param      t     time struct passing pointer to receving struct
 * 
 * Pseudo Code
 * 
 * @code{.unparsed}
 * Set type of struct to transmission struct
 * Call process function to handle emptying memory within.
 * @endcode
 */
void __cleanup_tx(struct timer_list *t)
{    
    struct encoding_tx* vmact = from_timer(vmact, t, enc_timeout);
    if (vmact)
    {
        vmact->clean.type = CLEAN_ENC_TX;
        process(&vmact->clean);            
    }
}
/**
 * @brief      cleanup struct for old kernel 4.18.0, same as cleanup_rx and tx.
 *
 * @param[in]  data  pointer to clean struct in encoding
 */
#else
void __cleanup(unsigned long data)
{
    struct enc_cleanup* clean=(struct enc_cleanup*)data;
    process(clean);
}
#endif
