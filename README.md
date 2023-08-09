# V-MAC 802.11ac
V-MAC 802.11ac is a system buit on RTL8812AU chipset. There are many vendors utilizing the chipset, the one we recommend using (been thoroughly tested) is the folllowing [link](https://www.amazon.com/ALFA-AWUS036ACH-%E3%80%90Type-C%E3%80%91-Long-Range-Dual-Band/dp/B08SJC78FH/ref=sr_1_1_sspa?crid=1CB7YX0MJCHG7&keywords=alfa+wifi+802.11ac&qid=1690814966&sprefix=alfa+wifi+802.11ac%2Caps%2C95&sr=8-1-spons&ufe=app_do%3Aamzn1.fos.006c50ae-5d4c-4777-9bc0-4513d670b6bc&sp_csd=d2lkZ2V0TmFtZT1zcF9hdGY&psc=1).

The repository consists of userspace code and kernel module. The kernel module needs to be built and installed first before using the userspace code. Please run the `monitor.sh` script before running the userspace code.

## Video Demo

Please see the following video demonstration of V-MAC capabilities sending 4k video multicast with receivers spread across a house - [link](https://youtu.be/IJAogomyhtc)

Please note this demo is demonstrating capabilities done on this platform that are not integrated yet, the capabilities are producer selection (OPSEL) and multicast rate control.

## Support

For any technical questions/inquiries, please contact mohammed.0.elbadry@gmail.com

For research collaboration and commercial usage, please contact fan.ye@stonybrook.edu

## License

This system is released under cc by-nc-sa 4.0 [link] (https://creativecommons.org/licenses/by-nc-sa/4.0/)


## Reference

[[PDF]](http://www.ece.stonybrook.edu/~fanye/papers/sec20-vmac.pdf) Please cite work work while using the system, thank you!

Elbadry, Mohammed, Fan Ye, Peter Milder, YuanYuan Yang “Pub/Sub in the Air: ANovel Data-centric
Radio Supporting Robust Multicast in Edge Environments.”, Proceedings of the 5th ACM/IEEE
Symposium on Edge Computing (2020) 

