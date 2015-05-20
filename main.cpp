#include <iostream>
#include <time.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "ClContext.h"

GLFWwindow* win;

struct my_vertex {
  cl_float4 pos;
  cl_float4 d;
  cl_float4 n;
};

void initGlfw(){
  // initialize the glfw
  if (!glfwInit())
    exit(EXIT_FAILURE);

  // create the window
  win = glfwCreateWindow(600, 400, "TEST GPU Buffer", NULL, NULL);
  if (!win)
  {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  // Make the current window as the current gl context
  glfwMakeContextCurrent(win);
}

void main() {

  cl_int error;
  int dev_idx = 0;

  ClContext* cl = ClContext::getSingletonPtr();
  
  initGlfw();
  cl->init();
  if (glewInit() != GLEW_OK){
    std::cout << "Cannot init Glew\n";
    exit(0);
  }

  std::cout << "\n\n\nChoose The Device: \t";
  
  /*do{
    std::cin >> dev_idx;
  } while (dev_idx >= cl->devices.size());
  std::cout << "Device: " << cl->devices[dev_idx].features.device_name << std::endl;
  bool use_gpu_mem = false;

  std::cout << "Use GL Buffers? \t";
  char ch; std::cin >> ch;

  if (ch == 'y' || ch == 'Y'){
    std::cout << "use_gpu_mem = true;\n";
    use_gpu_mem = true;
  }
  else {
    std::cout << "use_gpu_mem = false;\n";
    use_gpu_mem = false;
  }*/
  dev_idx = 0;
  std::cout << "Device: " << cl->devices[dev_idx].features.device_name << std::endl;
  bool use_gpu_mem = false;
  
  cl_kernel mykernel = cl->createKernel("testKernel.cl", "myKernel", cl->devices[dev_idx]);

  cl_int mem_size = 4 * 1024;
  std::vector<cl_float4> host_a(mem_size), /*host_b(mem_size),*/ host_c(mem_size);

  for (size_t i = 0; i < mem_size; i++){
    host_a[i].s[0] = rand() % 1000 / 1000.0f;
    host_a[i].s[1] = rand() % 1000 / 1000.0f;
    host_a[i].s[2] = rand() % 1000 / 1000.0f;
    host_a[i].s[3] = 1.0f;
   // host_b[i] = rand() % 1000;
  }

//#define USE_GPU_MEM
  cl_mem device_a, /*device_b,*/ device_c;
  GLuint gl_buffer_a, gl_buffer_b, gl_buffer_c = 0;

  if (!use_gpu_mem){
    device_a = clCreateBuffer(cl->devices[dev_idx].ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, mem_size * sizeof(cl_float4), host_a.data(), &error); cl->checkError(error);
    //device_b = clCreateBuffer(cl->devices[dev_idx].ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, mem_size * sizeof(cl_int), host_b.data(), &error); cl->checkError(error);
    device_c = clCreateBuffer(cl->devices[dev_idx].ctx, CL_MEM_WRITE_ONLY, 1000 * mem_size * sizeof(my_vertex), nullptr, &error); cl->checkError(error);
  }
  else {
    
    device_a = clCreateBuffer(cl->devices[dev_idx].ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, mem_size * sizeof(cl_float4), host_a.data(), &error); cl->checkError(error);

    /*glGenBuffers(1, &gl_buffer_a);
    glBindBuffer(GL_ARRAY_BUFFER, gl_buffer_a);
    glBufferData(GL_ARRAY_BUFFER, mem_size*sizeof(cl_float4), host_a.data(), GL_STATIC_DRAW);*/
    /*glGenBuffers(1, &gl_buffer_b);
    glBindBuffer(GL_ARRAY_BUFFER, gl_buffer_b);
    glBufferData(GL_ARRAY_BUFFER, mem_size*sizeof(cl_int), host_b.data(), GL_STATIC_DRAW);*/
    glGenBuffers(1, &gl_buffer_c);
    glBindBuffer(GL_ARRAY_BUFFER, gl_buffer_c);
    glBufferData(GL_ARRAY_BUFFER, 1000 * mem_size*sizeof(my_vertex), nullptr, GL_STATIC_DRAW);

    //device_a = clCreateFromGLBuffer(cl->devices[dev_idx].ctx, CL_MEM_READ_ONLY, gl_buffer_a, &error);               cl->checkError(error);
    //device_b = clCreateFromGLBuffer(cl->devices[dev_idx].ctx, CL_MEM_READ_ONLY, gl_buffer_b, &error);               cl->checkError(error);
    device_c = clCreateFromGLBuffer(cl->devices[dev_idx].ctx, CL_MEM_WRITE_ONLY, gl_buffer_c, &error);              cl->checkError(error);

    
  }

  while (true){

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindBuffer(GL_ARRAY_BUFFER, gl_buffer_c);
    glDrawArrays(GL_TRIANGLES, 0, 1000 * mem_size);

    glfwSwapBuffers(win);


    float avg_time = 0.0f;
    const int n_tests = 100;
    for (size_t i = 0; i < n_tests; i++){

      if (use_gpu_mem){
        glFinish();
        //error = clEnqueueAcquireGLObjects(cl->devices[dev_idx].cmd_queue, 1, &device_a, 0, nullptr, nullptr); cl->checkError(error);
        //error = clEnqueueAcquireGLObjects(cl->devices[dev_idx].cmd_queue, 1, &device_b, 0, nullptr, nullptr); cl->checkError(error);
        error = clEnqueueAcquireGLObjects(cl->devices[dev_idx].cmd_queue, 1, &device_c, 0, nullptr, nullptr); cl->checkError(error);
      }

      clSetKernelArg(mykernel, 0, sizeof(cl_mem), &device_a);
      clSetKernelArg(mykernel, 1, sizeof(cl_mem), &device_c);
      clSetKernelArg(mykernel, 2, sizeof(cl_int), &mem_size);

      size_t nKernels = mem_size;
      size_t local_ws = 32;
      size_t global_ws = (nKernels + local_ws - 1) / local_ws * local_ws;

      clock_t beg_time = clock();
      //std::cout << "Beg Time: " << beg_time << std::endl;
      error = clEnqueueNDRangeKernel(cl->devices[dev_idx].cmd_queue, mykernel, 1, nullptr, &global_ws, &local_ws, 0, nullptr, nullptr); cl->checkError(error);
      clFinish(cl->devices[dev_idx].cmd_queue);
      clock_t end_time = clock();

      avg_time += static_cast<float>((end_time - beg_time)) / CLOCKS_PER_SEC;
      //std::cout << "End Time: " << end_time << std::endl;
    }
    avg_time /= n_tests;

    std::cout << "Execution Time (Avg.)  = " << 1.0f / avg_time << std::endl;
  }
}