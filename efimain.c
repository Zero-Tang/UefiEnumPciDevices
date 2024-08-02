#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Protocol/LoadedImage.h>
#include <Guid/FileInfo.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>
#include <stdarg.h>
#include "efimain.h"
#include "pci_list.h"

void BlockUntilKeyStroke(IN CHAR16 Unicode)
{
	EFI_INPUT_KEY InKey;
	do
	{
		UINTN fi=0;
		gBS->WaitForEvent(1,&StdIn->WaitForKey,&fi);
		StdIn->ReadKeyStroke(StdIn,&InKey);
	}while(InKey.UnicodeChar!=Unicode);
}

void SetConsoleModeToMaximumRows()
{
	UINTN MaxHgt=0,MaxWdh=0,OptIndex;
	for(UINTN i=0;i<StdOut->Mode->MaxMode;i++)
	{
		UINTN Col,Row;
		EFI_STATUS st=StdOut->QueryMode(StdOut,i,&Col,&Row);
		if(st==EFI_SUCCESS)
		{
			if(Row>=MaxHgt)
			{
				if(Col>MaxWdh)
				{
					OptIndex=i;
					MaxHgt=Row;
					MaxWdh=Col;
				}
			}
		}
	}
	StdOut->SetMode(StdOut,OptIndex);
	StdOut->ClearScreen(StdOut);
}

// Use Binary-Search to reduce running-time complexity.
CHAR8* GetVendorName(UINT16 VendorId)
{
	INT32 Lo=0,Hi=KnownPciVendors;
	while(Hi>=Lo)
	{
		INT32 Mid=(Lo+Hi)>>1;
		if(VendorId>KnownPciVendorIds[Mid])
			Lo=Mid+1;
		else if(VendorId<KnownPciVendorIds[Mid])
			Hi=Mid-1;
		else
			return KnownPciVendorNames[Mid];
	}
	return UnknownVendor;
}

CHAR8* GetDeviceName(UINT16 VendorId,UINT16 DeviceId)
{
	UINT32 VendorDevice=(VendorId<<16)|DeviceId;
	INT64 Lo=0,Hi=KnownPciDevices;
	while(Hi>=Lo)
	{
		INT64 Mid=(Lo+Hi)>>1;
		if(VendorDevice>KnownPciDeviceIds[Mid])
			Lo=Mid+1;
		else if(VendorDevice<KnownPciDeviceIds[Mid])
			Hi=Mid-1;
		else
			return KnownPciDeviceNames[Mid];
	}
	return UnknownDevice;
}

CHAR8* GetSubsystemName(UINT16 VendorId,UINT16 DeviceId,UINT16 SubsystemVendorId,UINT16 SubsystemDeviceId)
{
	UINT64 Ordinal=((UINT64)VendorId<<48)|((UINT64)DeviceId<<32)|((UINT64)SubsystemVendorId<<16)|(UINT64)SubsystemDeviceId;
	INT64 Lo=0,Hi=KnownPciSubsystems;
	while(Hi>=Lo)
	{
		INT64 Mid=(Lo+Hi)>>1;
		if(Ordinal>KnownPciSubsystemIds[Mid])
			Lo=Mid+1;
		else if(Ordinal<KnownPciSubsystemIds[Mid])
			Hi=Mid-1;
		else
			return KnownPciSubsystemNames[Mid];
	}
	return UnknownSubsystem;
}

CHAR8* GetClassName(UINT8 ClassId)
{
	INT16 Lo=0,Hi=KnownPciClasses;
	while(Hi>=Lo)
	{
		INT16 Mid=(Lo+Hi)>>1;
		if(ClassId>KnownPciClassIds[Mid])
			Lo=Mid+1;
		else if(ClassId<KnownPciClassIds[Mid])
			Hi=Mid-1;
		else
			return KnownPciClassNames[Mid];
	}
	return UnknownClass;
}

CHAR8* GetSubclassName(UINT8 ClassId,UINT8 SubclassId)
{
	UINT16 Ordinal=(ClassId<<8)|SubclassId;
	INT32 Lo=0,Hi=KnownPciSubclasses;
	while(Hi>=Lo)
	{
		INT32 Mid=(Lo+Hi)>>1;
		if(Ordinal>KnownPciSubclassIds[Mid])
			Lo=Mid+1;
		else if(Ordinal<KnownPciSubclassIds[Mid])
			Hi=Mid-1;
		else
			return KnownPciSubclassNames[Mid];
	}
	return NULL;
}

