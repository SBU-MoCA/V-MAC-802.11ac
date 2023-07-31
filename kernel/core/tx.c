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
#include <drv_types.h>
#include <hal_data.h>
#include <net/cfg80211.h>
#include "vmac.h"
#include <linux/rhashtable.h>
//#define DEBUG_VMAC
struct ieee80211_tx_control ctr = {};

/**
 * @brief    Vmac core tx handles sending all kinds of frames and processing
 * them properly.
 *
 * @param      skb    The skb
 * @param[in]  enc    The encode
 * @param[in]  type    The type
 * @param[in]  tmprate    The tmprate
 * @param[in]  seqtmp    The seqtmp
 *
 * Pseudo Code
 *
 * @code{.unparsed}
 *  if type of frame is interest
 *      look up tx table for the same encoding
 *      if entry does not exist
 *          allocate struct entry (virtual memory)
 *          init variables
 *          set key to encoding
 *          setup encoding timeout
 *          insert entry into LET
 *      end If
 *      modify timeout of entry in LET 
 *      set vmac header type value to interest
 *      push header into the frame
 *  else if type is data
 *      look up tx table for the same encoding
 *      if entry does not exist
 *          vmalloc entry (virtual memory)
 *          init variables
 *          set key to encoding
 *          setup encoding timeout
 *          insert entry into LET
 *      end If
 *      modify timeout of entry in LET
 *      lock sequence lock within entry
 *      increment sequence number
 *      reset retransmission pacing value for frame
 *      push vmac data header frame into frame
 *      push vmac header frame into frame
 *      lock entry transmission
 *      copy frame into retransmission buffer
 *      unlock entry transmission
 *  else if type is announcment
 *      set data rate to 0 (i.e. lowest rate)
 *      push vmac header into frame
 *  else if type is frame injection
 *      set sequence number of frame header given from upper layer
 *      set type to injected
 *      push vmac data type header to frame
 *      push vmac header to frame
 *  else
 *      free kernel of frame //i.e. unkown format, cnanot process
 *      return
 *  End If
 *  set control station to null
 *  set flags for hardware (including QOS/No ACK, etc)
 *  call enq_uqueue function passing frame, sequence number, and rate
 * @endcode
 */
