### Space Time transform for T2S

By Yi-Hsiang Lai (yl2666@cornell.edu)

#### Semantics

Here we define the semantics of space-time transform in T2S. A space-time transform maps an n-dimensional sequential loop nest to a 1-dimensional sequential time around (n-1)-dimensional parallel space. This transform will be applied to a Halide `Func`. In its most simple form, only the loops of the `Func` to be mapped to space are specified. Among these  loops, one must immediately enclose another, and the last one is the innermost loop. That is, only **inner loops** can be mapped to space. These inner loops will be fully unrolled, resulting in many copies of the original loop body; the copies are **PEs** (Processing Elements) in hardware, running in parallel, subject to the dependences between them. The loop that immediately encloses the inner loops, if any, executes sequentially, i.e. the loop is mapped to time. 

If the loops to be mapped to space have any other loop between them, or any other loop inside them, **a compilation error will occur**. The correct way is to reorder the loops before applying space-time transform. Below we show some examples.

```c++
// The temporal definition
Func A, B;
Var i, j;
A(i, j) = select(j == 0, 0, A(i, j-1));
B(i, j) = A(i, j) + 1;

A.merge_ures(B);

// Valid schedule #1
A.space_time_transform(i);

// Valid schedule #2
// All loops are mapped to space.
A.space_time_transform(i, j);

// Invalide schedule
A.space_time_transform(j);

// Valid schedule #3
A.reorder(j, i)
 .space_time_transform(j);
```
The above simplest form of space-time transform lets the PEs run in a dataflow fashion, driven by data, but the programmer does not have precise control of their execution order.

In the second form of space-time transform, a time schedule can be further enforced on the PEs. A PE runs at a time step only it is scheduled to run at that time step. Inside each PE, all the remaining loops execute sequentially. Below we show some examples.

