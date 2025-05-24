#!/usr/bin/env bash

if [ "$1" = "i" ]; then
  cd src || exit 1

  make clean
  make

  if sudo dmsetup info dmp1 &>/dev/null; then
    sudo dmsetup remove dmp1
  fi

  if lsmod | grep -q "^dmp"; then
    sudo rmmod dmp
  fi

  if sudo dmsetup info zero1 &>/dev/null; then
    sudo dmsetup remove zero1
  fi

  sudo insmod dmp.ko
  sudo dmsetup create zero1 --table "0 4096 zero"
  sudo dmsetup create dmp1 --table "0 4096 dmp /dev/mapper/zero1"
fi

if [ "$1" = "w" ]; then
  cat /sys/module/dmp/stat/volumes
  sudo dd if=/dev/random of=/dev/mapper/dmp1 bs=4k count=1
  cat /sys/module/dmp/stat/volumes
fi

if [ "$1" = "r" ]; then
  cat /sys/module/dmp/stat/volumes
  sudo dd of=/dev/null if=/dev/mapper/dmp1 bs=4k count=1
  cat /sys/module/dmp/stat/volumes
fi

if [ "$1" = "clean" ]; then
  cd src || exit 1
  make clean
fi
