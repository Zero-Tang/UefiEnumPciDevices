# UefiEnumPciDevices
A simple UEFI-based tool that enumerates PCI devices in the system.

## Introduction
This project is intended for enumerating PCI devices in the system without any interferences from OS.

## Build
You will need to setup the following pre-requisites:

- Mount [EWDK11-26100 with VS Build Tools 17.8.6](https://learn.microsoft.com/en-us/legal/windows/hardware/enterprise-wdk-license-2022) to V: drive. You may use [WinCDEmu](https://wincdemu.sysprogs.org/download/) to mount ISO images.
- Install [Python 3](https://www.python.org/downloads/windows/). The minimum version of Python must be 3.9.
- Install [Git for Windows](https://git-scm.com/download/win).
- Install [mtools](https://github.com/Zero-Tang/NoirVisor/files/12706542/mtools-4.0.43-bin.zip). You need to extract these files and put them in `PATH` variable.
- Install [QEMU for Windows](https://qemu.weilnetz.de/w64/). This is intended for converting disk images.
- Clone [EDK-II-Library](https://github.com/Zero-Tang/EDK-II-Library) and build `MdePkg` libraries. You need to add the git repository path to `EDK2_PATH` environment variable.

### Setup EDK2
Hopefully the following commands can help you setup and build EDK2.
```
git clone https://github.com/Zero-Tang/EDK-II-Library.git
cd EDK-II-Library
git config --global core.longpaths true
git submodule update --init --recursive
python make_script.py prep MdePkg MdePkg
python make_script.py build MdePkg MdePkg Checked
```

### Build This Project
You may execute `build.bat` in order to build the EFI executable and disk images. You need Internet access in order to fetch latest PCI-ID list.

- `bootx64.efi` is the EFI executable.
- `UefiEnumPciDevices.img` is a raw disk image. You may use it on QEMU.
- `UefiEnumPciDevices.img.vmdk` is a VMDK disk image. You may use it on VMware.

## Rust
This tool is remastered with Rust. However, there still are pre-requisites to be done:

- Install [Rust](https://www.rust-lang.org/tools/install). You must also ensure you have installed UEFI-related target toolchain.
	```
	rustup target add x86_64-unknown-uefi
	```
- Install [Python 3](https://www.python.org/downloads/windows/). The minimum version of Python must be 3.9.
- Install [mtools](https://github.com/Zero-Tang/NoirVisor/files/12706542/mtools-4.0.43-bin.zip). You need to extract these files and put them in `PATH` variable.
- Install [QEMU for Windows](https://qemu.weilnetz.de/w64/). This is intended for converting disk images.

You may execute `build-rs.bat` in order to build the EFI executable and disk images. You need Internet access in order to fetch latest PCI-ID list.

- `enum-pci-devices.efi` is the EFI executable.
- `UefiEnumPciDevices.img` is a raw disk image. You may use it on QEMU.
- `UefiEnumPciDevices.img.vmdk` is a VMDK disk image. You may use it on VMware.

## Run
There are several ways to run this project.

### Real Machine
To run this project on a real machine, you need to prepare a USB flash stick, setup GUID Partition Table (GPT) and format with FAT32 file system. \
Then create `EFI\BOOT` folders and put `bootx64.efi` into it. \
Output will be written to `output.txt` in the root folder of your USB flash stick.

### QEMU for Windows
To run this project on QEMU for Windows, you may execute `run_qemu.bat`. This script will build this project and run QEMU. \
Output will be written to `output.txt` in this repository.

#### OVMF for Windows
You will need to use [OVMF](https://www.kraxel.org/repos/jenkins/edk2/edk2.git-ovmf-x64-0-20220719.209.gf0064ac3af.EOL.no.nore.updates.noarch.rpm) as the UEFI Firmware for QEMU VM. \
This is an `rpm` package. You may use [7-zip](https://www.7-zip.org/) to extract its contents. You need to place `OVMF-pure-efi.fd` into this repository.

### QEMU + Linux KVM
To run this project on QEMU+KVM from a Linux machine (or WSL2), you need to setup additional packages than QEMU itself. For example, in Ubuntu/Debian-based systems:
```
sudo apt install mtools ovmf
```
Then run QEMU:
```
qemu-system-x86_64 -accel kvm -machine q35 -bios /usr/share/ovmf/OVMF.fd -cpu host -drive format=raw,media=disk,file=UefiEnumPciDevices.img
```
Append additional arguments according to your needs. \
Extract the output:
```
mcopy -i UefiEnumPciDevices.img ::/output.txt output.txt
```

### VMware Workstation
To run this project on VMware Workstation, you need to add `UefiEnumPciDevices.img.vmdk` file to your VM as a hard drive. Do not upgrade the file version when VMware prompts you. \
Then execute `extract.bat` to extract the output.

## License
This project is licensed under the [MIT License](./license.txt).