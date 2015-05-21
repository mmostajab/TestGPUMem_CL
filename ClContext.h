#ifndef  __CL_CONTEXT_H__
#define __CL_CONTEXT_H__

// STD
#include <vector>
#include <string>
#define nullptr 0

// CL
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#include <CL/cl.h>
#include <CL/cl_gl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>

class ClContextDestructor;

//#define SEPRATE_CPU_GPU

struct ClDeviceFeatures {
  std::string device_name;
  std::string platform_name;
  cl_ulong    max_constant_buffer_size;
  bool        has_cl_khr_gl_sharing;
};

struct ClDevice {
  cl_device_id      id;
  ClDeviceFeatures  features;
  cl_context        ctx;
  cl_command_queue  cmd_queue;
  int               ctx_idx;

  int               active;
};

class ClContext {
  friend class ClContextDestructor;
public:

  void init(Display** display, Window* win, GLXContext* ctx);
  cl_kernel createKernel(const std::string& file_name, const std::string& kernel_name, const ClDevice& device);
  cl_kernel createKernel(const std::string& file_name, const std::string& definitions, const std::string& kernel_name, const ClDevice& device);

  static ClContext* getSingletonPtr();
  void checkError(cl_int error);
  ClDeviceFeatures getDeviceFeatures(cl_device_id dev_id);

  //std::vector<cl_context>                     ctx;
  std::vector<cl_platform_id>                 platform;
#ifdef SEPRATE_CPU_GPU
  std::vector<std::vector<cl_device_id>>      cpu_devices;
  std::vector<std::vector<cl_command_queue>>  cpu_devices_queue;
  std::vector<std::vector<cl_device_id>>      gpu_devices;
  std::vector<std::vector<cl_command_queue>>  gpu_devices_queue;
#else //  SEPRATE_CPU_GPU
  std::vector< std::vector<cl_device_id> >      platform_device;
  std::vector< std::vector<ClDeviceFeatures> >  platform_device_features;

  std::vector<ClDevice> devices;
#endif // SEPRATE_CPU_GPU

private:
  ClContext();
  ~ClContext();

  static ClContext* m_singleton;
  static ClContextDestructor m_singleton_destructor;
};

class ClContextDestructor {
public:
  ClContextDestructor() {
    m_singleton = 0;
  }

  void setSingleton(ClContext* singleton) {
    m_singleton = singleton;
  }

  ~ClContextDestructor() {
    if (m_singleton)
      delete m_singleton;
  }

private:
  ClContext* m_singleton;
};

#endif
