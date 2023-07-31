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
//#define DEBUG_VMAC
/**
 * @brief    netlink send frame from kernel to userspace
 *
 * @param      skb    The skb
 * @param[in]  enc    The encoding of frame
 * @param[in]  type    The type (i.e. data/interest/announcment/injected frame)
 * @param[in]  seq    The sequence of frame (if it has any)
 * 
 * 
 * @code{.unparsed}
 *  if there is a userspace process running to receive frame
 *      create memory based on size of frame and extra for control signals
 *      if memory was not created sucessfully
 *          return
 *      end If
 *      copy frame to new memory
 *      pass control signals (encoding, sequence, and type)
 *      set message netlink format to unicast and send
 *      if returned output < 0
 *          print logging message
 *      End If
 *  End If
 *  free memory of kernel from frame    
 * @endcode
 */
static void nl_send(struct sk_buff* skb, u64 enc, u8 type, u16 seq)
{
    struct nlmsghdr *nlh;
    struct sk_buff* skb_out;
    int res;
    struct control txc;
    struct sock * nl_sk = getsock();
    uint64_t ence = (uint64_t)enc;
    uint16_t typee = (uint8_t) type;
    int pidt = getpidt();
    char bwsg = 0;
    char offset = 0;
    u8 val;
    struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(skb);
    if (status->bw == RATE_INFO_BW_40)
    {
        bwsg |= 2;
    }        
    if (status->enc_flags & RX_ENC_FLAG_SHORT_GI)
    {
        bwsg |= 1;
    }

    if (status->encoding & RX_ENC_HT)
    {
        offset = 12;
    }
    if (pidt != -1)
    {
        skb_out = nlmsg_new(skb->len+115,0); /* extra len for headers and buffer for firmware and driver cross communication */
        if(!skb_out) 
        {
            printk(KERN_INFO "VMAC ERROR: Failed to allocate...(i.e. contact author please)\n"); 
            return;
        }   
        nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, skb->len+108, 0);
        nlh->nlmsg_pid = pidt;
        nlh->nlmsg_len = skb->len - 4 + 100;
        if (nlh->nlmsg_len < 100)
        {
                nlh->nlmsg_len = 100;
        }
        val = status->rate_idx + offset;
        NETLINK_CB(skb_out).dst_group = 0;
        memcpy(&txc.enc[0], &ence, 8);
        memcpy(&txc.seq[0], &seq, 2);
        memcpy(&txc.type, &typee, 1);
        //memcpy(&txc.bwsg, &bwsg, 1);
        //memcpy(&txc.rate_idx, &val, 1);
        //memcpy(&txc.signal, &status->signal, 1);
        memcpy(nlmsg_data(nlh), &txc, sizeof(struct control));
        memcpy(nlmsg_data(nlh) + sizeof(struct control), skb->data, skb->len - 4);
        res = nlmsg_unicast(nl_sk, skb_out, pidt);
        if (res < 0)
        {
            #ifdef DEBUG_VMAC
                printk(KERN_INFO "Failed...\n");
            #endif
        }
    }
    kfree_skb(skb);
    nlmsg_free(skb_out);

}

