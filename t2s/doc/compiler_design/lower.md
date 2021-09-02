# A Berief Introduction to the Lowering Process of Halide

By Hongbo Rong (hongbo.rong@intel.com)



This document briefly explains the IR change after every phase in Lower.cpp, so as to help compiler developers better understand the internals.

We take the following example:

        Var x("x"), xi("xi"), y("y");
        Func f("f"), g("g");
    
        f(x) = x * x;
        g(x) = f(x) + f(x - 1);
    
        g.split(x, x, xi, 10, TailStrategy::RoundUp);
        f.store_root().compute_at(g, x);

# Creating initial loop nest

        // f.store_root. The bounds of the Realize (allocation) are SYMBOLIC.
        realize f([f.x.min_realized, f.x.extent_realized]) {
         produce g {
          // The bounds of the loop (before splitting) are SYMBOLIC
          let g.s0.x.loop_max = g.s0.x.max
          let g.s0.x.loop_min = g.s0.x.min
          let g.s0.x.loop_extent = ((g.s0.x.max + 1) - g.s0.x.min)
          
          // g.split(x, x, xi, 10). The loop bounds are derived from the bounds before splitting.
          let g.s0.x.x.loop_extent = (((g.s0.x.loop_max - g.s0.x.loop_min) + 10)/10)
          let g.s0.x.x.loop_max = ((((g.s0.x.loop_max - g.s0.x.loop_min) + 10)/10) - 1)
          let g.s0.x.x.loop_min = 0
          let g.s0.x.xi.loop_extent = 10
          let g.s0.x.xi.loop_max = (10 - 1)
          let g.s0.x.xi.loop_min = 0
          
          for (g.s0.x.x, g.s0.x.x.loop_min, g.s0.x.x.loop_extent) {
           let g.s0.x.xi.base = ((g.s0.x.x*10) + g.s0.x.loop_min)
           
           // f.compute_at(g, x);
           produce f {
            // The loop bounds are SYMBOLIC
            let f.s0.x.loop_max = f.s0.x.max
            let f.s0.x.loop_min = f.s0.x.min
            let f.s0.x.loop_extent = ((f.s0.x.max + 1) - f.s0.x.min)
            for (f.s0.x, f.s0.x.loop_min, f.s0.x.loop_extent) {
             f(f.s0.x) = (f.s0.x*f.s0.x)
            }
           }
           consume f {
            for (g.s0.x.xi, g.s0.x.xi.loop_min, g.s0.x.xi.loop_extent) {
             let g.s0.x = (g.s0.x.xi.base + g.s0.x.xi)
             g((g.s0.x.xi.base + g.s0.x.xi)) = (f((g.s0.x.xi.base + g.s0.x.xi)) + f(((g.s0.x.xi.base + g.s0.x.xi) - 1)))
            }
           }
          }
         }
        }

As we can see, the bounds of Realize and loops (before splitting) depend on the following yet-to-define symbols:

    Allocation bounds:
        f.x.min_realized, f.x.extent_realized
        
    Computation bounds:
        g.s0.x.min, g.s0.x.max
        f.s0.x.min, f.s0.x.max

Note Func g is output Func, and its output buffer is passed in, and thus the IR has no Realize for it.

# Add_image_checks

