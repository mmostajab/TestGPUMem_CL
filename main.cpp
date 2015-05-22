#include <iostream>
#include <time.h>
#include <GL/glew.h>

#include "ClContext.h"

#ifdef _WIN32
  #include <GLFW/glfw3.h>
#elif _LINUX
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <unistd.h>
  #include <X11/Xlib.h>
  #include <X11/Xutil.h>
  #include <GL/gl.h>
  #include <GL/glx.h>
#endif

// BOOST
#include <boost/thread.hpp>

GLFWwindow* win[10];

void initGlfw(){
  // initialize the glfw
  if (!glfwInit())
    exit(EXIT_FAILURE);

  int num_monitors = -1;
  GLFWmonitor** monitors = glfwGetMonitors(&num_monitors);

  for(int i = 0;  i < num_monitors; i++){
      // create the window
      win[i] = glfwCreateWindow(600, 400, "TEST GPU Buffer", NULL, NULL);
      if (!win[i])
      {
        glfwTerminate();
        exit(EXIT_FAILURE);
      }

      // Make the current window as the current gl context
      glfwMakeContextCurrent(win[i]);
  }

}

#ifdef _LINUX
#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

// Helper to check for extension string presence.  Adapted from:
//   http://www.opengl.org/resources/features/OGLextensions/
static bool isExtensionSupported(const char *extList, const char *extension)
{
  const char *start;
  const char *where, *terminator;

  /* Extension names should not have spaces. */
  where = strchr(extension, ' ');
  if (where || *extension == '\0')
    return false;

  /* It takes a bit of care to be fool-proof about parsing the
     OpenGL extensions string. Don't be fooled by sub-strings,
     etc. */
  for (start=extList;;) {
    where = strstr(start, extension);

    if (!where)
      break;

    terminator = where + strlen(extension);

    if ( where == start || *(where - 1) == ' ' )
      if ( *terminator == ' ' || *terminator == '\0' )
        return true;

    start = terminator;
  }

  return false;
}

