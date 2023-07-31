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
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,18,0)
void __cleanup(unsigned long data);
#else
void __cleanup_rx(struct timer_list *t);
void __cleanup_tx(struct timer_list *t);
#endif
