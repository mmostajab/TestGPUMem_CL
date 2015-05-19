typedef struct {
  float4 p;
  float4 d;
  float4 n;
} vert;

__kernel void myKernel(
  __global float4* a,
  __global void* c_ptr,
  int count
  ){ 

  __global vert* c = (__global vert*) c_ptr;

  int thread_idx = get_global_id(0);

  if (thread_idx > count) return;

  int offset = thread_idx * 1000;
  const float4 zero_vec = { 0.0f, 0.0f, 0.0f, 0.0f };

  float4 s = a[thread_idx];
  float4 d;
  for(int i = 0; i < 1000; i++){
    d = (float4)(0, 0, 1, 0);
    s += d;
    c[offset + i].p = s;
    c[offset + i].d = d;
    c[offset + i].n = zero_vec;
  }    
}