This pass inserts definitions and assertions for I/O buffers' shapes (min, extent, stride) at the beginning of the IR:

    define I/O buffer's required shapes (Still SYMBOLIC):
        let g.extent.0.required = (((((((g.s0.x.max - g.s0.x.min)/10)*10) + g.s0.x.min) + 9) + 1) - (g.s0.x.min + 0))
        let g.min.0.required = (g.s0.x.min + 0)
        let g.stride.0.required = 1
        
    define I/O buffer's constrained shapes:
        let g.stride.0.constrained = 1
    
    define I/O buffer's proposed shapes:
        let g.stride.0.proposed = 1
        let g.min.0.proposed = g.min.0.required
        let g.extent.0.proposed = g.extent.0.required
        
    assertions and others:
        assert I/O buffer's proposed shape subsumes the required shape
    
        I/O buffer initialization with the proposed shape:
            _halide_buffer_init(g.buffer, ..., make_struct(g.min.0.proposed, g.extent.0.proposed, g.stride.0.proposed, 0), ...)
    
        assert I/O buffer's type, dimensions
    
        assert I/O buffer's (SYMBOLIC) real shape subsumes the required shape:
            assert(((g.min.0 <= g.min.0.required) && (((g.min.0 + g.extent.0) - 1) >= ((g.min.0.required + g.extent.0.required) - 1))), ...
    
        assert I/O buffer's (SYMBOLIC) real extents >=0: 
            assert((g.extent.0 >= 0) ...
            
        assert I/O buffer's (SYMBOLIC) real strides equal constrained strides:
             assert((g.stride.0 == g.stride.0.constrained), ...)
                    
        assert I/O buffer's real size not too large:
             assert(g.extent.0 * g.stride.0.constrained <= (uint64)2147483647, halide_error_buffer_allocation_too_large ...)

The required shape of a buffer is determined by the loop bounds around the buffer access. 

The constrained shape of a buffer comes from the programmer's specifying of `Parameter::set_min/extent/stride_constraint`. A default constraint is: the first dimension's stride is 1. This default constraint can be overridden by the programmer using `Parameter::set_stride_constraint` explicitly.

By passing a required min/extent/stride through the corresponding constraint on min/extent/stride, we get the proposed shape of a buffer. When there is no constraint, the proposed shape is the required shape. The proposed shape is used to initialize the buffer.

# (Computation) bounds inference

    Define the computation bounds: 
        let g.s0.x.max = ((g.min.0 + g.extent.0) - 1)
        let g.s0.x.min = g.min.0
        let f.s0.x.max = g.s0.x.max
        let f.s0.x.min = (g.s0.x.min + -1)

Now the computation bounds are deterimed by the output buffer's bounds (`g.min/extent.0`), which are SYMBOLIC.

# Sliding window

    Change the lower computation bound of the callee, intuitively like this:
        let f.s0.x.min = in the first g.s0.x.x iteration ? g.s0.x.min - 1 : the index after the last `f` value computed

The above statement basically means: `f.s0.x.min` points to the next `f` value to be computed.

# Allocation bounds inference

    Define the allocation bounds before the Realize:
         let f.x.max_realized = (((((g.s0.x.max - g.s0.x.min)/10)*10) + g.s0.x.min) + 9)
         let f.x.min_realized = min(select((g.s0.x.min < (g.s0.x.max + -9)), g.s0.x.min, (g.s0.x.min + -1)), (g.s0.x.min + -1))
         let f.x.extent_realized = ((((((g.s0.x.max - g.s0.x.min)/10)*10) + g.s0.x.min) - min(select((g.s0.x.min < (g.s0.x.max + -9)), g.s0.x.min, (g.s0.x.min + -1)), (g.s0.x.min + -1))) + 10)

As we can see, the allocation bounds are deterimed by the computation bounds (such as `g.s0.x.min`, defined after the computation bounds inference before).

From now on, the only undefined symbols are the output buffer's bounds (`g.min/extent.0`), which are passed in during execution. Therefore, the whole IR is considered complete, and the compiler can simplify statements (Before this point, the compiler can only simplify Exprs).

# First simplification

# Storage folding

     // Fold the storage into a buffer that will contain only live values.
     realize f([0, 16]) {
      produce g {
        ...
        produce f {
         ...
         for (f.s0.x, f.s0.x.min_1, ((((g.s0.x.x*10) + g.min.0) - f.s0.x.min_1) + 10)) {
          // Write to storage cyclically
          f((f.s0.x % 16)) = (f.s0.x*f.s0.x) 
         }
        }
        consume f {
         for (g.s0.x.xi, 0, 10) {
          let g.s0.x = (((g.s0.x.x*10) + g.min.0) + g.s0.x.xi)
          g((((g.s0.x.x*10) + g.min.0) + g.s0.x.xi)) = (f(((((g.s0.x.x*10) + g.min.0) + g.s0.x.xi) % 16)) + f((((((g.s0.x.x*10) + g.min.0) + g.s0.x.xi) + -1) % 16)))

# Prefetch

# Destructuring tuple-valued realizations

# Storage flattening

# Device: inject_host_dev_buffer_copies

# Second simplification

# Unrolling

# Vectorizing

Add vectorizing to function g:

```
    Var x("x"), xi("xi"), y("y");
    Func f("f"), g("g");

    f(x) = x * x;
    g(x) = f(x) + f(x - 1);

    g.split(x, x, xi, 16, TailStrategy::RoundUp).vectorize(xi,8);
    f.store_root().compute_at(g, x);
```

Lowering after vectorizing:

```
produce g{
	for(g.s0.x.x,g.min.0,(g.extent.0+15)/16){
		produce f{
			let f.s0.x.loop_max = f.s0.x.max
    		let f.s0.x.loop_min = f.s0.x.min
    		let f.s0.x.loop_extent = ((f.s0.x.max + 1) - f.s0.x.min)
    		for (f.s0.x, (((g.s0.x.x*16) + g.min.0) + -1), ((g.s0.x.x*16) + 17)) {
     			f(f.s0.x) = (f.s0.x*f.s0.x)
    		}
		}
		consume f {      		
      		for(g.so.x.xi.xi,0,2){
      			g[x8(g.s0.x.x*16+g.min.0)+x8((g.s0.x.xi.xi*8) + ramp(0, 1, 8))]=f[x8(g.s0.x.x*16+g.min.0) + x8((g.s0.x.xi.xi*8) + ramp(0, 1, 8)) + x8(-1)]+f[x8(g.s0.x.x*16+g.min.0) + x8((g.s0.x.xi.xi*8) + ramp(0, 1, 8))]
      		}
     	}
    }
}
```




# Rewriting vector interleaving

# Partitioning loops

# Loop trimming

# Injecting early frees

# Bounding small allocations

Use bounds analysis to attempt to bound the sizes of small allocations (Find constant bounds for them). Inside GPU kernels this is necessary in order to compile. On the CPU this is also useful, because it prevents malloc calls for (provably) tiny allocations.

# Final simplification



