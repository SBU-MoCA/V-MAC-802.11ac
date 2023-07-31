/*
* Copyright (c) 2017 - 2022, Mohammed Elbadry
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
void vmac_tx(struct sk_buff* skb, u64 enc, u8 type, u16 seqtmp, u8 rate, u8 bw, u8 sgi, u8 stream, _adapter *mon_adapter);
void vmac_low_tx(struct sk_buff* skb, u16 seq, u8 rate, u8 bw, u8 sgi, u8 stream, _adapter *mon_adapter);
s32 xmit_mo(struct sk_buff *skb, _adapter *padapter, u8 rate, u8 bw, u8 sgi, u8 stream);
