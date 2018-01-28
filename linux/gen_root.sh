#!/bin/sh
rm -r mnt root.bin
dd if=/dev/zero of=root.bin bs=4M count=64
mkfs.ext2 -F root.bin
mkdir mnt
sudo mount -o loop root.bin mnt
cd mnt
sudo mkdir -p bin etc dev lib proc sbin sys tmp usr usr/bin usr/lib usr/sbin
sudo cp ../../busybox-1.21.1/busybox bin
sudo curl -L http://riscv.org/install-guides/linux-inittab > ../inittab
sudo mv ../inittab etc/
sudo ln -s ../bin/busybox sbin/init

sudo cp -r /home/monjur/spec2006/Speckle/riscv-spec-test lib/

cd ..
sudo umount mnt

