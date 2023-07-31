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
struct sock* getsock(void);
int getpidt(void);
void vmac_send_hack(struct sk_buff* skb);
void init_tables(void);
struct encoding_tx* find_tx(int table, u64 enc);
struct encoding_rx* find_rx(int table, u64 enc);
void add_rx(struct encoding_rx*);
void add_tx(struct encoding_tx*);