/**
 * @brief    vmac rx main function note frame types are the following
 * - 0: Interest
 * - 1: Data
 * - 2: DACK
 * - 3: (used by userspace only to register, never comes to this function)
 * - 4: Announcment
 * - 5: Frame injection
 *
 * @param      skb    The socket buffer to be processed
 *
 * Pseudo Code
 *
 * @code{.unparsed}
 *  pull vmac header from frame
 *  if frame type is 0
 *   set sequence value to 0 //pass to upper layer no further action
 *  else if type is 1
 *   read data V-MAC header
 *   read sequence number of frame
 *   find struct for encoding within lookup table
 *   If struct does not exist
 *    free frame
 *    return
 *   else
 *    increment timeout for encoding in LET by 30 seconds
 *   End If
 *
 *   If received frame is after highest received sequence number
 *    while latest sequence number stored is less than received frame seq
 *     increment latest sequence number
 *     indicate in sliding window frame is lost
 *    End While
 *   else if received frame sequence number has been received is indicated by sliding window
 *    freee frame
 *    return
 *   End If
 *
 *   If frame received sequence number is within window and has not been received before
 *    set sliding window index value for that frame to 1
 *   EndIf
 *
 *   If we have NOT calculated time of first frame
 *    record time of reception
 *   else if we have NOT calculated time of second frame
 *    record time of reception
 *    calculate alpha by subtracting second frame timing from first and dividing by sequence number difference
 *    Set first frame timing to 0 //(i.e. reset)
 *      End If
 *
 *      set last index received value to received frame sequence number// note ths is a bug if frame received is retransmission
 *      If frame is 5th frame within round
 *       Calculate actual round number (not sequence numeber)
 *       rcall request DACK function passing encoding and round number
 *       set value for new round number
 *      End If
 *      pull Data type header from frame
 *  else if type is 2
 *   Look up encoding at rx table
 *   look up encoding at tx table
 *   read number of holes in DACK header
 *   read round number in DACK header
 *   pull DACK header from frame
 *   if entry exists at tx table
 *    set data rate to 0 (i.e. 1 mbps lowest)
 *    increment number of dacks received //statistics purposes
 *    set i to 0
 *    while i is less than number of holes
 *     read hole
 *     read le
 *     read re
 *     while le <re && le < sent sequence number
 *      if DACK received round is greater than round recorder for last retx request
 *       lock tx table entry for encoding
 *       copy frame from retransmission buffer
 *       unlock tx table entry for encoding
 *       if frame is valid
 *        set retransmission pacing to current DACK round + 6 (emperically)
 *        call function retrx passing frame and 255 (i.e. data rate selected by function)
 *        vmact increment frame count (statistics purposes)
 *       End If
 *      End If
 *      increment le
 *     End While
 *     pull hole from frame
 *    End While
 *   End If
 *   If entry exists at rx table
 *    if vmac rx entry succeeds at locking dacklock
 *     If current radio is still about to send and round is the same
 *      If radio head another dack already
 *       set dack sending signal to 0
 *       delete pending DACK from transmission
 *      else
 *       increment dack heard
 *      End If
 *     End If
 *     unlock rx entry dacklock
 *    End If
 *   End If
 *   free frame
 *   return
 *  else if type is 4
 *   set sequence to 0 //no further action here
 *  else if type is 5
 *   read sequence number from V-MAC data type header
 *   pull Data type header from frame
 *  else
 *   Free kernel memory from the frame (not V-MAC frame)
 *  End If
 *  call nl_send passing frame,encoding, type of frame, and sequence number (if exists)
 * @endcode
 */
