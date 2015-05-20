#include "ClContext.h"

// Windows
#include <windows.h>
#include <wingdi.h>

// STD
#include <algorithm>
#include <iostream>
#include <fstream>

// RPE
#include "helper.h"

ClContext* ClContext::m_singleton = 0;
ClContextDestructor ClContext::m_singleton_destructor;

void ClContext::init() {
  
  cl_int error = CL_SUCCESS;
  cl_uint num_platforms;
  clGetPlatformIDs(0, nullptr, &num_platforms);
  if (num_platforms == 0){
    std::cout << "Cannot find any platform.\n";
    return;
  }
  platform.resize(num_platforms);

  platform_device.resize(num_platforms);
  platform_device_features.resize(num_platforms);
  devices.clear();

  error = clGetPlatformIDs(num_platforms, platform.data(), nullptr);
  checkError(error);
  
  std::cout 
    << "=================================================================\n"
    << "Detecting the OPENCL Devices: \n"
    << "=================================================================\n";
  int ctx_idx = 0;
  for (cl_uint i = 0; i < num_platforms; i++){
     
    std::string platform_name;
    size_t platform_name_len;
    clGetPlatformInfo(platform[i], CL_PLATFORM_NAME, 0, nullptr, &platform_name_len);
    platform_name.resize(platform_name_len);
    clGetPlatformInfo(platform[i], CL_PLATFORM_NAME, platform_name_len, const_cast<char*>(platform_name.data()), nullptr);
    std::cout << "[" << i << "]\t" << platform_name << std::endl;
    
    cl_uint num_devices;
    error = clGetDeviceIDs(platform[i], CL_DEVICE_TYPE_GPU, 0, nullptr, &num_devices);
    platform_device[i].resize(num_devices);
    platform_device_features[i].resize(num_devices);
    error = clGetDeviceIDs(platform[i], CL_DEVICE_TYPE_GPU, num_devices, platform_device[i].data(), nullptr);

    // Context Properties for devices supporting opengl-opencl interoperatability.
    cl_context_properties custom_props[] = {
      // set platform
      CL_CONTEXT_PLATFORM, (cl_context_properties)platform[i],

      // set platform_device context
      CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),

      // set current context
      CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),

      0
    };

    for (cl_uint d = 0; d < num_devices; d++){

      platform_device_features[i][d] = getDeviceFeatures(platform_device[i][d]);
      ClDevice device;
      std::cout << "\t\t[" << d << "]\t" << platform_device_features[i][d].device_name << std::endl;
      std::cout << "\t\t\t" << "Max Const. Buf. Data = " << platform_device_features[i][d].max_constant_buffer_size << std::endl;
      if (platform_device_features[i][d].has_cl_khr_gl_sharing)
        std::cout << "\t\t\tCLGL Interoperation extension is supported.\n";
      else
        std::cout << "\t\t\tWarning: CLGL Interoperation extension is not supported.\n";

      device.id = platform_device[i][d];
      device.features = platform_device_features[i][d];
      device.features.platform_name = platform_name;
      device.ctx_idx = ctx_idx++;

      if (device.features.has_cl_khr_gl_sharing) {
        device.ctx = clCreateContext(custom_props, 1, &device.id, nullptr, nullptr, &error);    checkError(error);
        device.cmd_queue = clCreateCommandQueue(device.ctx, device.id, 0, &error);              checkError(error);
      }
      else {
        device.ctx = clCreateContext(0, 1, &device.id, nullptr, nullptr, &error);               checkError(error);
        device.cmd_queue = clCreateCommandQueue(device.ctx, device.id, 0, &error);              checkError(error);
      }

      // storing the device in the devices list
      devices.push_back(device);

    }

    std::cout << "***************************************************************\n";
  }

  std::cout << "=================================================================\n"
            << "=================================================================\n";
}