static bool ctxErrorOccurred = false;
static int ctxErrorHandler( Display *dpy, XErrorEvent *ev )
{
    ctxErrorOccurred = true;
    return 0;
}
void initGlx(const char* display_str, Display* display, Window& win, GLXContext& ctx){
    display = XOpenDisplay(display_str);

    if (!display)
    {
      printf("Failed to open X display\n");
      exit(1);
    }

    // Get a matching FB config
    static int visual_attribs[] =
      {
        GLX_X_RENDERABLE    , True,
        GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
        GLX_RENDER_TYPE     , GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
        GLX_RED_SIZE        , 8,
        GLX_GREEN_SIZE      , 8,
        GLX_BLUE_SIZE       , 8,
        GLX_ALPHA_SIZE      , 8,
        GLX_DEPTH_SIZE      , 24,
        GLX_STENCIL_SIZE    , 8,
        GLX_DOUBLEBUFFER    , True,
        //GLX_SAMPLE_BUFFERS  , 1,
        //GLX_SAMPLES         , 4,
        None
      };

    int glx_major, glx_minor;

    // FBConfigs were added in GLX version 1.3.
    if ( !glXQueryVersion( display, &glx_major, &glx_minor ) ||
         ( ( glx_major == 1 ) && ( glx_minor < 3 ) ) || ( glx_major < 1 ) )
    {
      printf("Invalid GLX version");
      exit(1);
    }

    printf( "Getting matching framebuffer configs\n" );
    int fbcount;
    GLXFBConfig* fbc = glXChooseFBConfig(display, DefaultScreen(display), visual_attribs, &fbcount);
    if (!fbc)
    {
      printf( "Failed to retrieve a framebuffer config\n" );
      exit(1);
    }
    printf( "Found %d matching FB configs.\n", fbcount );

    // Pick the FB config/visual with the most samples per pixel
    printf( "Getting XVisualInfos\n" );
    int best_fbc = -1, worst_fbc = -1, best_num_samp = -1, worst_num_samp = 999;

    int i;
    for (i=0; i<fbcount; ++i)
    {
      XVisualInfo *vi = glXGetVisualFromFBConfig( display, fbc[i] );
      if ( vi )
      {
        int samp_buf, samples;
        glXGetFBConfigAttrib( display, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf );
        glXGetFBConfigAttrib( display, fbc[i], GLX_SAMPLES       , &samples  );

        printf( "  Matching fbconfig %d, visual ID 0x%2x: SAMPLE_BUFFERS = %d,"
                " SAMPLES = %d\n",
                i, vi -> visualid, samp_buf, samples );

        if ( best_fbc < 0 || samp_buf && samples > best_num_samp )
          best_fbc = i, best_num_samp = samples;
        if ( worst_fbc < 0 || !samp_buf || samples < worst_num_samp )
          worst_fbc = i, worst_num_samp = samples;
      }
      XFree( vi );
    }

    GLXFBConfig bestFbc = fbc[ best_fbc ];

    // Be sure to free the FBConfig list allocated by glXChooseFBConfig()
    XFree( fbc );

    // Get a visual
    XVisualInfo *vi = glXGetVisualFromFBConfig( display, bestFbc );
    printf( "Chosen visual ID = 0x%x\n", vi->visualid );

    printf( "Creating colormap\n" );
    XSetWindowAttributes swa;
    Colormap cmap;
    swa.colormap = cmap = XCreateColormap( display,
                                           RootWindow( display, vi->screen ),
                                           vi->visual, AllocNone );
    swa.background_pixmap = None ;
    swa.border_pixel      = 0;
    swa.event_mask        = StructureNotifyMask;

    printf( "Creating window\n" );
    win = XCreateWindow( display, RootWindow( display, vi->screen ),
                                0, 0, 100, 100, 0, vi->depth, InputOutput,
                                vi->visual,
                                CWBorderPixel|CWColormap|CWEventMask, &swa );
    if ( !win )
    {
      printf( "Failed to create window.\n" );
      exit(1);
    }

    // Done with the visual info data
    XFree( vi );

    XStoreName( display, win, "GL 3.0 Window" );

    printf( "Mapping window\n" );
    XMapWindow( display, win );

    // Get the default screen's GLX extension list
    const char *glxExts = glXQueryExtensionsString( display,
                                                    DefaultScreen( display ) );

    // NOTE: It is not necessary to create or make current to a context before
    // calling glXGetProcAddressARBWindow
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
             glXGetProcAddressARB( (const GLubyte *) "glXCreateContextAttribsARB" );

    ctx = 0;

    // Install an X error handler so the application won't exit if GL 3.0
    // context allocation fails.
    //
    // Note this error handler is global.  All display connections in all threads
    // of a process use the same error handler, so be sure to guard against other
    // threads issuing X commands while this code is running.
    ctxErrorOccurred = false;
    int (*oldHandler)(Display*, XErrorEvent*) =
        XSetErrorHandler(&ctxErrorHandler);

    // Check for the GLX_ARB_create_context extension string and the function.
    // If either is not present, use GLX 1.3 context creation method.
    if ( !isExtensionSupported( glxExts, "GLX_ARB_create_context" ) ||
         !glXCreateContextAttribsARB )
    {
      printf( "glXCreateContextAttribsARB() not found"
              " ... using old-style GLX context\n" );
      ctx = glXCreateNewContext( display, bestFbc, GLX_RGBA_TYPE, 0, True );
    }

    // If it does, try to get a GL 3.0 context!
    else
    {
      int context_attribs[] =
        {
          GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
          GLX_CONTEXT_MINOR_VERSION_ARB, 0,
          //GLX_CONTEXT_FLAGS_ARB        , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
          None
        };

      printf( "Creating context\n" );
      ctx = glXCreateContextAttribsARB( display, bestFbc, 0,
                                        True, context_attribs );

      // Sync to ensure any errors generated are processed.
      XSync( display, False );
      if ( !ctxErrorOccurred && ctx )
        printf( "Created GL 3.0 context\n" );
      else
      {
        // Couldn't create GL 3.0 context.  Fall back to old-style 2.x context.
        // When a context version below 3.0 is requested, implementations will
        // return the newest context version compatible with OpenGL versions less
        // than version 3.0.
        // GLX_CONTEXT_MAJOR_VERSION_ARB = 1
        context_attribs[1] = 1;
        // GLX_CONTEXT_MINOR_VERSION_ARB = 0
        context_attribs[3] = 0;

        ctxErrorOccurred = false;

        printf( "Failed to create GL 3.0 context"
                " ... using old-style GLX context\n" );
        ctx = glXCreateContextAttribsARB( display, bestFbc, 0,
                                          True, context_attribs );
      }
    }

    // Sync to ensure any errors generated are processed.
    XSync( display, False );

    // Restore the original error handler
    XSetErrorHandler( oldHandler );

    if ( ctxErrorOccurred || !ctx )
    {
      printf( "Failed to create an OpenGL context\n" );
      exit(1);
    }

    // Verifying that context is a direct context
    if ( ! glXIsDirect ( display, ctx ) )
    {
      printf( "Indirect GLX rendering context obtained\n" );
    }
    else
    {
      printf( "Direct GLX rendering context obtained\n" );
    }

    printf( "Making context current\n" );
    glXMakeCurrent( display, win, ctx );
}
#endif