void vmac_rx(struct sk_buff* skb)
{
    u8 type;
    u16 seq, holes, le, re, i = 0, round;
    u64 enc;
    struct encoding_tx *vmact;
    struct ieee80211_hdr hdr;
    struct encoding_rx *vmacr;
    struct vmac_hole *hole;
    struct sk_buff *skb2 = NULL;
    struct vmac_DACK *ddr;
    int maxretx = 20;
    int counter = 0;
    u8 src[ETH_ALEN] __aligned(2) = {0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe};
    u8 dest[ETH_ALEN]__aligned(2) = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    u8 bssid[ETH_ALEN]__aligned(2) = {0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe};
    struct vmac_data *vdr;
    struct vmac_hdr *vmachdr = (struct vmac_hdr*)skb->data;
    type = vmachdr->type;
    enc = vmachdr->enc;
    hdr.duration_id = 11;
    memcpy(hdr.addr1, dest, ETH_ALEN); //was target
    memcpy(hdr.addr2, src, ETH_ALEN);// was target
    memcpy(hdr.addr3, bssid, ETH_ALEN);
    hdr.frame_control = cpu_to_le16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_DATA);
    skb_pull(skb, sizeof(struct vmac_hdr));
    #ifdef DEBUG_VMAC
        printk(KERN_INFO "Encoding received is: %llu\n", enc);
    #endif
    /* interest */
    if (type == VMAC_HDR_INTEREST)
    {
        seq = 0;
    } /* Data */
    else if (type == VMAC_HDR_DATA)
    {
        vdr = (struct vmac_data*)skb->data;
        seq = vdr->seq;
        vmacr = find_rx(RX_TABLE, enc);
        if (!vmacr || vmacr == NULL)
        {
            #ifdef DEBUG_VMAC
                printk(KERN_INFO "Does not Exist\n");
            #endif
            printk(KERN_INFO "Does not exist freeing memory and dropping y'know casual\n");
            kfree_skb(skb);
            return;
        }
        else
        {
         mod_timer(&vmacr->enc_timeout, jiffies + msecs_to_jiffies(30000)); 
        }

        if (vmacr->latest < vdr->seq) //uncomment once checked clear
        {
            while(vmacr->latest < vdr->seq)
            {
                vmacr->latest++;
                vmacr->window[vmacr->latest >= WINDOW ? vmacr->latest % WINDOW : vmacr->latest] = 0;
            }
        }
        else if (vmacr->window[seq >= WINDOW ? seq %WINDOW : seq] == 1)// unnecessary: &&vdr->seq>=(vmacr->latest>window?vmacr->latest%RX_WINDOW:0)
        {
            kfree_skb(skb);
            return;
        }

        if (vdr->seq >= (vmacr->latest >= WINDOW ? vmacr->latest % WINDOW : 0))
        {
            vmacr->window[(seq >= WINDOW ? vdr->seq % WINDOW : vdr->seq)] = 1;
        }

        if (vmacr->firstFrame == 0)
        {
            vmacr->firstFrame = jiffies;
        }
        else if (vmacr->SecondFrame == 0)
        {
            vmacr->SecondFrame = jiffies;
            vmacr->alpha = (((jiffies - vmacr->firstFrame) / (vdr->seq - vmacr->lastin)));
            vmacr->firstFrame = 0;
        }
        vmacr->lastin = vdr->seq;
        if (vmacr->round <= vdr->seq - 5)
        {
            vmacr->round = vdr->seq / 5;
            request_DACK(enc, vmacr->round);
            vmacr->round = vdr->seq;
        } 
        skb_pull(skb, sizeof(struct vmac_data));
        //#ifdef DEBUG_VMAC
            //printk(KERN_INFO "VMAC SEQ: %d", vdr->seq);
        //#endif
    } /* DACK */
    else if(type == VMAC_HDR_DACK)
    {
        #ifdef DEBUG_MO
            printk(KERN_INFO "LOOKING AT DACK\n");
        #endif
        i = 0;
        vmacr = find_rx(RX_TABLE, enc);
        vmact = find_tx(TX_TABLE, enc);        
        vmacr = NULL;
        ddr = (struct vmac_DACK*) skb->data;
        holes = ddr->holes;
        round = ddr->round;
        skb_pull(skb, sizeof(struct vmac_DACK));
        #ifdef DEBUG_MO
            printk(KERN_INFO "Encoding of DACK = %lld", enc);
        #endif
        if (vmact && vmact != NULL)
        {
            spin_lock(&vmact->seqlock);
            seq = vmact->seq;
            spin_unlock(&vmact->seqlock);
            #ifdef DEBUG_MO
                printk(KERN_INFO "Encoding of DACK = %lld holes= %d", enc, holes);
            #endif
            vmact->dackcounter++;
            hole = (struct vmac_hole*) skb->data;
            while(i < holes && holes != 0)
            {
                hole = (struct vmac_hole*)skb->data;
                le = hole->le;
                re = hole->re;
                i++;
                while(le < re && le < seq)
                {
                    if (round >= vmact->timer[(le >= WINDOW_TX ? le % WINDOW_TX : le)] && le >= (seq < WINDOW_TX ? 0 : seq - (WINDOW_TX)))
                    {
                        if (maxretx <= counter)
                            break; /* break off or kernel will crash */
                        if (vmact->retransmission_buffer[(le > WINDOW_TX ? le % WINDOW_TX : le)])
                        {
                            skb2 = skb_copy(vmact->retransmission_buffer[(le >= WINDOW_TX ? le % WINDOW_TX : le)], GFP_KERNEL); //mo here
                            counter++;
                        }
                        vmact->timer[le % WINDOW_TX] = round + 6;
                        if(skb2)
                        {
//                            retrx(skb2, 255); mo here
                            vmac_send_hack(skb2);                            
                            vmact->framecount++;
                        } 
                    }
                    le++;
                }
                skb_pull(skb, sizeof(struct vmac_hole)); //dont pull dumbass might be needed at bottom........or...convolute things, probably easier. didn't work, will just make a copy safer....(kinda lazy to do better way lol)
            }
        }
        if (vmacr && vmacr != NULL)
        {
            if (spin_trylock(&vmacr->dacklok))
            {
                if (vmacr->dac_info.send == 1 && vmacr->dac_info.round == round)
                {
                    if (vmacr->dac_info.dacksheard >= 1)
                    {
                        vmacr->dac_info.send = 0;
                    }
                    else vmacr->dac_info.dacksheard++;
                }
                spin_unlock(&vmacr->dacklok); 
            }
        }
        kfree_skb(skb);
        return;
    } /* Announcement */
    else if (type == VMAC_HDR_ANOUNCMENT)
    {
        seq = 0;
    } /* Injected frame */
    else if (type == VMAC_HDR_INJECTED)
    {
        vdr = (struct vmac_data*) skb->data;
        seq = vdr->seq;
        skb_pull(skb, sizeof(struct vmac_data));
    } /* Unknown frame type */
    else
    {
        kfree_skb(skb); 
        return;
    }
    nl_send(skb, enc, type, seq);
}

