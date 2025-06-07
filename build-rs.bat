@echo off
set binpath=.\target\x86_64-unknown-uefi\release
set imgname=UefiEnumPciDevices.img
set path=%ProgramFiles%\qemu;%path%

if not exist src\pcilist.rs (python make_header.py)
cargo build --target x86_64-unknown-uefi --release

echo Creating Disk Image...
set /A imagesize_kb=2880
set /A imagesize_b=%imagesize_kb*1024
if exist %imgname% (del %imgname%)
fsutil file createnew %imgname% %imagesize_b%
echo Formatting Disk Image...
mformat -i %imgname% -f %imagesize_kb% ::
mmd -i %imgname% ::/EFI
mmd -i %imgname% ::/EFI/BOOT
echo Copying into Disk Image...
mcopy -i %imgname% %binpath%\enum-pci-devices.efi ::/EFI/BOOT/bootx64.efi

echo Converting to VMDK...
qemu-img convert -f raw -O vmdk %imgname% %imgname%.vmdk