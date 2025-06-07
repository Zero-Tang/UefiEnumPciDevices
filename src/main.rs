#![no_std]
#![no_main]

extern crate alloc;

use core::arch::asm;
use uefi::{allocator::Allocator, proto::media::file::{File, FileAttribute, FileMode, RegularFile}, *};
use proto::console::text::*;
use runtime::ResetType;
use system::*;
use fprint::{_fprint,create_file_in_current_drive};
use acpis::{McfgManager,McfgItem};
use pcilist::*;

#[macro_use] mod fprint;
mod acpis;
mod pcilist;

#[global_allocator] static EFI_ALLOC:Allocator=Allocator;

#[inline(always)] fn outd(port:u16,value:u32)
{
	unsafe
	{
		asm!
		(
			"out dx,eax",
			in("eax") value,
			in("dx") port
		);
	}
}

#[inline(always)] fn ind(port:u16)->u32
{
	let value:u32;
	unsafe
	{
		asm!
		(
			"in eax,dx",
			out("eax") value,
			in("dx") port
		);
	}
	value
}

#[inline(always)] fn pci_read32(bdf_addr:u16,offset:u8)->u32
{
	outd(0xCF8,((bdf_addr as u32)<<8)|(offset as u32)|0x80000000);
	ind(0xCFC)
}

fn pci_read32x(mcfg_item:Option<&McfgItem>,bus:u8,device:u8,function:u8,offset:u8)->u32
{
	match mcfg_item
	{
		Some(item)=>item.pci_read32(bus,device,function,offset).unwrap(),
		None=>pci_read32(((bus as u16)<<8)|((device as u16)<<3)|(function as u16),offset)
	}
}

fn print_pci_device_info(log_file:&mut RegularFile,mcfg_item:Option<&McfgItem>,bus:u8,device:u8,function:u8)->bool
{
	let h=log_file;
	let v=pci_read32x(mcfg_item,bus,device,function,0);
	if v!=u32::MAX
	{
		let did=(v>>16) as u16;
		let vid=(v&0xffff) as u16;
		if let Some(item)=mcfg_item
		{
			fprint!(h,"{:04X}:",item.segment_group);
		}
		fprint!(h,"{bus:02X}:{device:02X}:{function:X} | Vendor ID: 0x{vid:04X}, Device ID: 0x{did:04X} ");
		fprintln!(h,"({} | {})",get_vendor_name(vid as u16),get_device_name(vid as u16,did as u16));
		let class=pci_read32x(mcfg_item,bus,device,function,8).to_be_bytes();
		fprint!(h,"Classification: {class:02X?} ({}",get_class_name(class[0]));
		if let Some(name)=get_subclass_name(class[0],class[1])
		{
			fprint!(h," > {name}");
		}
		if let Some(name)=get_progif_name(class[0],class[1],class[2])
		{
			fprint!(h," > {name}");
		}
		fprintln!(h,") Revision-ID: 0x{:02X}",class[3]);
		let subsystem=pci_read32x(mcfg_item,bus,device,function,0xC);
		let sub_did=(subsystem>>16) as u16;
		let sub_vid=(subsystem&0xffff) as u16;
		if sub_vid!=vid || sub_did!=did
		{
			fprint!(h,"Subsystem Vendor ID: 0x{sub_vid:04X}, Subsystem Device ID: 0x{sub_did:04X}");
			fprintln!(h," ({})",get_subsystem_name(vid,did,sub_vid,sub_did));
		}
	}
	v!=u32::MAX
}

#[inline(always)] fn get_bdf(bdf_addr:u16)->(u8,u8,u8)
{
	let b=(bdf_addr>>8) as u8;
	let d=((bdf_addr>>3)&0x1f) as u8;
	let f=(bdf_addr & 0x7) as u8;
	(b,d,f)
}

fn get_vendor_name(vendor_id:u16)->&'static str
{
	match KNOWN_PCI_VENDORS.binary_search_by(|(id,_name)| id.cmp(&vendor_id))
	{
		Ok(i)=>KNOWN_PCI_VENDORS[i].1,
		Err(_)=>"Unknown Vendor"
	}
}

fn get_device_name(vendor_id:u16,device_id:u16)->&'static str
{
	let vdid:u32=((vendor_id as u32)<<16)|(device_id as u32);
	match KNOWN_PCI_DEVICES.binary_search_by(|(id,_name)| id.cmp(&vdid))
	{
		Ok(i)=>KNOWN_PCI_DEVICES[i].1,
		Err(_)=>"Unknown Device"
	}
}

