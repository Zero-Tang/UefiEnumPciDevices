@echo off
set ddkpath=V:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.38.33130
set path=%ddkpath%\bin\Hostx64\x64;%path%
set incpath=V:\Program Files\Windows Kits\10\Include\10.0.26100.0
set mdepath=%EDK2_PATH%\edk2\MdePkg
set libpath=%EDK2_PATH%\bin\MdePkg
set binpath=.\compchk_uefix64
set objpath=.\compchk_uefix64\Intermediate
set imgname=UefiEnumPciDevices.img

cls

if not exist %objpath% (mkdir %objpath%)

echo Generating PCI-ID List...
python make_header.py

echo Compiling...
cl .\efimain.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /I"%ddkpath%\include" /nologo /Zi /W3 /WX /Od /Oi /D"_efi_boot" /FAcs /Fa"%objpath%\efimain.cod" /Fo"%objpath%\efimain.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /utf-8 /c

echo Linking...
link "%objpath%\*.obj" /NODEFAULTLIB /LIBPATH:"%libpath%\compchk_uefix64" "MdePkgGuids.lib" "BaseLib.lib" "BaseDebugPrintErrorLevelLib.lib" "BaseMemoryLib.lib" "BasePrintLib.lib" "UefiLib.lib" "UefiDebugLibConOut.lib" "UefiMemoryAllocationLib.lib" "UefiBootServicesTableLib.Lib" "UefiRuntimeServicesTableLib.Lib" "RegisterFilterLibNull.lib" "UefiDevicePathLib.lib" /NOLOGO /OUT:"%binpath%\bootx64.efi" /SUBSYSTEM:EFI_APPLICATION /ENTRY:"EfiEntry" /DEBUG /PDB:"%binpath%\bootx64.pdb" /Machine:X64

echo Creating Disk Image...
set /A imagesize_kb=2880
set /A imagesize_b=%imagesize_kb*1024
if exist %binpath%\%imgname% (del %binpath%\%imgname%)
fsutil file createnew %binpath%\%imgname% %imagesize_b%
echo Formatting Disk Image...
mformat -i %binpath%\%imgname% -f %imagesize_kb% ::
mmd -i %binpath%\%imgname% ::/EFI
mmd -i %binpath%\%imgname% ::/EFI/BOOT
echo Copying into Disk Image...
mcopy -i %binpath%\%imgname% %binpath%\bootx64.efi ::/EFI/BOOT

echo Launching QEMU...
qemu-system-x86_64 -machine q35 -cpu max -bios OVMF-pure-efi.fd -drive format=raw,media=disk,file=.\compchk_uefix64\UefiEnumPciDevices.img -nographic

mcopy -i %binpath%\%imgname% ::/output.txt output.txt

pause.