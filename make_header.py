# Python tool to generate PCI-IDs header.
import time
import requests
import os
import sys

class device_progif:
	def __init__(self,parent,progif_id:int,progif_name:str):
		self.id:int=progif_id
		self.name:str=progif_name
		self.parent:device_subclass=parent

class device_subclass:
	def __init__(self,parent,subclass_id:int,subclass_name:str):
		self.id:int=subclass_id
		self.name=subclass_name
		self.parent:device_class=parent
		self.progif_list:list[device_progif]=[]

class device_class:
	def __init__(self,class_id:int,class_name:str):
		self.id:int=class_id
		self.name:str=class_name
		self.subclass_list:list[device_subclass]=[]

class subsystem:
	def __init__(self,parent,vendor_id:int,device_id:int,device_name:str):
		self.vendor_id:int=vendor_id
		self.device_id:int=device_id
		self.name:str=device_name
		self.parent:device=parent

class device:
	def __init__(self,parent,device_id:int,device_name:str):
		self.id:int=device_id
		self.name:str=device_name
		self.parent:vendor=parent
		self.subsystem_list:list[subsystem]=[]

class vendor:
	def __init__(self,vendor_id:int,vendor_name:str):
		self.id:int=vendor_id
		self.name:str=vendor_name
		self.device_list:list[device]=[]

def summarize_vendor_devices(vendor_list:list[vendor])->list[device]:
	l:list[device]=[]
	for v in vendor_list:
		l+=v.device_list
	return l

def summarize_subsystems(vendor_list:list[vendor])->list[subsystem]:
	l:list[subsystem]=[]
	for v in vendor_list:
		for d in v.device_list:
			l+=d.subsystem_list
	return l

def summarize_subclasses(class_list:list[device_class])->list[device_subclass]:
	l:list[device_subclass]=[]
	for c in class_list:
		l+=c.subclass_list
	return l

def summarize_programming_interfaces(class_list:list[device_class])->list[device_progif]:
	l:list[device_progif]=[]
	for c in class_list:
		for s in c.subclass_list:
			l+=s.progif_list
	return l

def parse_to_dict(lines:list[str])->dict:
	ret_dict:dict[list,list]={"vendors":[],"classes":[],"version":"","date":""}
	current_vendor:vendor=None
	current_device:device=None
	current_class:device_class=None
	current_subclass:device_subclass=None
	is_class:bool=False
	is_subclass:bool=False
	is_progif:bool=False
	is_vendor:bool=False
	is_device:bool=False
	is_subsys:bool=False
	for l in lines:
		# Parse the line.
		if len(l)==0:
			continue
		if l[0]=='#':
			x=l[1:].strip()
			if x.startswith("Version: "):
				ret_dict["version"]=x[9:]
			elif x.startswith("Date: "):
				ret_dict["date"]=x[9:]
			continue
		if l[0]=='\t':
			if l[1]!='\t':
				is_subclass=is_class
				is_device=is_vendor
				is_subsys=False
				is_progif=False
			else:
				is_subsys=is_device
				is_progif=is_subclass
		elif l[0]=='C':
			is_class=True
			is_subclass=False
			is_progif=False
			is_vendor=False
			is_device=False
			is_subsys=False
		elif l[0].isnumeric() or (l[0]>='a' and l[0]<='f'):
			is_vendor=True
			is_device=False
			is_subsys=False
			is_class=False
			is_subclass=False
			is_progif=False
		else:
			print("Unknown Line: {}".format(l),end='')
		tmp=l.split('  ')
		# Add to list.
		if is_vendor:
			if is_subsys:
				tmp2=tmp[0].split(' ')
				vendor_id=int(tmp2[0],16)
				device_id=int(tmp2[1],16)
				device_name="  ".join(tmp[1:])
				current_device.subsystem_list.append(subsystem(current_device,vendor_id,device_id,device_name))
			elif is_device:
				device_id=int(tmp[0][1:],16)
				device_name="  ".join(tmp[1:])
				current_device=device(current_vendor,device_id,device_name)
				current_vendor.device_list.append(current_device)
			else:
				vendor_id=int(tmp[0],16)
				vendor_name="  ".join(tmp[1:])
				current_vendor=vendor(vendor_id,vendor_name)
				ret_dict["vendors"].append(current_vendor)
		elif is_class:
			if is_progif:
				progif_id=int(tmp[0][2:],16)
				progif_name="  ".join(tmp[1:])
				current_subclass.progif_list.append(device_progif(current_subclass,progif_id,progif_name))
			elif is_subclass:
				subclass_id=int(tmp[0][1:],16)
				subclass_name="  ".join(tmp[1:])
				current_subclass=device_subclass(current_class,subclass_id,subclass_name)
				current_class.subclass_list.append(current_subclass)
			else:
				class_id=int(tmp[0][2:],16)
				class_name="  ".join(tmp[1:])
				current_class=device_class(class_id,class_name)
				ret_dict["classes"].append(current_class)
	return ret_dict