fn get_subsystem_name(vendor_id:u16,device_id:u16,subsystem_vid:u16,subsystem_did:u16)->&'static str
{
	let ordinal:u64=((vendor_id as u64)<<48)|((device_id as u64)<<32)|((subsystem_vid as u64)<<16)|(subsystem_did as u64);
	match KNOWN_PCI_SUBSYSTEMS.binary_search_by(|(id,_name)| id.cmp(&ordinal))
	{
		Ok(i)=>KNOWN_PCI_SUBSYSTEMS[i].1,
		Err(_)=>"Unknown Subsystem"
	}
}

fn get_class_name(class_id:u8)->&'static str
{
	match KNOWN_PCI_CLASSES.binary_search_by(|(id,_name)| id.cmp(&class_id))
	{
		Ok(i)=>KNOWN_PCI_CLASSES[i].1,
		Err(_)=>"Unknown PCI Class"
	}
}

fn get_subclass_name(class_id:u8,subclass_id:u8)->Option<&'static str>
{
	let ordinal:u16=((class_id as u16)<<8)|(subclass_id as u16);
	match KNOWN_PCI_SUBCLASSES.binary_search_by(|(id,_name)| id.cmp(&ordinal))
	{
		Ok(i)=>Some(KNOWN_PCI_SUBCLASSES[i].1),
		Err(_)=>None
	}
}

fn get_progif_name(class_id:u8,subclass_id:u8,progif_id:u8)->Option<&'static str>
{
	let ordinal:u32=((class_id as u32)<<16)|((subclass_id as u32)<<8)|(progif_id as u32);
	match KNOWN_PCI_PROGIFS.binary_search_by(|(id,_name)| id.cmp(&ordinal))
	{
		Ok(i)=>Some(KNOWN_PCI_PROGIFS[i].1),
		Err(_)=>None
	}
}

fn check_keystroke(key:char,stdin:&mut Input)->bool
{
	let uk=Char16::try_from(key).unwrap();
	let mut evt=[stdin.wait_for_key_event().unwrap()];
	let _=boot::wait_for_event(&mut evt).discard_errdata();
	if let Some(k)=stdin.read_key().unwrap()
	{
		return k==Key::Printable(uk);
	}
	return false;
}

fn block_until_keystroke(key:char)
{
	loop
	{
		let r=with_stdin(|stdin| check_keystroke(key, stdin));
		if r {break;}
	}
}

#[entry] fn main()->Status
{
	// Create the file used for logging.
	let mut h=match create_file_in_current_drive("output.txt",true,FileMode::CreateReadWrite,FileAttribute::empty())
	{
		Ok(f)=>f,
		Err(e)=>panic!("Failed to create file! Reason: {e}")
	};
	fprintln!(&mut h,"PCI-IDS Version: {}, Date: {}",KNOWN_PCI_LIST_VERSION,KNOWN_PCI_LIST_DATE);
	let mut count:usize=0;
	// Check if MCFG is present in ACPI.
	match McfgManager::new()
	{
		Some(mcfg_mgr)=>
		{
			// We'll use MCFG table to enumerate PCI devices.
			fprintln!(&mut h,"Found MCFG! {mcfg_mgr}");
			for mcfg_item in mcfg_mgr.as_slice()
			{
				fprintln!(&mut h,"Working on PCI Segment Group 0x{:04X}...",mcfg_item.segment_group);
				for bus in mcfg_item.start_bus..=mcfg_item.end_bus
				{
					for dev in 0..32
					{
						for func in 0..8
						{
							if print_pci_device_info(&mut h,Some(mcfg_item),bus,dev,func)
							{
								count+=1;
							}
						}
					}
				}
			}
		}
		None=>
		{
			// We'll use legacy method to enumerate PCI devices.
			fprintln!(&mut h,"No MCFG ACPI Table was found!");
			for i in 0..=u16::MAX
			{
				let (b,d,f)=get_bdf(i);
				if print_pci_device_info(&mut h,None,b,d,f)
				{
					count+=1;
				}
			}
		}
	}
	fprintln!(&mut h,"Found {count} PCI devices in the system!");
	h.close();
	println!("Press Enter key to shutdown...");
	block_until_keystroke('\r');
	runtime::reset(ResetType::SHUTDOWN,Status::SUCCESS,None);
}

// Panic handler is required for no_std crates. But it is NOT FOR tests!
// Put it under non-test conditional-compilation, or otherwise
// the rust-analyzer of VSCode will report duplicate panic_impl.
#[cfg(not(test))]
mod panicking
{
	use core::panic::PanicInfo;
	use uefi::println;

	#[panic_handler] fn panic(panic: &PanicInfo)->!
	{
		println!("[PANIC] xbp-tester {}",panic);
		loop{}
	}
}