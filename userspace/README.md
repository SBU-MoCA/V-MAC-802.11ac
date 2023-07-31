# V-MAC Userspace library

## Getting started
To build the code, please use the following commmand:
```
gcc vmac-usrp.c csiphash.c stress-test.c -pthread -lm -o output 
```

To run the code, simply run the binary:

```
./output [arg]
```
#### note please do not forget to turn on radio and setup monitor interface by running `./monitor.sh` first.
The [arg] can either be 'p' or 'c' (i.e. consumer or producer)

## Notes

The code in it's current format does the following: 
- producer runs first, waits till it receives an interest
- run consumer (i.e. ``./output c``)
- once producer receives interest, it sends 500 data frames for the same dataname back to back at 60Mbps nominal data rate.

The system supports sending announcement and frame injections, for more information please refer to vmac-usrp.c and vmac-usrp.h. Feel free to contact me at mohammed.0.elbadry@gmail.com 

## Bugs
The timing calculation in stress-test of goodput is odd and should be fixed to be more intuitive and straightforward.

##
## Reference

Please cite work work while using the system, Thank you!

Paper: Pub/Sub in the Air: A Novel Data-centric Radio Supporting Robust Multicast in Edge Environments
bibtex:

@inproceedings{vmac,
  title={Pub/Sub in the Air: A Novel Data-centric Radio Supporting Robust Multicast in Edge Environments.},
  author={Elbadry, Mohammed and Ye, Fan and Milder, Peter and Yang, YuanYuan},
  booktitle={Proceedings of the 5th ACM/IEEE Symposium on Edge Computing},
  pages={1--14},
  year={2020}
}

