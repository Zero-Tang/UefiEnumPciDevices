use acpi::{mcfg::{Mcfg, McfgEntry}, AcpiHandler, PhysicalMapping};
use uefi::{println, system::with_config_table, table::cfg::{ACPI2_GUID,ACPI_GUID}};
use core::{arch::x86_64::_mm_mfence, cmp::Ordering, ffi::c_void, fmt::{self,Display}, ptr::{null,NonNull}};
use alloc::vec::Vec;

#[derive(Clone)] struct AcpiMapper;

impl AcpiHandler for AcpiMapper
{
	unsafe fn map_physical_region<T>(&self, physical_address: usize, size: usize) -> PhysicalMapping<Self, T>
	{
		unsafe
		{
			PhysicalMapping::new(physical_address,NonNull::new_unchecked(physical_address as *mut T),size,size,AcpiMapper)
		}
	}

	fn unmap_physical_region<T>(_region: &acpi::PhysicalMapping<Self, T>)
	{
		
	}
}

// Rewrap the McfgEntry so that it's aligned and also comparable.
#[derive(PartialEq, PartialOrd, Eq)] pub struct McfgItem
{
	pub base:u64,
	pub segment_group:u16,
	pub start_bus:u8,
	pub end_bus:u8
}

impl McfgItem
{
	fn new(entry:&McfgEntry)->Self
	{
		Self
		{
			base:{entry.base_address},
			segment_group:{entry.pci_segment_group},
			start_bus:entry.bus_number_start,
			end_bus:entry.bus_number_end
		}
	}

	pub fn pci_read32(&self,bus:u8,device:u8,function:u8,offset:u8)->Option<u32>
	{
		if (self.start_bus..=self.end_bus).contains(&bus)
		{
			let mcfg_offset:usize=(((bus-self.start_bus) as usize)<<20)|((device as usize)<<15)|((function as usize)<<12)|(offset as usize);
			let v:u32=unsafe
			{
				_mm_mfence();
				let v=(self.base as *const u32).byte_add(mcfg_offset).read_unaligned();
				_mm_mfence();
				v
			};
			Some(v)
		}
		else
		{
			None
		}
	}
}

impl Display for McfgItem
{
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result
	{
		write!(f,"PCI Segment Group: 0x{:04X}, ",self.segment_group)?;
		write!(f,"PCI Bus Start Number: 0x{:02X}, ",self.start_bus)?;
		write!(f,"PCI Bus End Number: 0x{:02X}, ",self.end_bus)?;
		write!(f,"MCFG Base Address: 0x{:016X}",self.base)
	}
}

// This trait is required for binary-searching.
impl Ord for McfgItem
{
	fn cmp(&self, other: &Self) -> Ordering
	{
		match self.segment_group.cmp(&other.segment_group)
		{
			Ordering::Equal=>self.start_bus.cmp(&other.start_bus),
			r=>r
		}
	}
}

pub struct McfgManager
{
	// This list is sorted in order to allow binary-searching.
	list:Vec<McfgItem>
}

impl McfgManager
{
	pub fn new()->Option<Self>
	{
		let mut mcfg_bases:Vec<McfgItem>=Vec::new();
		// Locate ACPI Base Address, preferably the XSDT.
		let acpi_base=with_config_table
		(
			|table|
			{
				let mut acpi1_base:*const c_void=null();
				for config in table
				{
					if config.guid==ACPI_GUID
					{
						println!("Found ACPI1 base at {:p}",config.address);
						acpi1_base=config.address;
						continue;
					}
					if config.guid==ACPI2_GUID
					{
						println!("Found ACPI2 base at {:p}",config.address);
						return Some(config.address);
					}
				}
				Some(acpi1_base)
			}
		);
		// If found, enumerate the MCFG.
		match unsafe{acpi::AcpiTables::from_rsdp(AcpiMapper,acpi_base.unwrap() as usize)}
		{
			Ok(tables)=>
			{
				match tables.find_table::<Mcfg>()
				{
					Ok(t)=>
					{
						for e in t.entries()
						{
							let entry=McfgItem::new(e);
							match mcfg_bases.binary_search(&entry)
							{
								// It's weird to find the same MCFG Entry. Let's panic here.
								Ok(_)=>panic!("Found equal MCFG Entry! Segment-Group: {:04X}, Bus: {:02X}",entry.segment_group,entry.start_bus),
								Err(i)=>mcfg_bases.insert(i,entry)
							}
						}
					}
					Err(_)=>return None
				}
			}
			Err(_)=>return None
		}
		if mcfg_bases.len()!=0
		{
			Some(McfgManager{list:mcfg_bases})
		}
		else
		{
			None
		}
	}

	pub fn as_slice(&self)->&[McfgItem]
	{
		self.list.as_slice()
	}
}

impl Display for McfgManager
{
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result
	{
		write!(f,"[")?;
		for (i,e) in self.list.iter().enumerate()
		{
			write!(f,"MCFG #{i}: {{{e}}}")?;
			if i+1<self.list.len() {write!(f,", ")?;}
		}
		write!(f,"]")
	}
}