CHAR8* GetProgrammingInterfaceName(UINT8 ClassId,UINT8 SubclassId,UINT8 ProgifId)
{
	UINT32 Ordinal=(ClassId<<16)|(SubclassId<<8)|ProgifId;
	INT32 Lo=0,Hi=KnownPciProgifs;
	while(Hi>=Lo)
	{
		INT32 Mid=(Lo+Hi)>>1;
		if(Ordinal>KnownPciProgifIds[Mid])
			Lo=Mid+1;
		else if(Ordinal<KnownPciProgifIds[Mid])
			Hi=Mid-1;
		else
			return KnownPciProgifNames[Mid];
	}
	return NULL;
}

EFI_STATUS CreateFileInCurrentDrive(IN EFI_HANDLE ImageHandle,IN CHAR16* FileName,IN BOOLEAN Truncate,IN UINT64 OpenMode,IN UINT64 Attributes,OUT EFI_FILE_PROTOCOL* *File)
{
	// Locate the root folder of this EFI Application.
	EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
	EFI_STATUS st=gBS->HandleProtocol(ImageHandle,&gEfiLoadedImageProtocolGuid,&LoadedImage);
	if(st==EFI_SUCCESS)
	{
		EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Fsp;
		st=gBS->HandleProtocol(LoadedImage->DeviceHandle,&gEfiSimpleFileSystemProtocolGuid,&Fsp);
		if(st==EFI_SUCCESS)
		{
			EFI_FILE_PROTOCOL *Volume;
			st=Fsp->OpenVolume(Fsp,&Volume);
			if(st==EFI_SUCCESS)
			{
				// Create the file.
				st=Volume->Open(Volume,File,FileName,OpenMode,Attributes);
				if(st==EFI_SUCCESS && Truncate)
				{
					// We need to truncate file contents.
					CHAR8 FileInfoBuff[1024];
					EFI_FILE_INFO *FileInfo=(EFI_FILE_INFO*)FileInfoBuff;
					UINTN Fis=sizeof(FileInfoBuff);
					FileInfo->Size=sizeof(EFI_FILE_INFO);
					st=(*File)->GetInfo(*File,&gEfiFileInfoGuid,&Fis,FileInfo);
					if(st!=EFI_SUCCESS)
						Print(L"Failed to get file info! Status=%u\n",st);
					else
					{
						FileInfo->FileSize=FileInfo->PhysicalSize=0;
						st=(*File)->SetInfo(*File,&gEfiFileInfoGuid,Fis,FileInfo);
						if(st!=EFI_SUCCESS)Print(L"Failed to set file info! Status=%u\n",st);
					}
				}
				Volume->Close(Volume);
			}
		}
	}
	return st;
}

EFI_STATUS FilePrint(IN EFI_FILE_PROTOCOL* File,IN CONST CHAR8* Format,...)
{
	CHAR8 OutputString[512];
	UINTN OutputLength;
	va_list ArgList;
	va_start(ArgList,Format);
	OutputLength=AsciiVSPrint(OutputString,sizeof(OutputString),Format,ArgList);
	va_end(ArgList);
	Print(L"%a",OutputString);
	return File->Write(File,&OutputLength,OutputString);
}

