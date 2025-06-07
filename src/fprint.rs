// fprint facility

use core::{ffi::c_void, fmt, ptr::null_mut};
use alloc::{string::String, vec::Vec};

use uefi::{boot, print, proto::{loaded_image::LoadedImage, media::{file::{File, FileAttribute, FileMode, RegularFile}, fs::SimpleFileSystem}, ProtocolPointer}, table::system_table_raw, CStr16, Identify, Status};

pub fn _fprint(handle:&mut RegularFile,args:fmt::Arguments)
{
	let mut s=String::new();
	let _=fmt::write(&mut s,args);
	let _=handle.write(s.as_bytes());
	let _=handle.flush();
	// Also print it onto the console.
	print!("{args}");
}

pub fn create_file_in_current_drive(file_name:&str,truncate:bool,open_mode:FileMode,attrib:FileAttribute)->Result<RegularFile,Status>
{
	// The `gBS->HandleProtocol` is not implemented in the uefi crate.
	let handle_protocol=unsafe
	{
		let bs=system_table_raw().unwrap().as_ref().boot_services.as_ref().unwrap();
		bs.handle_protocol
	};
	// Locate the loaded image protocol.
	let loaded_image=unsafe
	{
		let mut p:*mut c_void=null_mut();
		let g=LoadedImage::GUID;
		match handle_protocol(boot::image_handle().as_ptr(),&raw const g,&raw mut p)
		{
			Status::SUCCESS=>&mut *LoadedImage::mut_ptr_from_ffi(p),
			st=>return Err(st)
		}
	};
	// Locate the simple file system protocol through the loaded-image's device.
	let fsp=match loaded_image.device()
	{
		Some(h)=>
		{
			let g=SimpleFileSystem::GUID;
			unsafe
			{
				let mut p:*mut c_void=null_mut();
				match handle_protocol(h.as_ptr(),&raw const g,&raw mut p)
				{
					Status::SUCCESS=>&mut *SimpleFileSystem::mut_ptr_from_ffi(p),
					st=>return Err(st)
				}
			}
		}
		None=>panic!("No device was found for current image!")
	};
	// Open the volume.
	let mut vol=match fsp.open_volume()
	{
		Ok(d)=>d,
		Err(e)=>return Err(e.status())
	};
	// Translate the UTF-8 to UTF-16. Also append null-terminator.
	let mut v:Vec<u16>=file_name.encode_utf16().collect();
	let fname=
	{
		v.push(0);
		let s=CStr16::from_u16_with_nul(v.as_slice()).unwrap();
		s
	};
	// Open the file.
	let mut h=match vol.open(fname,open_mode,attrib)
	{
		Ok(h)=>h,
		Err(e)=>return Err(e.status())
	}.into_regular_file().unwrap();
	// Truncate the file.
	if truncate
	{
		let _=h.set_position(0);
	}
	// Close the volume.
	vol.close();
	Ok(h)
}

macro_rules! fprint
{
	($handle:expr,$($arg:tt)*) =>
	{
		(_fprint($handle,format_args!($($arg)*)))		
	};
}

macro_rules! fprintln
{
	($handle:expr) =>
	{
		fprint!($handle,"\n")
	};
	($handle:expr,$($arg:tt)*) =>
	{
		fprint!($handle,"{}\n",format_args!($($arg)*))
	};
}