ClDeviceFeatures ClContext::getDeviceFeatures(cl_device_id device_id) {
  
  cl_int error = 0;
  ClDeviceFeatures features;

  // Query for platform_device name
  size_t device_name_len;
  clGetDeviceInfo(device_id, CL_DEVICE_NAME, 0, nullptr, &device_name_len);
  features.device_name.resize(device_name_len);
  clGetDeviceInfo(device_id, CL_DEVICE_NAME, device_name_len, const_cast<char*>(features.device_name.data()), nullptr);

  // remove tabs from device names
  std::remove(features.device_name.begin(), features.device_name.end(), ' ');

  cl_ulong max_constant_buffer_size = 0;
  error = clGetDeviceInfo(device_id, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(cl_ulong), &max_constant_buffer_size, nullptr);
  checkError(error);

  // Query for platform_device maximum constant buffer size
  error = clGetDeviceInfo(device_id, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(cl_ulong), &features.max_constant_buffer_size, nullptr);
  checkError(error);
  
  // Query for all platform_device extensions
  std::string device_extension;
  size_t device_extension_len;
  error = clGetDeviceInfo(device_id, CL_DEVICE_EXTENSIONS, 0, nullptr, &device_extension_len);                              
  checkError(error);
  device_extension.resize(device_extension_len);
  error = clGetDeviceInfo(device_id, CL_DEVICE_EXTENSIONS, device_extension_len, (void*)device_extension.data(), nullptr);  
  checkError(error);
  
  // check if the platform_device supports "cl_khr_gl_sharing" or not.
  if (findString(device_extension, "cl_khr_gl_sharing")){
    features.has_cl_khr_gl_sharing = true;
  }
  else {
    features.has_cl_khr_gl_sharing = false;
  }

  return features;
}

cl_kernel ClContext::createKernel(const std::string& file_name, const std::string& kernel_name, const ClDevice& device) {
  
  cl_int error = 0;
  std::ifstream prog_file(file_name);
  if (!prog_file){
    std::cout << "Cannot open " << file_name << std::endl;
  }
  std::string source(std::istreambuf_iterator<char>(prog_file), (std::istreambuf_iterator<char>()));
  const char* source_cstr = source.c_str();
  cl_program prog = clCreateProgramWithSource(device.ctx, 1, &source_cstr, NULL, &error);  checkError(error);
  error = clBuildProgram(prog, 0, NULL, "-cl-fast-relaxed-math -DINTEG_METHOD_EULER", NULL, NULL);                              checkError(error);
//  if (error != CL_SUCCESS){
        std::ofstream build_log_file(file_name + "_build_" + device.features.device_name + ".log");
    std::string build_log;
    size_t build_log_len = 0;

    clGetProgramBuildInfo(prog, device.id, CL_PROGRAM_BUILD_LOG, 0, nullptr, &build_log_len);
    build_log.resize(build_log_len);
    clGetProgramBuildInfo(prog, device.id, CL_PROGRAM_BUILD_LOG, build_log_len, const_cast<char*>(build_log.data()), nullptr);

    std::cout << "File name: " << file_name << ", Build Log:\n" << build_log << std::endl;

    if (build_log_file)
      build_log_file << build_log << std::endl;

    // STOP The application, if the kernel does not compile.
    // Due to crashes on nvidia gpu.
    if (error != CL_SUCCESS) exit(0);

  cl_kernel kernel = clCreateKernel(prog, kernel_name.c_str(), &error);                 checkError(error);
  
  return kernel;
}