void vmac_tx(struct sk_buff* skb, u64 enc, u8 type, u16 seqtmp, u8 rate, u8 bw, u8 sgi, u8 stream, _adapter *mon_adapter)
{
    struct enc_cleanup *clean;
    struct dack_info *dac_info;
    struct encoding_rx *vmacr;
    struct encoding_tx *vmact;
    struct vmac_data ddr;
    struct vmac_hdr vmachdr;
    struct ieee80211_hdr hdr;
    struct sk_buff *tmp1;
    struct sk_buff *tmp2 = NULL; 
    u16 seq;
    struct ieee80211_tx_control control = {};    
    vmachdr.type = type;
    vmachdr.enc = enc;

    #ifdef NFD_VNLP
      seq = seqtmp;
    #else
      seq = 0;
    #endif
    printk(KERN_INFO "HERE\n");
    #ifdef DEBUG_VMAC
	printk(KERN_INFO "VMAC: type of frame %d", type);
    #endif
    if (type == VMAC_HDR_INTEREST)
    {
        vmacr = find_rx(RX_TABLE, enc);
        #ifdef DEBUG_VMAC
            printk(KERN_INFO "VMACTX: TEST");
        #endif
        if (!vmacr || vmacr == NULL)
        {
            #ifdef DEBUG_VMAC
                printk(KERN_INFO "VMACTX: making new entry");
            #endif
            vmacr = vmalloc(sizeof(struct encoding_rx));
            clean = &vmacr->clean;
            clean->enc = enc;
            vmacr->key = enc;
            clean->type = CLEAN_ENC_RX;
            dac_info = &vmacr->dac_info;
            dac_info->dack_counter = 0;
            dac_info->send = 0;
            /* init */
            
            vmacr->alpha = 0;
            vmacr->firstFrame = 0;
            vmacr->SecondFrame = 0;
            vmacr->lastin = 0;
            vmacr->latest = 0;
            vmacr->round = 0;
            vmacr->key = enc;
            vmacr->dacksent = 0;
            vmacr->dac_info.send = 0;
            spin_lock_init(&vmacr->dacklok);
            vmacr->dac_info.dacklok = &vmacr->dacklok;
            vmacr->dac_info.dack_timer = &vmacr->dack_timer;
	    #ifdef DEBUG_VMAC
                printk(KERN_INFO "VMAC: about to call rx");
            #endif
            add_rx(vmacr);

            #if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
                timer_setup(&vmacr->dack_timer, __sendDACK, 0);
                #ifdef DEBUG_VMAC
                    printk(KERN_INFO "CALLING MOD TIMER %u\n", vmacr->key);
                #endif
                timer_setup(&vmacr->enc_timeout, __cleanup_rx, 0);
            #else
                setup_timer(&vmacr->dack_timer, __sendDACK, (unsigned long) dac_info);
                setup_timer(&vmacr->enc_timeout, __cleanup, (unsigned long)clean);
            #endif
                mod_timer(&vmacr->dack_timer, jiffies);  
        }
        vmachdr.type = VMAC_HDR_INTEREST;
	#ifdef DEBUG_VMAC
	    printk(KERN_INFO "VMAC: about to mod timer");
	#endif
        mod_timer(&vmacr->enc_timeout, jiffies + msecs_to_jiffies(30000)); /* FIXME: Needs to be defaulted from vmac.h or userspace */
	
	#ifdef DEBUG_VMAC
	    printk(KERN_INFO "VMAC completed");
	#endif
        memcpy(skb_push(skb, sizeof(struct vmac_hdr)), &vmachdr, sizeof(struct vmac_hdr));
    }//Data
    else if(type == VMAC_HDR_DATA)
    {
        #ifdef DEBUG_VMAC
            printk(KERN_INFO "VMACTX: TEST2");
        #endif
        vmact = find_tx(TX_TABLE, enc);
        if (!vmact || vmact == NULL)
        {
            vmact = vmalloc(sizeof(struct encoding_tx));
            clean = &vmact->clean;
            clean->enc = enc;
            clean->type = CLEAN_ENC_TX;
            /* init */
            spin_lock_init(&vmact->seqlock);
            vmact->framecount = 0;
            vmact->seq = 0;
            vmact->key = enc;
            vmact->dackcounter = 0;
            vmact->prevround = 0;
            vmact->prevtime = 0;
            #if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
                timer_setup(&vmact->enc_timeout, __cleanup_tx, 0);
            #else
                setup_timer(&vmact->enc_timeout, __cleanup, (unsigned long) clean);
            #endif
            vmact->round_inc[0] = 0;
            vmact->round_inc[1] = 0;
            vmact->round_inc[2] = 0;
            vmact->round_dec[0] = 0;
            vmact->round_dec[1] = 0;
            vmact->round_dec[2] = 0;
            add_tx(vmact);
        }
        mod_timer(&vmact->enc_timeout, jiffies + msecs_to_jiffies(30000));/* FIXME: Needs to be defaulted from vmac.h or userspace */     
        vmachdr.type = VMAC_HDR_DATA;
        spin_lock(&vmact->seqlock);
        ddr.seq = vmact->seq++;
	    spin_unlock(&vmact->seqlock);
        vmact->timer[ddr.seq % WINDOW_TX] = 0;
        memcpy(skb_push(skb, sizeof(struct vmac_data)), &ddr, sizeof(struct vmac_data));
        memcpy(skb_push(skb, sizeof(struct vmac_hdr)), &vmachdr, sizeof(struct vmac_hdr));
        tmp1 = skb_copy(skb, GFP_KERNEL);
        if (ddr.seq >= WINDOW_TX)
        {  
            #ifdef DEBUG_VMAC
                printk(KERN_INFO "FREEING?!\n");
            #endif 
            tmp2 = (vmact->retransmission_buffer[ddr.seq % WINDOW_TX]);
        }
        #ifdef DEBUG_VMAC
            printk(KERN_INFO "Making copy\n");
        #endif
        if (tmp1)
        {
            vmact->retransmission_buffer[ddr.seq % WINDOW_TX] = tmp1;
        }

        if (tmp2)
        {
            kfree_skb(tmp2);
        }
    }
    else if (type == VMAC_HDR_ANOUNCMENT)
    {
        #ifdef DEBUG_VMAC
            printk(KERN_INFO "VMACTX: TEST3");
        #endif
        hdr.duration_id = 0;
        memcpy(skb_push(skb, sizeof(struct vmac_hdr)), &vmachdr, sizeof(struct vmac_hdr));
    }
    else if (type == VMAC_HDR_INJECTED)
    {
        ddr.seq = seqtmp;
        vmachdr.type = VMAC_HDR_INJECTED;
        memcpy(skb_push(skb, sizeof(struct vmac_data)), &ddr, sizeof(struct vmac_data));
        memcpy(skb_push(skb, sizeof(struct vmac_hdr)), &vmachdr, sizeof(struct vmac_hdr));
    }
    else
    {
        kfree_skb(skb);
        return;
    }
    control.sta = NULL;
    IEEE80211_SKB_CB(skb)->flags = 0;
    IEEE80211_SKB_CB(skb)->flags |= IEEE80211_TX_CTL_DONTFRAG;
    IEEE80211_SKB_CB(skb)->flags |= IEEE80211_TX_INTFL_DONT_ENCRYPT;// |       IEEE80211_TX_CTL_DONTFRAG;
    IEEE80211_SKB_CB(skb)->flags |= IEEE80211_TX_CTL_REQ_TX_STATUS;
    IEEE80211_SKB_CB(skb)->flags |= IEEE80211_TX_CTL_NO_ACK;
    IEEE80211_SKB_CB(skb)->flags |= IEEE80211_QOS_CTL_ACK_POLICY_NOACK;
    #ifdef DEBUG_VMAC
        printk(KERN_INFO "VMAC_MID: Data rate: %02x", rate);
    #endif
    vmac_low_tx(skb, seq, rate, bw, sgi, stream, mon_adapter);
}