```c++
// We have 3 dimensions here.
Func A;
Var i, j, k;
// The temporal definition is omitted

// Case 1: (i, j, k) => (i, j, t)
//         t = 2*i + 3*j + k
// The coefficient for k is 1 because inside each
// PE(i, j), the k loop runs sequentially. 
// In other words, any PE(i, j) starts at time
// 2*i + 3*j, and ends at time 2*i + 3*j + K-1,
// assuming the k loop iterates from 0 to K-1, where
// K is a constant.
A.space_time_transform({i, j}, {2, 3});

// Case 2: (i, j) => (i, t)
//         t = 3*i + j
// The transform applies only to the two inner loops.
// The outer loop k is unaffected.
A.space_time_transform({i}, {3});

// Case 3: double projection
//         (i, j, k) => (i, j, t1) => (i, t2, t1)
//         t1 = 2*i + 3*j + k
//         t2 = 2*i + j
// In this case, the compiler first recovers j by using t2,
//         j = t2 - 2*i,
// then recovers k
//         k = t1 - 2*i - 3*j = t1 - 3*t2 + 4*i
A.space_time_transform({i, j}, {2, 3})
 .space_time_transform({i}, {2});
```
As we can see from case 1 above, each PE has a time schedule for it to run. Therefore, the compiler should explicitly generate a check inside each PE if the PE should run at the current time. **By default, the check is not generated so the programmer needs to either ensure that a PE can always run (even when it is not its time: the trash data the PE produces won't get used and thus do not affect the final results), or add such a check to the UREs**. Simply set the third argument to `SpaceTimeTransform::CheckTime` if we want the compiler to add the check automatically. Below is an example:

```c++
Func A;
Var i, j, k;
// The temporal definition is omitted

// Add a check.
A.space_time_transform({i, j}, {2, 3}, SpaceTimeTransform::CheckTime);
```

#### Compiler 

The compiler work can be separated into two parts. The first part is the frontend implementation, where we add to Halide the `space_time_transform()` interface function, which only records the transform information. The real transform happens in the second part after the loop nest is built. There we transform the loops, calculate the time distance between expressions and allocate shift registers.

##### Front-end

For front-end, we add the `space_time_transform()` interface function in `Function.h` and implement it in `Function.cpp`. Here we need to check several things. For convenience, let us say *T* is the scheduling vector specified by the programmer in `space_time_transform()`, and we will call the  the loops to be mapped to space as **space loops**.

1. If space loops have any other loop between them, or any other loop inside them, error out.
2. If any dependence *d > 0* is not respected by the scheduling (i.e. if *T'd* <= 0 ), error out. 
3. If there is a series of space-time transforms, the space loops in one transform must be a proper subset of the space loops in the previous transform. Otherwise, error out.

We will add a list of structs in `StageScheduleContent`, called `space_time_transforms`. Each struct contains the following information.

1. A scheduling vector.
2. `CheckTime` or `NoCheckTime`.

The structs will be applied in order during lowering.

##### Lowering

During lowering, we perform space-time transform after the loop nest is built. Let us say the specification is

```c++
Func A, B, C, c;
Var i, j, k;
A(i, j, k) = select(i == 0, 0, A(i-1, j, k));
B(i, j, k) = select(j == 0, 0, B(i, j-1, k));
C(i, j, k) = select(k == 0, 0, A(i, j, k) + B(i, j, k) + C(i, j, k-1));
c(i, j)    = select(k == K - 1, C(i, j, k)); // The last Func to be merged is the output Func.

// Put the UREs into the same loop nest and set the bounds of the loops.
A.merge_ures(A, B, C, c)
 .set_bounds(i, 0, I, j, 0, J, k, 0, K);

// Double projection
//        (i, j, k) => (i, j, t1) => (i, t2, t1)
//         t1 = 2*i + 3*j + k
//         t2 = 2*i + j
A.space_time_transform({i, j}, {2, 3})
 .space_time_transform({i}, {2});

// Compile or realize will activate the second-part compilation.
c.compile_jit();   // or: c.realize();
```

The output Func is always the last Func in the merged UREs, and it has less loops than the other UREs. The output Func can only be written, and cannot be read, inside any of the UREs.

Below we list the steps for the compiler.

The initial loop nest before space-time transform is

```c++
// The stage of Func A
for (k, 0, K)
  for (j, 0, J)
    for (i, 0, I)
      A(i, j, k) = select(i == 0, 0, A(i-1, j, k))
      B(i, j, k) = select(j == 0, 0, B(i, j-1, k))
      C(i, j, k) = select(k == 0, 0, A(i, j, k) + B(i, j, k) + C(i, j, k-1))
      c(i, j)    = select(k == K - 1, C(i, j, k));
```

1. We locate a stage that has at least one space-time transform. In this example, the stage is `A`.
2. We create new loop variables that becomes the time loops. In this example, we create `t1` and `t2`.
3. For each transform, we reassign the indices for each expression. The space indices stay unchanged while the time distance becomes the time index. We also mark the space loops as unrolled.

   3.1. First, we calculate dependence distance by comparing the indices of an expression with the indices in LHS. The dependence distance should be a positive vector.

For the first transform (`t1 = 2*i + 3*j + k`), the dependence distantance is `(1, 0, 0)`, `(0, 1, 0)` and `(0, 0, 1)` for `A`, `B` and `C`, respectively. There is also a dependence distance `(0, 0, 0)` for each of `A`, `B` and `C` as well.

   3.2. Then, we apply the scheduling vector to calculate the time distance. 

```c++
// We replace the last dimension with the time distance.
// The dependence distance `(0, 0, 0)` is always mapped to time distance of 0.
// The space indices stay unchanged
// The space loops are unrolled
for (k, 0, K)
  unrolled for (j, 0, J)
    unrolled for (i, 0, I)
      A(i, j, 0) = select(i == 0, 0, A(i-1, j, 2)) // For A: (2, 3, 1)' x (1, 0, 0) = 2
      B(i, j, 0) = select(j == 0, 0, B(i, j-1, 3)) // For B: (2, 3, 1)' x (0, 1, 0) = 3
      C(i, j, 0) = select(k == 0, 0, A(i, j, 0) + B(i, j, 0) + C(i, j, 1)) // For C: (2, 3, 1)' x (0, 0, 1) = 1
      c(i, j)    = select(k == K - 1, C(i, j, 0));
```
Note that we do not touch the LHS of the output Func `c`.

In this example, the scheduling vector is `(2, 3, 1)`. If, let us say, the scheduling vector was not specified, then the vector is automatically set as `(0, 0, 1)`.

   3.3. We also need to update the time loops and add the check.

```c++
// min of t1 = 2*0 + 3*0 + 0 = 0
// max of t1 = 2*(I-1)+3*(J-1)+(K-1) = 2*I + 3*J + K - 6
for (t1, 0, 2*I+3*J+K-5)
  unrolled for (j, 0, J)
    unrolled for (i, 0, I)
      k = t1 - 2*i - 3*j
      if (0 <= k < K)
        A(i, j, 0) = select(i == 0, 0, A(i-1, j, 2))
        B(i, j, 0) = select(j == 0, 0, B(i, j-1, 3))
        C(i, j, 0) = select(k == 0, 0, A(i, j, 0) + B(i, j, 0) + C(i, j, 1))
        c(i, j)    = select(k == K - 1, C(i, j, 0));
```

   3.4. We can do similar calculations for the second transform.

For the second transform (`t2 = 2*i + j`), the dependence distantance is `(1, 0, 2)`, `(0, 1, 3)` and `(0, 0, 1)` for `A`, `B` and `C`, respectively. There is also a dependence distance `(0, 0, 0)` for each of `A`, `B` and `C` as well.

Now we can calculate the time distance for `t2`:

```c++
// And we only have one unrolled loop this time
// min of t2 = 2*0 + 0 = 0
// max of t2 = 2*(I-1) + (J-1) = 2*I + J - 3
// To make things simple, let's assume I = 10, J = 10, K = 10
for (t1, 0, 55 /* 2*I+3*J+K-5 */)
  for (t2, 0, 28 /*2*I+J-2*/)
    unrolled for (i, 0, 10 /*I*/)
      // Note that we need to recover j before recovering k
      j = t2 - 2*i
      k = t1 - 2*i - 3*j
      if (0 <= k < K) && (0 <= j <J)
        A(i, 0, 0) = select(i == 0, 0, A(i-1, 2, 2)) // For A: (2, 1)' x (1, 0) = 2
        B(i, 0, 0) = select(j == 0, 0, B(i, 1, 3))   // For B: (2, 1)' x (0, 1) = 1
        C(i, 0, 0) = select(k == 0, 0, A(i, 0, 0) + B(i, 0, 0) + C(i, 0, 1)) // For C: (2, 1)' x (0, 0) = 0
        c(i, j)    = select(k == K - 1, C(i, 0, 0));
```

4. Now we can flatten the time indices.

```c++
// t = t1 * 28 + t2
for (t1, 0, 55)
  for (t2, 0, 28)
    unrolled for (i, 0, 10)
      j = t2 - 2*i
      k = t1 - 2*i - 3*j
      if (0 <= k < K) && (0 <= j <J)
        A(i, 0) = select(i == 0, 0, A(i-1, 60))  // 2 * 28 + 2 = 58
        B(i, 0) = select(j == 0, 0, B(i, 88))    // 3 * 28 + 1 = 85
        C(i, 0) = select(k == 0, 0, A(i, 0) + B(i, 0) + C(i, 29)) // 1 * 28 + 0 = 28
        c(i, j) = select(k == K - 1, C(i, 0));
```

5. Now we can allocate shift registers for each `Func`. The size of a register is the maximum time distance appeared. We also need to replace the expressions with registers.

```c++
// We need to duplicate the registers for each PE
allocate srA[10][59] // the maximum for A is 58
allocate srB[10][86] // the maximum for B is 85
allocate srC[10][29] // the maximum for C is 28
or (t1, 0, 55)
  for (t2, 0, 28)
    unrolled for (i, 0, 10)
      j = t2 - 2*i
      k = t1 - 2*i - 3*j
      if (0 <= k < K) && (0 <= j <J)
        srA[i, 0] = select(i == 0, 0, srA[i-1, 58])
        srB[i, 0] = select(j == 0, 0, srB[i, 85])
        srC[i, 0] = select(k == 0, 0, srA[i, 0] + srB(i, 0) + srC[i, 28])
        c(i, j)   = srC[i, 0]
```

6. Finally, we can insert the shift register logic. Note that all the logics are unrolled.

```c++
allocate srA[10][59]
allocate srB[10][86]
allocate srC[10][29]
for (t1, 0, 55)
  for (t2, 0, 28)
    // Shift register logics
    // All loops are unrolled
    unrolled for (i, 0, 10)
      unrolled for (x, 0, 58)
        srA[i, 58-x] = srA[i, 57-x]
      unrolled for (x, 0, 85)
        srB[i, 85-x] = srA[i, 84-x]
      unrolled for (x, 0, 28)
        srC[i, 28-x] = srA[i, 27-x]
    // Main computation
    unrolled for (i, 0, 10)
      j = t2 - 2*i
      k = t1 - 2*i - 3*j
      if (0 <= k < K) && (0 <= j <J)
        srA[i, 0] = select(i == 0, 0, srA[i-1, 58])
        srB[i, 0] = select(j == 0, 0, srB[i, 85])
        srC[i, 0] = select(k == 0, 0, srA[i, 0] + srB(i, 0) + srC[i, 28])
        c(i, j)   = srC[i, 0]
```

##### Minimizing FIFO size
In the above step 4, we calculate the time distance of a producer expression and a consumer expression in terms of the flattened time `t = t1 * 29 + t2`; then we allocate a FIFO with the size equal to the maximum of such a flattened time distance, and we shift the FIFO every flattened time step. This FIFO size, however, is conservative: the producer expression in a PE does not necessarily produce a value every flattened time step; instead, the producer expression in the PE produces a value only at the steps when the PE is supposed to run. 

So how to minimize the FIFO size? We know that

```
//        (i, j, k) => (i, j, t1) => (i, t2, t1)
//         t1 = 2*i + 3*j + k
//         t2 = 2*i + j
```
So the flattened time is `t = 28*t1 + t2 = 58*i + 85*j + 28*k`. Take `B` for example. The originally dependence distance of `B` was `(0, 1, 0)`, the corresponding flattened time distance is 85 (That is why we have `srB[i, 85]` generated). The question is during 85 flattened time steps, how many new `B` values are produced by the producer expression `B(i, j, k)` in a PE? For this PE, the `i` index is fixed, and the PE iterates through `j` and `k`; when `j` (`k`) increments by 1, the flattened time increases by 85 (28).  There could be two possibilities: (1) `j` increases by 1, which means 1 new value produced by the PE during 85 flattened time steps, or (2) `k` increases by 3, which means 3 new values produced by the PE during 85 flattened time steps.

Since there is more than one possibility, we do not know exactly how many new values will be produced during 88 flattened time steps. However, we do know the number is either 1 or 3. Therefore, we could insert a channel with depth of max(1, 3)=3 to realize the FIFO instead. That leads to a saving of 85 registers. The channel is controlled by HW logic, which is the price to pay for the saving.

On the other hand, in general, if we know exactly 1 new value is produced by a producer expression in a PE every `s` flattened time steps, and the flattened time distance between the producer expression and its consumer expression is `x`, then we can allocate shift registers with size of `ceiling(x/s)`, and shift the registers every `s` flattened time steps. The code skeleton will look like this:

```c++
for t1
  for t2
    if flattened time of t1 and t2 passes by s steps
       shift registers once
    unrolled for i
      ...
```

#### Implementation Details

##### File Organization

We create the following files:
* t2s/src/SpaceTimeTransform.h
* t2s/src/SpaceTimeTransform.cpp

We modify the following files:
* Halide/src/Func.h
* Halide/src/Schedule.h
* Halide/src/Schedule.cpp
* Halide/src/Lower.cpp

##### General Implementation Guidelines

1. All helper functions and helper classes are put inside an anynomous namespace to avoid naming conflicts with other functions and classes.
2. Do not use ``using namespace xxx``.
3. For each class, memeber variables are placed in front of member functions.

##### Front-end Implementation

For front-end support, we first introduce a new C++ struct ``SpaceTimeTransformParams`` in ``Schedule.h``. The struct has two fields, which are the scheduling vector ``sch_vector`` and whether adding check time logic ``check_time``, respectively. Then, for each ``StageScheduleContents``, we introduce a new field ``transform_params`` that holds a vector of ``SpaceTimeTransformParams``. The reason we need a vector because the user may apply more than one space-time transformation. To access the transformation vector, we create two methods. Both methods return a reference to the vector. The only difference is that one is ``const`` while the other is not.

For user interface, we provide three ways to describe space-time transformations.

```c++
Var i, j, k;
// 1. Only the space loops
A.space_time_transform(i, j, k);

// 2. Space loops, but in the form of arrays
A.space_time_transform({i, j, k});

// 3. Space loops with scheduling vector
A.space_time_transform({i, j}, {1, 2});
```

The declaration can be found in ``Func.h`` while the implementation is in ``SpaceTimeTransform.cpp``. For implementation, we perform several checks first as mentioned in the above sections. If all checks are passed, we update the ``transform_params`` of each ``Func``. The real transformation happens during lowering.

##### Lowering Pass

During lowering, a new pass ``apply_space_time_transform`` is introduced right after the inital loop nest is built. The function definition locates in ``SpaceTimeTransform.h``. It takes in two arguments, which are the statement of the loop nest and the environment variable that contains the scheduling information, respectively.

The first step is to apply a Halide ``simplify`` pass, which removes all the loops with extent of one. Ideally, after simplification, we will have a loop nest having the following attributes.

1. We should have a single loop nest for each output. To be more specific, without isolation, we should have only one **perfect loop nest**. Moreover, the number of loops should be the same as the number of arguments. For example, if we have ``Func(Int(32), {i, j, k})`` then we should have three loops ``i``, ``j``, and ``k``.
2. For each loop nest, we will have a number of ``Realize`` nodes. Each ``Func`` that is not realized in the given program should have a corresponding ``Realize`` node. For example, if we have ``A.merge_ures(B, C, D)`` and ``D.realize()``, we should have three ``Realize`` nodes for ``A``, ``B``, and ``C``.

To make sure this happens, two extra passes are added in front of the main transformation pass, which are ``SelectToIfConverter`` and ``Simplifier``, respectively. For the former one, we transform any ``Select`` expression without an ``else_value`` to an ``IfThenElse`` statement without ``else_case``. For example,

```c++
// before transformation
A(...) = select(cond, B(...))

// after transformation
if (cond)
  A(...) = B(...)
```

The main reason that we are doing this are twofold.

1. Existing Halide simplification pass cannot handle ``Select`` with ``false_value`` undefined.
2. It is easier for code generation later on because OpenCL does not have semantics for such operation.

Usually, this transformation happens only for the output URE. However, this is not a restriction. Next, we perform a custom simplification pass ``Simplifier``. It is a class inherited from Halide's ``Simplify`` class. Here we only override one visit function for ``IfThenElse``. For Halide's simplification pass, it automatically propogates the simplified condition value of an ``IfThenElse`` statement to both its branches. However, this is not ideal because it may prevent us from calculating the time distance for some expressions. For example,

```c++
// input statement
if (k == 2)
  B(i, j) = A(i, j, k)
  
// incorrect simplification because we lose the information of index k
if (k == 2)
  B(i, j) = A(i, j, 2)
```

