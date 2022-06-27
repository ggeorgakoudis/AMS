#include <umpire/Umpire.hpp>
#include "utilities.hpp"

namespace AMS {
namespace utilities {
AMSDevice defaultDloc = AMSDevice::CPU;

void setDefaultDataAllocator(AMSDevice location) { defaultDloc = location; }

AMSDevice getDefaultDataAllocator() { return defaultDloc; }

const char *getDeviceAllocatorName() { return "mmp-device-quickpool"; }

const char *getHostAllocatorName() { return "mmp-host-quickpool"; }

void *allocate(size_t bytes){
  static auto& rm = umpire::ResourceManager::getInstance();
  if ( defaultDloc == CPU ){
        static auto cpuAllocator = rm.getAllocator(getHostAllocatorName());
        return cpuAllocator.allocate(bytes);
  }
  else if ( defaultDloc == DEVICE ){
        static auto deviceAllocator = rm.getAllocator(getDeviceAllocatorName());
        return deviceAllocator.allocate(bytes);
  }
  return nullptr;
}

void *allocate(size_t bytes, AMSDevice dev){
  static auto& rm = umpire::ResourceManager::getInstance();
  if ( dev == CPU ){
        static auto cpuAllocator = rm.getAllocator(getHostAllocatorName());
        return cpuAllocator.allocate(bytes);
  }
  else if ( dev == DEVICE ){
        static auto deviceAllocator = rm.getAllocator(getDeviceAllocatorName());
        return deviceAllocator.allocate(bytes);
  }
  return nullptr;
}

const char *getDefaultAllocatorName() {
    switch (defaultDloc) {
    case AMSDevice::CPU:
        return getHostAllocatorName();
    case AMSDevice::DEVICE:
        return getDeviceAllocatorName();
    }
    return "unknown";
}

} // namespace utilities
} // namespace AMS