/**
 * @brief    { function_description }
 *
 * @param      skb    The skb
 * @param[in]  rate    The rate
 * 
 * @code{.unparsed}
 *  copy spoofed 802.11 header format
 *  push spoofed 802.11 header to frame
 *  set flags to not fragment and request tx_status from hardware
 *  request no ACK needed for frame (i.e. 802.11 standard broadcast)
 *  call lower level driver tx function passing control struct and frame
 * @endcode
 */
void vmac_low_tx(struct sk_buff* skb, u16 seq, u8 rate, u8 bw, u8 sgi, u8 stream, _adapter *mon_adapter)
{
struct ieee80211_radiotap_header *rtap_hdr = NULL;
    u8 src[ETH_ALEN] __aligned(2) = {0x00, 0xc0, 0xca, 0xa8, 0xf2, 0xa2};
    u8 dest[ETH_ALEN]__aligned(2) = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    u8 bssid[ETH_ALEN]__aligned(2) = {0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe};
    struct ieee80211_hdr hdr;
    int ret;
	u8 hdr_buf[64] = {0};
	u32 tmp_32bit = 0;
	u16 rt_len = 8;
	u8 mcs_have = 0;
	u8 flags = 0;
	u8 mcs = 2;
	u8 *ptr = NULL;
	u16 txflags = IEEE80211_RADIOTAP_F_TX_NOACK;
	printk(KERN_INFO "WE ARE HERE AT FAKE SEN  using source mac ADDRESS~\n");
	printk(KERN_INFO " INITI value is %d\n", ret);
	memcpy(hdr.addr1, dest, ETH_ALEN); //was target
    memcpy(hdr.addr2, src, ETH_ALEN);// was target
    memcpy(hdr.addr3, bssid, ETH_ALEN);
    hdr.frame_control = cpu_to_le16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_DATA);
    memcpy(skb_push(skb, sizeof(struct ieee80211_hdr)), &hdr, sizeof(struct ieee80211_hdr));
	ret = xmit_mo(skb, mon_adapter, rate, bw, sgi, stream);
	printk(KERN_INFO "VALUe we got is %d\n", ret);
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24))
static struct xmit_frame* monitor_alloc_mgtxmitframe(struct xmit_priv *pxmitpriv) {
	int tries;
	int delay = 300;
	struct xmit_frame *pmgntframe = NULL;

