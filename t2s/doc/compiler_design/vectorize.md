# Vectorization Design

By Size Zheng (zhengsz@pku.edu.cn)

## The overall idea

We will extend the current 1-D vectorization in Halide (VectorizeLoops.cpp) in the following two aspects:

### 1. **Enabling 1-D vectorization for FPGAs.**

A producer Func may pass vectorized data to its consumer Func, which may use the data as vectors or de-vectorize the data into scalars. We expect the vload and vstore feature in our previous T2S prototypes can be realized with the latest Halide's `vectorize()` primitive without special handling, except a small extension to the OpenCL code generator for FPGAs: currently, the code generator (CodeGen_OpenCL_Dev.cpp) allows vector size of only 2, 3, 4, 8, 16, but we would like to allow arbitrary size between 2 and 32 (by generating, for example `struct { int data[size];}` when `size` is not that of native OpenCL vector types.

#### 1.1 Design details for 1-D vectorization
##### 1. Entry function:

```c
Stmt vectorize_loops(Stmt s, const Target &t) {

    if (t.has_feature(Target::IntelFPGA)) {
        // initial vectorize
        VectorizeLoops vecloops(t);
        s = vecloops.mutate(s);
        // vectorize data path
        return VecDatapath(vecloops.vec_len).mutate(s);
    } else {
        // normal vectorize
        return VectorizeLoops(t).mutate(s);
    }
}
```

The new class `VecDatapath` is an IRMutator.
At first, a canonical mutator `VectorizeLoops` is used to do a basic job: finding `Vectorized` for loops and changing the body of the loops (Most of the work is changing data types). During the process, we collect information about `read_channel` and `write_channel`. The information is stored in `std::map<std::string, std::pair<int, int>> vec_len`, which is a map of `channel_name:string -> read_channel_lanes:int -> write_channel_lanes:int`. 
The information is used by `VecDatapath`.
`VecDatapath` will compare the read/write lanes of the same channel, if the lanes of read/write don't match, we need to change the `read_channel` to proper length.

##### 2. Match data length
```c
Expr visit(const Call *op) override {
    if (op->name == Call::read_channel || op->name == Call::write_channel) {
        int read_len = ... //read_channel_lanes
        int write_len = ... //write_channel_lanes
        if (read_len == write_len) {
            return op;
        } else if (read_len > write_len) {  // read must be vectorized
            if (read_len % write_len != 0) {
                user_error ...
            } else {
                // in this case, only need to change read
                if (op->name == Call::read_channel) {
                    // TODO: add codegen for shuffle concat
                    return Shuffle::make_concat(...);
                }
                // omit the change for write_channel
            }
        } else {    // read_len < write_len, write must be vectorized
            // in this case, only need to change read
            if (op->name == Call::read_channel) {
                // Use only a slice of the vector
                // TODO: add codegen for shuffle slice
                return Shuffle::make_slice(...);
            }
            // omit the change for write_channel
        }
    }
    // other cases
    return op;
}
```

The basic idea is when read lanes is longer than write lanes, break a single read into several smaller reads and use `Shuffle:: make_concat` to concat them; if read lanes is shorter than write lanes then read a long vector and only use a slice of it by `Shuffle::make_slice`.

#### 1.2 Serialize Vectorized Loop to Solve Dependence Problem

In UREs, we often write expressions like `C(k, i, j) = select(k == 0, some initial value, C(k-1, i, j))`, and we want to vectorize loop `k`. This appears in many designs such as the design for GEMM.  The problem is that there is a dependence on loop `k`, so the vectorized result is not correct. In such a case, we should change it back to serial loop. This is done in the pass `apply_serialize`.

### 2. **Extending 1-D vectorization to multi-dimensional vectorization for GPUs.**

Currently, Halide does not allow vectorizing more than one loop. When an outer and an inner loop are both marked as `ForType::Vectorized`, the compiler will vectorize only the outer loop: 

```
    // VectorizeLoops.cpp Line 850
    Stmt visit(const For *op) override {
        ForType for_type = op->for_type;
        if (for_type == ForType::Vectorized) {
            user_warning << "Warning: Encountered vector for loop over " << op->name
                         << " inside vector for loop over " << var << "."
                         << " Ignoring the vectorize directive for the inner for loop.\n";
            for_type = ForType::Serial;
        }
        ....
```
We need to remove this limitation, and allow vectorizing the inner loop as well.

