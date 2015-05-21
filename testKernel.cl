__kernel void myKernel(
  __global float4* a,
  __global float4* c,
  int count
  ){ 

  int thread_idx = get_global_id(0);
  if (thread_idx >= count) return;

  c[thread_idx]  = a[thread_idx];
}