/**
 * @brief      Receives frames from low-level driver kernel module and filters V-MAC frames from non V-MAC frames.
 *
 * @param      hw    The hardware struct
 * @param      skb   The skb
 * 
 * Pseudo Code
 * 
 * @code{.unparsed}
 *  if received frame 802.11 header has at first two bytes value 0xfe //(we assume it is V-MAC)
 *      Remove 802.11 header
 *      read V-MAC header
 *      if frame type is interest, data, announcment, or frame injection
 *          call vmac_rx passing frame  i.e. core
 *      else if frame type is DACK
 *          add frame to management queue
 *      else 
 *          free kernel memory from frame i.e. not V-MAC frame
 *  Else
 *      free kernel memory of frame     i.e. not V-MAC frame
 *  End If
 * @endcode
 */
void ieee80211_rx_vmac(struct ieee80211_hw *hw, struct sk_buff *skb)
{
    struct ieee80211_hdr* hdr = (struct ieee80211_hdr*)skb->data;
    struct vmac_hdr *vmachdr;
    #ifdef DEBUG_VMAC
        struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(skb);
    #endif
    u8 type;
    if (hdr->addr2[0] == 0xfe && hdr->addr2[1] == 0xfe)
    {
        #ifdef DEBUG_VMAC
            if (status->enc_flags & RX_ENC_FLAG_SHORT_GI)
            {
                printk(KERN_INFO "VMAC: Short GI\n");
            }        
            if (status->bw == RATE_INFO_BW_40)
            {
                printk(KERN_INFO "VMAC: BW40\n");
            }
            printk(KERN_INFO "V-MAC, Rate: %d\n", status->rate_idx);
            printk(KERN_INFO "VMAC: signal val: %d\n", -status->signal);
        #endif
        
        skb_pull(skb, sizeof(struct ieee80211_hdr)); 
        vmachdr = (struct vmac_hdr*)skb->data;
        type = vmachdr->type;
        if (type == VMAC_HDR_INTEREST || type == VMAC_HDR_DATA || type == VMAC_HDR_ANOUNCMENT || type == VMAC_HDR_INJECTED)
        {
            vmac_rx(skb);
        }
        else if (type == VMAC_HDR_DACK)
        {            
            // add_mgmt(skb); mo here
        }
        else
        {
            #ifdef DEBUG_VMAC
                printk(KERN_INFO "ERROR: Received type= %d\n",type);
            #endif
            kfree_skb(skb);
        }
    }
    else 
    {
        #ifdef ENABLE_OVERHEARING
            #ifdef DEBUG_VMAC
                if (status->enc_flags & RX_ENC_FLAG_SHORT_GI)
                {
                    printk(KERN_INFO "VMAC: Short GI\n");
                }        
                if (status->bw == RATE_INFO_BW_40)
                {
                    printk(KERN_INFO "VMAC: BW40\n");
                }
                if (status->rate_idx != 0)
                {
                    printk(KERN_INFO "V-MAC, Rate: %d\n", status->rate_idx);
                    printk(KERN_INFO "VMAC: signal val: %d\n", -status->signal);
                }
            #endif
            nl_send(skb, 0, V_MAC_OVERHEAR, 0);
        #else
            kfree_skb(skb);
        #endif
    }
}
EXPORT_SYMBOL(ieee80211_rx_vmac);