	for(tries = 3; tries >= 0; tries--) {
		pmgntframe = alloc_mgtxmitframe(pxmitpriv);
		if(pmgntframe != NULL)
			return pmgntframe;
		rtw_udelay_os(delay);
		delay += delay/2;
	}
	return NULL;
}
#endif
s32 xmit_mo(struct sk_buff *skb, _adapter *padapter, u8 rate, u8 bw, u8 sgi, u8 stream)
{
	int ret = 0;
	int rtap_len;
	int qos_len = 0;
	int dot11_hdr_len = 24;
	int snap_len = 6;
	unsigned char *pdata;
	u16 frame_ctl;
	unsigned char src_mac_addr[6];
	unsigned char dst_mac_addr[6];
	struct rtw_ieee80211_hdr *dot11_hdr;
	struct ieee80211_radiotap_header *rtap_hdr;
	struct ieee80211_radiotap_iterator iterator;
	u8 fixed_rate = MGN_1M, bwidth = 0, ldpc = 0, stbc = 0;
	u16 txflags = 0;
	//_adapter *padapter = (_adapter *)rtw_netdev_priv(ndev);

	struct xmit_frame		*pmgntframe;
	struct pkt_attrib	*pattrib;
	unsigned char	*pframe;
	struct rtw_ieee80211_hdr *pwlanhdr;
	struct xmit_priv	*pxmitpriv = &(padapter->xmitpriv);
	struct mlme_ext_priv	*pmlmeext = &(padapter->mlmeextpriv);
	u8 *buf = skb->data;
	u32 len = skb->len;
	u8 category, action;
	int type = -1;
	printk(KERN_INFO "Sending at rate %u, %u, %u\n", rate, bw, sgi);
	if (skb)
		rtw_mstat_update(MSTAT_TYPE_SKB, MSTAT_ALLOC_SUCCESS, skb->truesize);

	if ((pmgntframe = monitor_alloc_mgtxmitframe(pxmitpriv)) == NULL) {
		DBG_COUNTER(padapter->tx_logs.core_tx_err_pxmitframe);
		return NETDEV_TX_BUSY;
	}


	pattrib = &pmgntframe->attrib;
	update_monitor_frame_attrib(padapter, pattrib);

	_rtw_memset(pmgntframe->buf_addr, 0, WLANHDR_OFFSET + TXDESC_OFFSET);

	pframe = (u8 *)(pmgntframe->buf_addr) + TXDESC_OFFSET;

	_rtw_memcpy(pframe, (void*)skb->data, skb->len);

	pattrib->pktlen = skb->len;

	//printk("**** rt mcs %x rate %x raid %d sgi %d bwidth %d ldpc %d stbc %d txflags %x\n", fixed_rate, pattrib->rate, pattrib->raid, sgi, bwidth, ldpc, stbc, txflags);
	//pattrib->rate = MGN_MCS0 + rate;
	if (stream == 0){
		pattrib->rate = MGN_VHT1SS_MCS0;
	}
	else{
		pattrib->rate = MGN_VHT2SS_MCS0;
	}
	pattrib->rate += rate;
//	pattrib->ht_en = _TRUE;
	pattrib->sgi = sgi;
	pattrib->bwmode = bw; // 0-20 MHz, 1-40 MHz, 2-80 MHz
	pattrib->ldpc = ldpc;
	pattrib->stbc = stbc;
	pattrib->retry_ctrl = _FALSE;


	pwlanhdr = (struct rtw_ieee80211_hdr *)pframe;

	pmlmeext->mgnt_seq = GetSequence(pwlanhdr);
	pattrib->seqnum = pmlmeext->mgnt_seq;
	pmlmeext->mgnt_seq++;

	pattrib->last_txcmdsz = pattrib->pktlen;
	dump_mgntframe(padapter, pmgntframe);
	DBG_COUNTER(padapter->tx_logs.core_tx);
	pxmitpriv->tx_pkts++;
	pxmitpriv->tx_bytes += skb->len;
	pattrib->raid = RATEID_IDX_BGN_40M_1SS;
fail:
	rtw_skb_free(skb);
	return NETDEV_TX_OK;
}