UINT32 EnumeratePciDevicesByPortIO(EFI_FILE_PROTOCOL* LogFile)
{
	UINT32 Count=0;
	for(UINT32 b=0;b<256;b++)
	{
		for(UINT32 d=0;d<32;d++)
		{
			for(UINT32 f=0;f<8;f++)
			{
				UINT16 VendorId,DeviceId;
				// Setup Register Value to write.
				PCI_CONFIG_ADDRESS_REGISTER ConfigAddress;
				ConfigAddress.Offset=0;
				ConfigAddress.Function=f;
				ConfigAddress.Device=d;
				ConfigAddress.Bus=b;
				ConfigAddress.Reserved=0;
				ConfigAddress.Enable=1;
				// Write to PCI...
				__outdword(0xCF8,ConfigAddress.Value);
				// Read from PCI...
				VendorId=__inword(0xCFC);
				DeviceId=__inword(0xCFE);
				// Print out...
				if(VendorId!=0xFFFF && DeviceId!=0xFFFF)
				{
					PCI_CONFIG_HEADER_TYPE HeaderType;
					PCI_CONFIG_CLASS_REGISTER ClassRegister;
					CHAR8 *VendorName=GetVendorName(VendorId),*DeviceName=GetDeviceName(VendorId,DeviceId);
					CHAR8 *ClassName,*SubclassName,*ProgifName;
					// Output Vendor/Device Info...
					FilePrint(LogFile,"Address: %03u.%02u.%01u    ",b,d,f);
					FilePrint(LogFile,"Vendor ID: 0x%04X Device ID: 0x%04X (%a | %a)\n",VendorId,DeviceId,VendorName,DeviceName);
					// Grab Class Info...
					ConfigAddress.Offset=8;
					__outdword(0xCF8,ConfigAddress.Value);
					ClassRegister.Value=__indword(0xCFC);
					ClassName=GetClassName(ClassRegister.Class);
					SubclassName=GetSubclassName(ClassRegister.Class,ClassRegister.Subclass);
					ProgifName=GetProgrammingInterfaceName(ClassRegister.Class,ClassRegister.Subclass,ClassRegister.ProgIF);
					// Output Classification...
					FilePrint(LogFile,"Classification: %02X.%02X.%02X   ",ClassRegister.Class,ClassRegister.Subclass,ClassRegister.ProgIF);
					FilePrint(LogFile,"(%a",ClassName);
					if(SubclassName)
						FilePrint(LogFile," > %a",SubclassName);
					if(ProgifName)
						FilePrint(LogFile," > %a",ProgifName);
					FilePrint(LogFile,")\n");
					// Grab Header Type...
					ConfigAddress.Offset=0xC;
					__outdword(0xCF8,ConfigAddress.Value);
					HeaderType.Value=__inbyte(0xCFE);
					if(HeaderType.HeaderType==0)
					{
						UINT16 SubsystemVendorId,SubsystemDeviceId;
						ConfigAddress.Offset=0x2C;
						__outdword(0xCF8,ConfigAddress.Value);
						SubsystemVendorId=__inword(0xCFC);
						SubsystemDeviceId=__inword(0xCFE);
						if(SubsystemVendorId!=VendorId || SubsystemDeviceId!=DeviceId)
							FilePrint(LogFile,"Subsystem Vendor ID: 0x%04X Subsystem ID: 0x%04X (%a)\n",SubsystemVendorId,SubsystemDeviceId,GetSubsystemName(VendorId,DeviceId,SubsystemVendorId,SubsystemDeviceId));
					}
					FilePrint(LogFile,"\n");
					Count++;
				}
			}
		}
	}
	return Count;
}

