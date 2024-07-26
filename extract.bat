@echo off
set binpath=.\compchk_uefix64
set imgname=UefiEnumPciDevices.img

echo Converting from VMDK into RAW...
qemu-img convert -f vmdk -O raw %binpath%\%imgname%.vmdk %binpath%\%imgname%

echo Extracting...
mcopy -i %binpath%\%imgname% ::/output.txt output.txt