Particularly, we would like **space-time transformed UREs to be vectorized** so that we can efficiently run arbitrary systolic arrays on both FPGAs and GPUs.

Note that we **extend** 1-D vectorization to multi-dimensional vectorization, instead of inventing a separate multi-dimensional vectorization. To achieve this purpose, we need the following extensions:

1. **Extending Halide::Type to express tensors, without affecting any workload of the current Halide.**

```c++
struct Type {
private:
    halide_type_t type;
    // Extension: if not null, "base_type", instead of the above "type", is the base type, and
    // the "base_type_lanes" indicates how many "base_type" elements this type contains.
    struct Type  *base_type;
    uint16_t      base_type_lanes;
public:
    // Add any interface function needed for tensors. These functions
    // are invoked only by vectorization. 
```

**Probably we do not need change type inference** at all: operations related with vector types are already handled in IROperators.cpp, for example, 

```
Expr operator+(Expr a, int b) {
    ...
    return Internal::Add::make(std::move(a), Internal::make_const(t, b));
}
```
where `make_const` will do the necessary broadcast. It looks pretty general, and should be applicable to tensors without any change. Therefore, we should expect these tensor operations be processed without (any significant) special handling: `ramp(0, 1, W) + q`, `ramp(0, 1, H) == 0`, `A(ramp(0, 1, H), ramp(0, 1, W)) + 1`, and `B(ramp(0, 1, H), ramp(0, 1, W)) = b(k, c, p, q)`.

2. **Allowing multi-dimensional intervals.**

This should be automatically done by the current Halide without any special handling: currently, every Variable is extended to a vector individually (See `VectorSub::visit(const Variable *op)`) and therefore, multiple such Variables will naturally lead to multi-dimensional intervals.

For example, for this loop nest:

```c++
for (i, 0, I)
  for (j, 0, J)
    for (k, 0, K)
      ... A(i, j, k) with primitive type t ... // A reference that can be in either LHS or RHS.
```

after vectorizing all the three loops, we get the transformed code like this:

```c++
      ... A(ramp(0, 1, I), (0, 1, J), (0, 1, K)) with tensor type t' ...
```
Here type `t'` is a 3-dimensional tensor type, which is expressed like this: 

```c++
t''': with type = (code=t, bits=bits of t, lanes = K)
t'' : with base_type=(base_type = t''', base_type_lanes = J)
t'  : with base_type=(base_type = t'', base_type_lanes = I)
```

3. Code generation.

First, we need map tensor types to native types of the target language of the target HW. OpenCL has native vector types, and CM has native vector and matrix types. We need map to them whenever possible. And if not directly mappable, we need decompose the tensors so that decomposed types are mappable, e.g. by generating something like `struct { native_type data[size];}`. This is kind of "de-vectorizing".

Second, we need map tensor operations to native operations of target language of the target HW. If not directly mappable, decompose the operations, e.g. by generating `for {native operations}`. This is kind of "de-vectorizing".

For example, for a tensor load, `a(n, c, ramp(0, 1, H) + p, ramp(0, 1, W) + q)`, we can generate code like

```c++
#pragma unroll
for (h, 0, H):
    vread(a(n, c, h + p, q), W) // read a vector of size W
```

For another tensor load, `a(ramp(0, 1, H), ramp(0, 1, W), c, n)`, we can generate code like

```c++
#pragma unroll
for (h, 0, H)
  #pragma unroll
  for (w, 0, W)
    vread(a(h + p, w + q, c, n), 1) // read a vector of size 1, single element
```

Stores can be handled similarly.

In addition to expand a single operation individually (which is always right), we may look at more than one operation to generate better code, if the operations are all about assignement to the same destination. For example, it is a common pattern due to UREs to see IR like this:

```c++
if h < some_value:
    B(h, ramp(0, 1, W)) = b(k, c, p, q) // Operation 1
else:
    // shift B
    B(h, ramp(0, 1, W)) = B(h-some_value, ramp(0, 1, W)) // Operation 2
```

We can generate the operations individually. But it is better to optimize the IR into this:

```c++
B(ramp(0, 1, some_value), ramp(0, 1, W)) = b(k, c, p, q)
// shift B
B(ramp(0, 1, H-some_value), ramp(0, 1, W)) = B(ramp(0, 1, H-some_value)-some_value, ramp(0, 1, W))
```
and then generate code.