UINT32 EnumeratePciDevices(EFI_FILE_PROTOCOL* LogFile)
{
	EFI_ACPI_COMMON_HEADER* McfgHead=EfiLocateFirstAcpiTable(EFI_ACPI_2_0_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_SIGNATURE);
	if(McfgHead==NULL)
		return EnumeratePciDevicesByPortIO(LogFile);
	else
	{
		UINT32 Count=0;
		do
		{
			EFI_ACPI_MEMORY_MAPPED_ENHANCED_CONFIGURATION_SPACE_BASE_ADDRESS_ALLOCATION_STRUCTURE *McfgArray=(EFI_ACPI_MEMORY_MAPPED_ENHANCED_CONFIGURATION_SPACE_BASE_ADDRESS_ALLOCATION_STRUCTURE*)((UINTN)McfgHead+sizeof(EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_HEADER));
			const UINT32 TableCount=(McfgHead->Length-sizeof(EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_HEADER))/sizeof(EFI_ACPI_MEMORY_MAPPED_ENHANCED_CONFIGURATION_SPACE_BASE_ADDRESS_ALLOCATION_STRUCTURE);
			for(UINT32 i=0;i<TableCount;i++)
			{
				Print(L"Enumerating PCI Segment 0x%04X via MCFG base 0x%llX...\n",McfgArray[i].PciSegmentGroupNumber,McfgArray[i].BaseAddress);
				for(UINT32 b=McfgArray[i].StartBusNumber;b<McfgArray[i].EndBusNumber;b++)
				{
					for(UINT32 d=0;d<32;d++)
					{
						for(UINT32 f=0;f<8;f++)
						{
							const UINT64 MmioBase=McfgArray[i].BaseAddress+(b<<20)+(d<<15)+(f<<12);
							UINT16 VendorId=*(UINT16*)(MmioBase+0);
							UINT16 DeviceId=*(UINT16*)(MmioBase+2);
							if(DeviceId!=0xFFFF && VendorId!=0xFFFF)
							{
								PCI_CONFIG_CLASS_REGISTER ClassRegister={.Value=*(UINT32*)(MmioBase+8)};
								PCI_CONFIG_HEADER_TYPE HeaderType={.Value=*(UINT8*)(MmioBase+0xE)};
								// Get Names...
								CHAR8* VendorName=GetVendorName(VendorId);
								CHAR8* DeviceName=GetDeviceName(VendorId,DeviceId);
								CHAR8* ClassName=GetClassName(ClassRegister.Class);
								CHAR8* SubclassName=GetSubclassName(ClassRegister.Class,ClassRegister.Subclass);
								CHAR8* ProgifName=GetProgrammingInterfaceName(ClassRegister.Class,ClassRegister.Subclass,ClassRegister.ProgIF);
								// Output
								FilePrint(LogFile,"Address: %04X:%03u:%02u:%01u    ",McfgArray[i].PciSegmentGroupNumber,b,d,f);
								FilePrint(LogFile,"Vendor ID: 0x%04X Device ID: 0x%04X (%a | %a)\n",VendorId,DeviceId,VendorName,DeviceName);
								FilePrint(LogFile,"Classification: %02X.%02X.%02X   ",ClassRegister.Class,ClassRegister.Subclass,ClassRegister.ProgIF);
								FilePrint(LogFile,"(%a",ClassName);
								if(SubclassName)
									FilePrint(LogFile," > %a",SubclassName);
								if(ProgifName)
									FilePrint(LogFile," > %a",ProgifName);
								FilePrint(LogFile,")\n");
								if(HeaderType.HeaderType==0)
								{
									UINT16 SubsystemVendorId=*(UINT16*)(MmioBase+0x2C);
									UINT16 SubsystemDeviceId=*(UINT16*)(MmioBase+0x2E);
									if(SubsystemVendorId!=VendorId || SubsystemDeviceId!=DeviceId)
										FilePrint(LogFile,"Subsystem Vendor ID: 0x%04X Subsystem ID: 0x%04X (%a)\n",SubsystemVendorId,SubsystemDeviceId,GetSubsystemName(VendorId,DeviceId,SubsystemVendorId,SubsystemDeviceId));
								}
								FilePrint(LogFile,"\n");
								Count++;
							}
						}
					}
				}
			}
			McfgHead=EfiLocateNextAcpiTable(EFI_ACPI_2_0_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_SIGNATURE,McfgHead);
		}while(McfgHead);
		return Count;
	}
}

EFI_STATUS EFIAPI EfiEntry(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_FILE_PROTOCOL* LogFile;
	EFI_STATUS st;
	// Initialize EDK2 Library.
	UefiBootServicesTableLibConstructor(ImageHandle,SystemTable);
	UefiRuntimeServicesTableLibConstructor(ImageHandle,SystemTable);
	// Initialize Console.
	StdIn=SystemTable->ConIn;
	StdOut=SystemTable->ConOut;
	SetConsoleModeToMaximumRows();
	// Create Log File.
	st=CreateFileInCurrentDrive(ImageHandle,L"output.txt",TRUE,EFI_FILE_MODE_CREATE|EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,0,&LogFile);
	if(st!=EFI_SUCCESS)
		Print(L"Failed to open file! Status=%u\n",st);
	else
	{
		FilePrint(LogFile,"PCI-List Version: %a Date: %a\n\n",KnownPciListVersion,KnownPciListDate);
		// Enumerate PCI Devices...
		UINT32 Count=EnumeratePciDevices(LogFile);
		// Conclusion...
		FilePrint(LogFile,"Detected %u PCI Devices in the system...\n",Count);
		LogFile->Flush(LogFile);
		LogFile->Close(LogFile);
	}
	// Shutdown the system...
	StdOut->OutputString(StdOut,L"Press Enter Key to shutdown...\r\n");
	BlockUntilKeyStroke(L'\r');
	gRT->ResetSystem(EfiResetShutdown,EFI_SUCCESS,0,NULL);
	return st;
}