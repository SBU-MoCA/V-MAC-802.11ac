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
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*/
#include <net/mac80211.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/bitmap.h>
#include <linux/inetdevice.h>
#include <net/net_namespace.h>
#include <net/cfg80211.h>
#include <linux/mutex.h>
#include <net/addrconf.h>
#include <net/ieee80211_radiotap.h>
#include <net/cfg80211.h>
#include <linux/hashtable.h> 
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include "rtw_xmit.h"
#include "tx.h"
#include "clean.h"
#include "dack.h"
/*const*/


/* defines */
#define DEBUG_MO

/* NETLINK Kernel Module Registration */
#define VMAC_USER           29
#define KERNEL                4.19
/* V-MAC Headers Frame Control */
#define VMAC_HDR_INTEREST   0x00
#define VMAC_HDR_DATA 0x01
#define VMAC_HDR_DACK 0x02
#define VMAC_HDR_ANOUNCMENT 0x03
#define VMAC_HDR_INJECTED 0x05
#define V_MAC_OVERHEAR 0x06

#define sizerx 450
#define WINDOW 700
#define WINDOW_TX 500

/* VMAC ENUMS */
enum clean_type {
    CLEAN_ENC_TX,
    CLEAN_ENC_RX,
};
enum table_type{
    RX_TABLE,
    TX_TABLE,
};


/* prototype functions */
/*static void nl_send(struct sk_buff* skb, u64 enc, u8 type, u16 seq);
static void nl_recv(struct sk_buff* skb);
void vmac_tx(struct sk_buff* skb, u64 enc, u8 type, u8 rate,u16 seq);
void vmac_low_tx(struct sk_buff* skb, u8 rate);
void vmac_rx(struct sk_buff* skb);
void insert(void);
int sta_info_init(struct ieee80211_local *local);
*/
void exit_vmac(void);
/**
 * 0=interest
 * 1=data
 * 2=DACK
 ***/
struct vmac_hdr{
    u64 enc;
    u8 type;
}__packed;

struct vmac_data{
    u16 seq;
}__packed;
struct vmac_DACK{
    u16 holes;  
    u16 round;
}__packed;
struct vmac_hole{
    u16 le;
    u16 re;
}__packed;
/**
 * 
 * vmac queue 
 * */
struct vmac_queues_status{ 
    spinlock_t flock;
    spinlock_t mlock;
    spinlock_t rlock;
};

struct vmac_queue{
    struct list_head list;
    struct sk_buff* frame;
    u64 enc;
    u16 seq;
    u8 type;
    u8 rate;
    struct rate_decision* decis;

};

struct dackprep{
    struct list_head list;
    u64 enc;
    u32 round;
    int k;
};

struct dack_info
{
    int send; 
    u16 dack_counter;
    u8 round;
    u8 dacksheard;
    spinlock_t*  dacklok;
    struct timer_list* dack_timer;
    struct sk_buff* dack;
    
};

struct enc_cleanup{
    u64 enc;
    u8 type; 
};
struct retrx_out_info
{
    u64 enc;
    u16 num;    
};


struct rate_decision
{
    u8 rate_idx[3];
    u8 rate;
};

struct encoding_tx
{
    u64 key;
    struct enc_cleanup clean;
    struct sk_buff *retransmission_buffer[WINDOW_TX];
    u8 timer[WINDOW_TX]; 
    u16 seq;
    u16 offset; 
    u64 retrxout;
    u64 prevtime;
    u16 prevround;  
    struct timer_list enc_timeout;  
    struct mutex mt;
    spinlock_t seqlock;
    u16 dackcounter;
    u16 framecount;
    u8 round_inc[3];
    u8 round_dec[3];
    struct hlist_node node;
};

struct encoding_rx
{
    u64 key;
    struct enc_cleanup clean;    
    u16 window[WINDOW];
    u32 alpha;
    u32 firstFrame;
    u32 SecondFrame;
    u16 lastin;
    u16 latest; 
    struct dack_info dac_info;
    u16 round;
    u16 offset;
    u16 dacksent;
    spinlock_t dacklok;
    struct sk_buff* DACK;
    struct timer_list enc_timeout;
    struct timer_list dack_timer;
    struct hlist_node node;
};



/**
 ** ABI Be careful when changing to adjust userspace information as well.
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
