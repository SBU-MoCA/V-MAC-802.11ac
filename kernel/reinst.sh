#!/bin/sh

sudo make
sudo make dkms_remove
sudo make dkms_install

