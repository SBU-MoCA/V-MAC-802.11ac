## RTL8812AU/21AU and RTL8814AU drivers
### Supports Realtek 8811, 8812, 8814 and 8821 chipsets


### DKMS
This driver can be installed using [DKMS]. This is a system which will automatically recompile and install a kernel module when a new kernel gets installed or updated. To make use of DKMS, install the `dkms` package, which on Debian (based) systems is done like this:
```
$ sudo apt-get install dkms
```

### Installation of Driver
In order to install the driver open a terminal in the directory with the source code and execute the following command:
```
$ sudo ./dkms-install.sh
```

### Removal of Driver
In order to remove the driver from your system open a terminal in the directory with the source code and execute the following command:
```
$ sudo ./dkms-remove.sh
```

### Build / Install with Make
For building & installing the driver with 'make' use
```
$ make
$ make install
```

Package / Build dependencies (Kali)
```
$ apt-get install build-essential
$ apt-get install bc
$ apt-get install libelf-dev
$ apt-get install linux-headers-`uname -r`
```
For Raspberry (RPI 2/3) you will need kernel sources
```
$ wget "https://raw.githubusercontent.com/notro/rpi-source/master/rpi-source" -O /usr/bin/rpi-source
$ chmod 755 /usr/bin/rpi-source
$ rpi-source 
```
Then you need to download and compile the driver on the RPI
```
$ git clone https://github.com/aircrack-ng/rtl8812au -b v5.2.20
$ cd rtl*
$ make
$ cp 8812au.ko /lib/modules/`uname -r`/kernel/drivers/net/wireless
$ depmod -a
$ modprobe 88XXau
```
then run this step to change platform in Makefile, For RPI 2/3:
```
$ sed -i 's/CONFIG_PLATFORM_I386_PC = y/CONFIG_PLATFORM_I386_PC = n/g' Makefile
$ sed -i 's/CONFIG_PLATFORM_ARM_RPI = n/CONFIG_PLATFORM_ARM_RPI = y/g' Makefile
```
But for RPI 3 B+ you will need to run those below
which builds the ARM64 arch driver:
```
$ sed -i 's/CONFIG_PLATFORM_I386_PC = y/CONFIG_PLATFORM_I386_PC = n/g' Makefile
$ sed -i 's/CONFIG_PLATFORM_ARM64_RPI = n/CONFIG_PLATFORM_ARM64_RPI = y/g' Makefile
```

### For setting monitor mode
  1. run mon.sh script provided in the repo in parent directory.