// Memory.cc -- Apr 14, 2009
//    by geohot
//  released under GPLv3, see http://gplv3.fsf.org/

#include "macros.h"
#include "Memory.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <fstream>

#include <cstdlib>

using namespace eda;

File& Memory::operator[](Address address) {
  //info << "got access at " << address << ": ";

  std::map<int, std::vector<File> >::iterator addr;
  addr = mChunks.upper_bound(address);
  if (addr == mChunks.begin())
    return mMemoryUndefined[address];
  addr--;
  if (address < (addr->first + addr->second.size())) {
    //exists in chunk
    //info << "found in chunk" << std::endl;
    return addr->second[address - (addr->first)];
  } else {
    //return the random
    //info << "not found" << std::endl;
    return mMemoryUndefined[address];
  }
}

std::string Memory::getName(Address address) {
  std::map<Data, std::string>::iterator walk;
  walk = mNames.find(address);
  if (walk != mNames.end())
    return walk->second;
  else {
    std::stringstream ss;
    ss << "loc_" << std::hex << address;
    return ss.str();
  }
}

bool Memory::isNameSet(Address address) {
  return (mNames.find(address) != mNames.end());
}

void Memory::setName(Address address, std::string name) {
  std::map<Data, std::string>::iterator nameiter = mNames.find(address);
  if (nameiter != mNames.end())
    mReverseNames.erase(nameiter->second);
  mNames.erase(address);
  mNames.insert(std::make_pair(address, name)); //does insert overwrite?
  mReverseNames.insert(std::make_pair(name, address));
}

bool Memory::lookupName(std::string name, Address *address) {
  std::map<std::string, Data>::iterator nameiter = mReverseNames.find(name);
  if (nameiter == mReverseNames.end())
    return false;
  *address = nameiter->second;
  return true;
}

bool Memory::exists(Address address) {
  //info << "Checking existence of " << address << std::endl;
  std::map<int, std::vector<File> >::iterator addr;
  addr = mChunks.upper_bound(address);
  if (addr == mChunks.begin())
    return false;
  addr--;
  if (address < (addr->first + addr->second.size()))
    return true;
  else if (mMemoryUndefined.find(address) != mMemoryUndefined.end())
    return true;
  else
    return false;
}

std::vector<File>* Memory::getChunk(Address address) {
  std::map<int, std::vector<File> >::iterator addr;
  addr = mChunks.find(address);
  if (addr != mChunks.end())
    return &(addr->second);
  else
    return 0; //not found
}

//allocate a chunk of memory in mMemory
//we really should migrate this space from undefined
bool Memory::allocate(Address address, int len) {
  info << "allocating region " << address << "-" << len << std::endl;
  if (checkRegion(address, len))
    return false;
  std::vector<File> *region = new std::vector<File>;
  region->resize(len);
  mChunks.insert(std::make_pair(address, *region));
  return true;
}

bool Memory::importIDC(const char *filename) {
  char line[256];
  std::ifstream fin(filename);
  while (fin.good()) {
    fin.getline(line, 256);
    if (memcmp(line, "\tMakeName\t", 10) == 0) {
      char *addr = (strchr(line, 'X') + 1);
      char *name = (strchr(line, '"') + 1);
      (*strchr(addr, ',')) = '\0';
      (*strchr(name, '"')) = '\0';
      Data daddr = strtoul(addr, NULL, 16);
      info << "set name of " << daddr << " to " << name << std::endl;
      setName(daddr, name);
    }
  }
  return true;
}

//load a file into memory
//is it bad i only know how to do this in c?
bool Memory::loadFile(const char *filename, Address address) {
  FILE *f = fopen(filename, "rb");
  if (f == 0) {
    debug << "file " << filename << " not found" << std::endl;
    return false;
  }
  int len = fileSize(f);
  if (!allocate(address, len)) {
    debug << "allocate failed" << std::endl;
    return false;
  }
  Data buffer[0x200];
  int rlen, cl = 0;
  std::vector<File> *d = getChunk(address);
  if (d == 0) {
    debug << "finding chunk failed" << std::endl;
    return false;
  }
  //no byte stop files
  while ((rlen = fread(buffer, 4, 0x200, f)) > 0) {
    for (int t = 0; t < rlen; t++) {
      (*d)[(t * 4) + cl].set(0, buffer[t]); //loads are all on changelist 0 for now
    }
    cl += rlen * 4;
  }

  return true;
}

//see if a region is empty, ignore undefineds
//false is no region found
bool Memory::checkRegion(Address address, int len) {
  std::map<int, std::vector<File> >::iterator addr;
  //get first start segment before (address+len)
  addr = mChunks.upper_bound(address + len);
  if (addr == mChunks.begin())
    return false;
  addr--;
  //if ending in region or past region
  return (address < (addr->first + addr->second.size()));
}

void Memory::consoleDump(Address address, int len, int changelistNumber) {
  for (int t = 0; t < len; t += 4) {
    if (!exists(address + t)) {
      std::cout << "Memory doesn't exist";
      break;
    }
    if (t != 0 && ((t % 0x10) == 0))
      printf("\n");
    if ((t % 0x10) == 0) {
      std::cout << std::hex << address + t << ": ";
    }
    std::cout << std::hex << std::setfill('0') << std::setw(8)
        << (*this)[address + t][changelistNumber] << " ";
  }
  std::cout << std::endl;
}

void Memory::debugPrint() {
  std::map<int, std::vector<File> >::iterator addr = mChunks.begin();
  while (addr != mChunks.end()) {
    std::cout << std::hex << "Region: 0x" << addr->first << " - 0x"
        << addr->first + addr->second.size() << std::endl;
    ++addr;
  }
  std::cout << "plus " << std::dec << mMemoryUndefined.size() << " undefineds"
      << std::endl;
}

int Memory::fileSize(FILE *f) {
  int pos, end;
  pos = ftell(f);
  fseek(f, 0, SEEK_END);
  end = ftell(f);
  fseek(f, pos, SEEK_SET);
  return end;
}

//deal with functions in Memory
Function *Memory::addFunction(int start) {
  if (!isNameSet(start)) {
    std::stringstream name;
    name << std::hex << "sub_" << start;
    setName(start, name.str()); //name the function in the memory space
  }
  return &(mFunctionStore.insert(std::make_pair(start, Function(start))).first->second);
}

Function* Memory::inFunction(Address addr) {
  //return function with this instruction in it
  //doesn't work yet
  FunctionIterator a = mFunctionStore.find(addr);
  if (a != mFunctionStore.end())
    return &(a->second);
  else
    return 0;
}