cl_kernel ClContext::createKernel(const std::string& file_name, const std::string& definitions, const std::string& kernel_name, const ClDevice& device) {

  std::cout << "========================================================\n";
  std::cout << "Compiling: " << file_name << std::endl;
  std::cout << "Device: " << device.features.device_name << std::endl;
  std::cout << "========================================================\n";

  cl_int error = 0;
  std::ifstream prog_file(file_name);
  if (!prog_file){
    std::cout << "Cannot open " << file_name << std::endl;
  }
  std::string source(std::istreambuf_iterator<char>(prog_file), (std::istreambuf_iterator<char>()));
  const char* source_cstr = source.c_str();
  cl_program prog = clCreateProgramWithSource(device.ctx, 1, &source_cstr, NULL, &error);  checkError(error);
  error = clBuildProgram(prog, 0, NULL, definitions.data(), NULL, NULL);                            checkError(error);
  cl_kernel kernel = clCreateKernel(prog, kernel_name.c_str(), &error);                             checkError(error);
  //if (error != CL_SUCCESS){
    std::ofstream build_log_file(file_name + "_build_" + device.features.device_name + ".log");
    std::string build_log;
    size_t build_log_len = 0;

    clGetProgramBuildInfo(prog, device.id, CL_PROGRAM_BUILD_LOG, 0, nullptr, &build_log_len);
    build_log.resize(build_log_len);
    clGetProgramBuildInfo(prog, device.id, CL_PROGRAM_BUILD_LOG, build_log_len, const_cast<char*>(build_log.data()), nullptr);

    std::cout << "Build Log:\n" << build_log << std::endl;

    if (build_log_file)
      build_log_file << build_log << std::endl;
 // }

    // STOP The application, if the kernel does not compile.
    // Due to crashes on nvidia gpu.
    if (error != CL_SUCCESS) exit(0);

    std::cout << "========================================================\n\n\n";

  return kernel;
}

ClContext* ClContext::getSingletonPtr(){
  if (!m_singleton){
    m_singleton = new ClContext();
    m_singleton_destructor.setSingleton(m_singleton);
  }
  return m_singleton;
}

ClContext::ClContext() {
}

ClContext::~ClContext() {

}