void myThread(){
  cl_int error;
  int dev_idx = 0;

  ClContext* cl = ClContext::getSingletonPtr();
#ifdef _WIN32
  initGlfw();
  cl->init();
#else
  const char* display_str[2] = { ":0.0", ":0.1" };
  Display* display[2];
  Window win[2];
  GLXContext ctx[2];
  initGlx(display_str[0], display[0], win[0], ctx[0]);
  //initGlx(display_str[1], display[1], win[1], ctx[1]);
  int d = 1;
  //glXMakeCurrent( display[d], win[d], ctx[d] );
  cl->init(display, win, ctx);
#endif

  if (glewInit() != GLEW_OK){
    std::cout << "Cannot init Glew\n";
    exit(0);
  }

  std::cout << "\n\n\nChoose The Device: \t";

  do{
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
  }

#ifdef _WIN32 
  cl_kernel mykernel = cl->createKernel("testKernel.cl", "myKernel", cl->devices[dev_idx]);
#elif _LINUX
  cl_kernel mykernel = cl->createKernel("/home/smostaja/MultiGPUComputing/testKernel.cl", "myKernel", cl->devices[dev_idx]);
#endif

  cl_int mem_size = 16 * 1024 * 1024;
  std::vector<cl_float4> host_a(mem_size), /*host_b(mem_size),*/ host_c(mem_size);

  for (size_t i = 0; i < mem_size; i++){
    host_a[i].s[0] = rand() % 1000 / 1000.0f;
    host_a[i].s[1] = rand() % 1000 / 1000.0f;
    host_a[i].s[2] = rand() % 1000 / 1000.0f;
    host_a[i].s[3] = 1.0f;
  }

  //#define USE_GPU_MEM
  cl_mem device_a, device_c;
  GLuint gl_buffer_c = 0;

  device_a = clCreateBuffer(cl->devices[dev_idx].ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, mem_size * sizeof(cl_float4), host_a.data(), &error); cl->checkError(error);

  if (!use_gpu_mem){
    device_c = clCreateBuffer(cl->devices[dev_idx].ctx, CL_MEM_WRITE_ONLY, mem_size * sizeof(cl_float4), nullptr, &error); cl->checkError(error);
  }
  else {

    glGenBuffers(1, &gl_buffer_c);
    glBindBuffer(GL_ARRAY_BUFFER, gl_buffer_c);
    glBufferData(GL_ARRAY_BUFFER, mem_size*sizeof(cl_float4), nullptr, GL_STATIC_DRAW);

    device_c = clCreateFromGLBuffer(cl->devices[dev_idx].ctx, CL_MEM_WRITE_ONLY, gl_buffer_c, &error);              cl->checkError(error);

  }

  while (true){

    float avg_time = 0.0f;
    const int n_tests = 100;
    std::vector<cl_float4> temp_mem(mem_size);
    for (size_t i = 0; i < n_tests; i++){

      //===============================
      // Draw
      //===============================
      //glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
      //glfwSwapBuffers(win);

      //===============================
      // update
      //===============================
      if (use_gpu_mem){
        glFinish();
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


      if (use_gpu_mem){
        error = clEnqueueReleaseGLObjects(cl->devices[dev_idx].cmd_queue, 1, &device_c, 0, nullptr, nullptr); cl->checkError(error);
      }
      else {

        clEnqueueReadBuffer(cl->devices[dev_idx].cmd_queue, device_c, CL_TRUE, 0, mem_size * sizeof(cl_float4), temp_mem.data(), 0, nullptr, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, gl_buffer_c);
        glBufferSubData(GL_ARRAY_BUFFER, 0, mem_size * sizeof(cl_float4), temp_mem.data());
      }

      clFinish(cl->devices[dev_idx].cmd_queue);
      clock_t end_time = clock();
      avg_time += static_cast<float>((end_time - beg_time)) / CLOCKS_PER_SEC;
    }
    avg_time /= n_tests;

    std::cout << "Execution Time (Avg.)  = " << 1.0f / avg_time << std::endl;
  }
}

int main() {

  boost::thread t(myThread);

  t.join();

  return 0;
}