def main(lines:list[str]):
	r=parse_to_dict(lines)
	vendor_list:list[vendor]=r["vendors"]
	device_list:list[device]=summarize_vendor_devices(vendor_list)
	subsys_list:list[subsystem]=summarize_subsystems(vendor_list)
	class_list:list[device_class]=r["classes"]
	subclass_list:list[device_subclass]=summarize_subclasses(class_list)
	progif_list:list[device_progif]=summarize_programming_interfaces(class_list)
	f=open("pci_list.h",'w',encoding='utf-8')
	# Generate Header
	f.write("// Generated PCI-ID List header file\n\n")
	f.write("#define KnownPciVendors\t\t\t{}\n".format(len(vendor_list)))
	f.write("#define KnownPciDevices\t\t\t{}\n".format(len(device_list)))
	f.write("#define KnownPciSubsystems\t\t{}\n".format(len(subsys_list)))
	f.write("#define KnownPciClasses\t\t\t{}\n".format(len(class_list)))
	f.write("#define KnownPciSubclasses\t\t{}\n".format(len(subclass_list)))
	f.write("#define KnownPciProgifs\t\t\t{}\n\n".format(len(progif_list)))
	f.write("#define KnownPciListVersion\t\t\"{}\"\n".format(r["version"]))
	f.write("#define KnownPciListDate\t\t\"{}\"\n\n".format(r["date"]))
	# Generate Vendor IDs
	f.write("UINT16 KnownPciVendorIds[KnownPciVendors]=\n{\n")
	lf_counter=0
	for v in vendor_list:
		if lf_counter==0:
			f.write('\t')
		f.write("0x{:04X},".format(v.id))
		lf_counter+=1
		if lf_counter==16:
			lf_counter=0
			f.write('\n')
	f.write("\n};\n\n")
	# Generate Vendor Names
	f.write("char* KnownPciVendorNames[KnownPciVendors]=\n{\n")
	for v in vendor_list:
		f.write("\t\"{}\",\n".format(v.name.replace('"','\\"')))
	f.write("};\n\n")
	# Generate Device IDs
	f.write("UINT32 KnownPciDeviceIds[KnownPciDevices]=\n{\n")
	lf_counter=0
	for d in device_list:
		v=d.parent
		device_id=(v.id<<16)|d.id
		if lf_counter==0:
			f.write('\t')
		f.write("0x{:08X},".format(device_id))
		lf_counter+=1
		if lf_counter==8:
			lf_counter=0
			f.write('\n')
	f.write("\n};\n\n")
	# Generate Device Names
	f.write("char* KnownPciDeviceNames[KnownPciDevices]=\n{\n")
	for d in device_list:
		f.write("\t\"{}\",\n".format(d.name.replace('"','\\"')))
	f.write("};\n\n")
	# Generate Subsystem IDs
	f.write("UINT64 KnownPciSubsystemIds[KnownPciSubsystems]=\n{\n")
	lf_counter=0
	for s in subsys_list:
		d=s.parent
		v=d.parent
		subsys_id=(v.id<<48)|(d.id<<32)|(s.vendor_id<<16)|s.device_id
		if lf_counter==0:
			f.write('\t')
		f.write("0x{:016X},".format(subsys_id))
		lf_counter+=1
		if lf_counter==4:
			lf_counter=0
			f.write('\n')
	f.write("\n};\n\n")
	# Generate Subsystem Names
	f.write("char* KnownPciSubsystemNames[KnownPciSubsystems]=\n{\n")
	for s in subsys_list:
		f.write("\t\"{}\",\n".format(s.name.replace('"','\\"')))
	f.write("};\n\n")
	# Generate Class Codes
	f.write("UINT8 KnownPciClassIds[KnownPciClasses]=\n{\n")
	lf_counter=0
	for c in class_list:
		if lf_counter==0:
			f.write('\t')
		f.write("0x{:02X},".format(c.id))
		lf_counter+=1
		if lf_counter==32:
			lf_counter=0
			f.write('\n')
	f.write("\n};\n\n")
	# Generate Class Names
	f.write("char* KnownPciClassNames[KnownPciClasses]=\n{\n")
	for c in class_list:
		f.write("\t\"{}\",\n".format(c.name.replace('"','\\"')))
	f.write("};\n\n")
	# Generate Subclass Codes
	f.write("UINT16 KnownPciSubclassIds[KnownPciSubclasses]=\n{\n")
	lf_counter=0
	for s in subclass_list:
		c=s.parent
		subclass_id=(c.id<<8)|s.id
		if lf_counter==0:
			f.write('\t')
		f.write("0x{:04X},".format(subclass_id))
		lf_counter+=1
		if lf_counter==16:
			lf_counter=0
			f.write('\n')
	f.write("\n};\n\n")
	# Generate Subclass Names
	f.write("char* KnownPciSubclassNames[KnownPciSubclasses]=\n{\n")
	for s in subclass_list:
		f.write("\t\"{}\",\n".format(s.name.replace('"','\\"')))
	f.write("};\n\n")
	# Generate Programming-Interface IDs
	f.write("UINT32 KnownPciProgifIds[KnownPciProgifs]=\n{\n")
	lf_counter=0
	for p in progif_list:
		s=p.parent
		c=s.parent
		progif_id=(c.id<<16)|(s.id<<8)|p.id
		if lf_counter==0:
			f.write('\t')
		f.write("0x{:06X},".format(progif_id))
		lf_counter+=1
		if lf_counter==12:
			lf_counter=0
			f.write('\n')
	f.write("\n};\n\n")
	# Generate Programming-Interface Names
	f.write("char* KnownPciProgifNames[KnownPciProgifs]=\n{\n")
	for p in progif_list:
		f.write("\t\"{}\",\n".format(p.name.replace('"','\\"')))
	f.write("};\n\n")
	# Finished
	f.close()
	# Generate Rust Module
	f=open(os.path.join("src","pcilist.rs"),'w',encoding='utf-8')
	f.write("// Generated PCI-ID List header file\n\n")
	f.write("pub const KNOWN_PCI_LIST_VERSION:&'static str=\"{}\";\n".format(r["version"]))
	f.write("pub const KNOWN_PCI_LIST_DATE:&'static str=\"{}\";\n\n".format(r["date"]))
	# Generate Vendor List
	f.write("pub static KNOWN_PCI_VENDORS:[(u16,&'static str);{}]=\n[\n".format(len(vendor_list)))
	for v in vendor_list:
		f.write("\t(0x{:04X},\"{}\"),\n".format(v.id,v.name.replace('"','\\"')))
	f.write("\n];\n\n")
	# Generate Device List
	f.write("pub static KNOWN_PCI_DEVICES:[(u32,&'static str);{}]=\n[\n".format(len(device_list)))
	for d in device_list:
		v=d.parent
		device_id=(v.id<<16)|d.id
		f.write("\t(0x{:08X},\"{}\"),\n".format(device_id,d.name.replace('"','\\"')))
	f.write("];\n\n")
	# Generate Subsystem Names
	f.write("pub static KNOWN_PCI_SUBSYSTEMS:[(u64,&'static str);{}]=\n[\n".format(len(subsys_list)))
	for s in subsys_list:
		d=s.parent
		v=d.parent
		subsys_id=(v.id<<48)|(d.id<<32)|(s.vendor_id<<16)|s.device_id
		f.write("\t(0x{:016X},\"{}\"),\n".format(subsys_id,s.name.replace('"','\\"')))
	f.write("];\n\n")
	# Generate Classes
	f.write("pub static KNOWN_PCI_CLASSES:[(u8,&'static str);{}]=\n[\n".format(len(class_list)))
	for c in class_list:
		f.write("\t(0x{:02X},\"{}\"),\n".format(c.id,c.name.replace('"','\\"')))
	f.write("];\n\n")
	# Generate Subclasses
	f.write("pub static KNOWN_PCI_SUBCLASSES:[(u16,&'static str);{}]=\n[\n".format(len(subclass_list)))
	for s in subclass_list:
		c=s.parent
		subclass_id=(c.id<<8)|s.id
		f.write("\t(0x{:04X},\"{}\"),\n".format(subclass_id,s.name.replace('"','\\"')))
	f.write("];\n\n")
	# Generate Programming-Interfaces
	f.write("pub static KNOWN_PCI_PROGIFS:[(u32,&'static str);{}]=\n[\n".format(len(progif_list)))
	for p in progif_list:
		s=p.parent
		c=s.parent
		progif_id=(c.id<<16)|(s.id<<8)|p.id
		f.write("\t(0x{:06X},\"{}\"),\n".format(progif_id,p.name.replace('"','\\"')))
	f.write("];")
	f.close()

if __name__=="__main__":
	t1=time.time()
	if sys.argv.count("--refresh") or not os.path.exists("pci.ids"):
		r=requests.request('GET',"https://pci-ids.ucw.cz/v2.2/pci.ids")
		lines=r.text.split('\n')
		f=open("pci.ids",'w',encoding='utf-8')
		f.write(r.text)
		f.close()
	else:
		f=open("pci.ids",'r',encoding='utf-8')
		lines=f.read().split('\n')
		f.close()
	main(lines)
	t2=time.time()
	print("Time Elapsed due to Generator: {} seconds".format(t2-t1))