void ClContext::checkError(cl_int error) {
  if (error != CL_SUCCESS) {
    std::cerr << "OpenCL call failed with error " << error << std::endl;
    std::cerr << "\t";
   switch (error){

      case CL_DEVICE_NOT_FOUND:
        std::cout << "CL_DEVICE_NOT_FOUND.\n";
        break;

      case CL_DEVICE_NOT_AVAILABLE:
        std::cout << "CL_DEVICE_NOT_AVAILABLE.\n";
        break;

      case CL_COMPILER_NOT_AVAILABLE:
        std::cout << "CL_COMPILER_NOT_AVAILABLE.\n";
        break;
      case CL_MEM_OBJECT_ALLOCATION_FAILURE:
        std::cout << "CL_MEM_OBJECT_ALLOCATION_FAILURE.\n";
        break;

      case CL_OUT_OF_RESOURCES:
        std::cout << "CL_OUT_OF_RESOURCES.\n";
        break;

      case CL_OUT_OF_HOST_MEMORY:
        std::cout << "CL_OUT_OF_HOST_MEMORY.\n";
        break;

      case CL_PROFILING_INFO_NOT_AVAILABLE:
        std::cout << "CL_PROFILING_INFO_NOT_AVAILABLE.\n";
        break;
      case CL_MEM_COPY_OVERLAP:
        std::cout << "CL_MEM_COPY_OVERLAP.\n";
        break;

      case CL_IMAGE_FORMAT_MISMATCH:
        std::cout << "CL_IMAGE_FORMAT_MISMATCH.\n";
        break;
      
      case CL_IMAGE_FORMAT_NOT_SUPPORTED:
        std::cout << "CL_IMAGE_FORMAT_NOT_SUPPORTED.\n";
        break;

      case CL_BUILD_PROGRAM_FAILURE:
        std::cout << "CL_BUILD_PROGRAM_FAILURE.\n";
        break;

      case CL_MAP_FAILURE:
        std::cout << "CL_MAP_FAILURE.\n";
        break;

      case CL_INVALID_VALUE:
        std::cout << "CL_INVALID_VALUE.\n";
        break;

      case CL_INVALID_DEVICE_TYPE:
        std::cout << "CL_INVALID_DEVICE_TYPE.\n";
        break;

      case CL_INVALID_PLATFORM:
        std::cout << "CL_INVALID_PLATFORM.\n";
        break;

      case CL_INVALID_DEVICE:
        std::cout << "CL_INVALID_DEVICE.\n";
        break;

      case CL_INVALID_CONTEXT:
        std::cout << "CL_INVALID_CONTEXT.\n";
        break;

      case CL_INVALID_QUEUE_PROPERTIES:
        std::cout << "CL_INVALID_QUEUE_PROPERTIES.\n";
        break;

      case CL_INVALID_COMMAND_QUEUE:
        std::cout << "CL_INVALID_COMMAND_QUEUE.\n";
        break;

      case CL_INVALID_HOST_PTR:
        std::cout << "CL_INVALID_HOST_PTR.\n";
        break;

      case CL_INVALID_MEM_OBJECT:
        std::cout << "CL_INVALID_MEM_OBJECT.\n";
        break;

      case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
        std::cout << "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR.\n";
        break;

      case CL_INVALID_IMAGE_SIZE:
        std::cout << "CL_INVALID_IMAGE_SIZE.\n";
        break;

      case CL_INVALID_SAMPLER:
        std::cout << "CL_INVALID_SAMPLER.\n";
        break;

      case CL_INVALID_BINARY:
        std::cout << "CL_INVALID_BINARY.\n";
        break;

      case CL_INVALID_BUILD_OPTIONS:
        std::cout << "CL_INVALID_BUILD_OPTIONS.\n";
        break;

      case CL_INVALID_PROGRAM:
        std::cout << "CL_INVALID_PROGRAM.\n";
        break;

      case CL_INVALID_PROGRAM_EXECUTABLE:
        std::cout << "CL_INVALID_PROGRAM_EXECUTABLE.\n";
        break;

      case CL_INVALID_KERNEL_NAME:
        std::cout << "CL_INVALID_KERNEL_NAME.\n";
        break;

      case CL_INVALID_KERNEL_DEFINITION:
        std::cout << "CL_INVALID_KERNEL_DEFINITION.\n";
        break;

      case CL_INVALID_KERNEL:
        std::cout << "CL_INVALID_KERNEL.\n";
        break;

      case CL_INVALID_ARG_INDEX:
        std::cout << "CL_INVALID_ARG_INDEX.\n";
        break;

      case CL_INVALID_ARG_VALUE:
        std::cout << "CL_INVALID_ARG_VALUE.\n";
        break;

      case CL_INVALID_ARG_SIZE:
        std::cout << "CL_INVALID_ARG_SIZE.\n";
        break;

      case CL_INVALID_KERNEL_ARGS:
        std::cout << "CL_INVALID_KERNEL_ARGS.\n";
        break;

      case CL_INVALID_WORK_DIMENSION:
        std::cout << "CL_INVALID_WORK_DIMENSION.\n";
        break;

      case CL_INVALID_WORK_GROUP_SIZE:
        std::cout << "CL_INVALID_WORK_GROUP_SIZE.\n";
        break;

      case CL_INVALID_WORK_ITEM_SIZE:
        std::cout << "CL_INVALID_WORK_ITEM_SIZE.\n";
        break;

      case CL_INVALID_GLOBAL_OFFSET:
        std::cout << "CL_INVALID_GLOBAL_OFFSET.\n";
        break;

      case CL_INVALID_EVENT_WAIT_LIST:
        std::cout << "CL_INVALID_EVENT_WAIT_LIST.\n";
        break;

      case CL_INVALID_EVENT:
        std::cout << "CL_INVALID_EVENT.\n";
        break;

      case CL_INVALID_OPERATION:
        std::cout << "CL_INVALID_OPERATION.\n";
        break;

      case CL_INVALID_GL_OBJECT:
        std::cout << "CL_INVALID_GL_OBJECT.\n";
        break;

      case CL_INVALID_BUFFER_SIZE:
        std::cout << "CL_INVALID_BUFFER_SIZE.\n";
        break;

      case CL_INVALID_MIP_LEVEL:
        std::cout << "CL_INVALID_MIP_LEVEL.\n";
        break;

      case CL_INVALID_GLOBAL_WORK_SIZE:
        std::cout << "CL_INVALID_GLOBAL_WORK_SIZE.\n";
        break;
   }

    //std::exit(1);
  }
}