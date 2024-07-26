#pragma once

EFI_STATUS EFIAPI UefiBootServicesTableLibConstructor(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS EFIAPI UefiRuntimeServicesTableLibConstructor(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS EFIAPI UefiLibConstructor(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable);

EFI_SYSTEM_TABLE *gST;
EFI_BOOT_SERVICES *gBS;
EFI_RUNTIME_SERVICES *gRT;
EFI_SIMPLE_TEXT_INPUT_PROTOCOL *StdIn;
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdOut;

CHAR8 gEfiCallerBaseName[]="UefiEnumPciDevices";

typedef union _PCI_CONFIG_ADDRESS_REGISTER
{
	struct
	{
		UINT32 Offset:8;
		UINT32 Function:3;
		UINT32 Device:5;
		UINT32 Bus:8;
		UINT32 Reserved:7;
		UINT32 Enable:1;
	};
	UINT32 Value;
}PCI_CONFIG_ADDRESS_REGISTER,*PPCI_CONFIG_ADDRESS_REGISTER;

typedef union _PCI_CONFIG_CLASS_REGISTER
{
	struct
	{
		UINT8 Class;
		UINT8 Subclass;
		UINT8 ProgIF;
		UINT8 RevisionId;
	};
	UINT32 Value;
}PCI_CONFIG_CLASS_REGISTER,*PPCI_CONFIG_CLASS_REGISTER;

typedef union _PCI_CONFIG_HEADER_TYPE
{
	struct
	{
		UINT8 HeaderType:7;
		UINT8 MultiFunction:1;
	};
	UINT8 Value;
}PCI_CONFIG_HEADER_TYPE,*PPCI_CONFIG_HEADER_TYPE;

CHAR8* UnknownVendor="Unknown Vendor";
CHAR8* UnknownDevice="Unknown Device";
CHAR8* UnknownSubsystem="Unknown Subsystem";
CHAR8* UnknownClass="Unknown Class";
CHAR8* UnknownSubclass="Unknown Subclass";
CHAR8* UnknownProgIF="Unknown Programming Interface";