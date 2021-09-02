# Design of Merge Ures

By Weihao Zhang (zwh18@mails.tsinghua.edu.cn)

## Func Declaration and Definition

For declaration, T2S only adds one new feature. That is declared by return types and arguments, this can be viewed as two sub-features:
1. explicit Func(Type return_type, const std::vector<Var> &args, Place place = Place::Host);
2. explicit Func(const std::vector<Type> &return_types, const std::vector<Var> &args, Place place = Place::Host); 

For definition, we use URE syntax and the UER constraints should be kept (The following is not formal math description):
1. Simple args constraints: The URE arguments of LHS and RHS should be simple form, like: L(i, j) = R(i - 1, j - 1);
2. Order constraints: the URE arguments of LHS and RHS should have same number and be in same order.
3. Distance constraints: The distance between caller and callee should be non-negative vector (Each element is non-negative).
4. Extended URE: Extended URE can ignore some arguments that RHS has. The extended URE can only appeared in LHS and be allowed in the last URE in merge_ures(). Example:
```
f(x, y)= select(x == 0, x, f(x - 1, y));
out(y) = select(x == 10, f(x, y), 0);  // 'out' is extended URE
```

There are two things should be noted:
1. The 'Use Before Define' is allowed, if and only if the Func is declared by [return_type(s) and argument(s)] or [Expression], like:
```
Func f(Int(32), {i, j});
f(i, j) = select(i == 0, i, f(i - 1, j)); // f is used before define.
```
Or:
```
ImageParam a(Int(32), 2, "a");
Buffer<int> in = create_a_buffer();
a.set(in);

Func f(a), g;
g(i, j) = f(i, j); // f is used before define.
```

2. If the func is declared by return_type(s), the user should keep the type right during definition, like:
```
Func f(Float(32, 1), {i, j}), g(Int(32, 1), {i, j});
f(i, j) = (i + j) * 1.0f;
g(i, j) = cast(Int(32, 1), f(i, j - 1));
```
## Interface of merge_ures()

```
A.merge_ures(Func B, Func C, Func D, ...)
```
A is called representative URE (previously called control URE, which however, might confuse with a URE for passing control signals). The B, C, D, ... will be realized under the A loop nests and the realization order is A, B, C, D, ...

## Design

### Basic

The `merged_ures()` will put all the merged UREs under the inner-most loop level of the representative URE. This can be achieved by the Halide directive: `compute_with()` (But in Halide, two Funcs that do `compute_with()` can't have direct dependencies. We should do extra modifies to allow it). The `compute_with()` will fuse one Func under the specified loop level of another Func, like:
```
f(x, y) = x + y;
g(x, y) = x - y;
h(x, y) = f(x, y) + g(x, y);
f.compute_root();
g.compute_root();
g.compute_with(f, x);
```
The resulting loop nests are:
```
for y:
  for x:
    f(x, y) = x + y
    g(x, y) = x - y
for y:
  for x:
    h(x, y) = f(x, y) + g(x, y)
```
So, for `A.merge_ures(B, C, D)`, we can do a series of `compute_with()` (MergeUREs.cpp):
```
Var inner_loop = find_inner_most_loop_level(A);
D.compute_with(C, inner_loop);
C.compute_with(B, inner_loop);
B.compute_with(A, inner_loop);
```
### Realization Order

The realization order of merged UREs is specified by the user through the `merged_ure()` interface. In Halide, the order will be deduced automatically. This order should be replaced with the specified order. And during this procedure, the dependency problem of `compute_with()` will be solved by the way.\
The Halide will construct a `graph` according to the fuse information recorded by `compute_with()` and do DFS to get the realization order (RealizationOrder.cpp). There are some cases to show the `graph`: \
![Halide realization order graph](/t2s/doc/compiler_design/img/merge_ures_1.png)
For one fuse group (Representing the Funcs that are fused by `compute_with()`), the compiler create a dummy node `_fg*` to represent. All the func nodes in this group will point to the `_fg*`. The `_fg*` will also point to the Funcs that called by the Funcs in this group (The red arrow). With this `_fg*` node, all the dependencies of this group will be realized first, while Funcs in this group can be realized in arbitrary order.\
But the Halide DFS has an issue: The graph can't contain circles. So Halide uses some assertions to avoid this. The circle is caused by the two fused Funcs which have direct dependency, like this:
```
f(i, j) = 0;
g(i, j) = f(i, j);
g.compute_with(f, j);
```
So, the Halide document says "There should not be any dependencies between those two fused stages" for `compute_with()`. \
The semantic of `compute_with()` should not be broken so that the T2S pipeline also can be a mixture of compute_with and merge_ures. So in T2S graph, some decorations are added in merge_ures area and other areas are kept. The original DFS is done to get the correct order. This is a case: \
![T2S realization order graph](/t2s/doc/compiler_design/img/merge_ures_2.png)
For the fused group that is caused by merge_ures (_fg$2), a node called `_mg*` will be created to replace the merged Funcs (e, f) and the Func who calls the merge_ures (c) will be pointed by the `_mg*` node. The `_mg*` node also points to `_fg*` node (as a normal member of the `_fg*` group). Thus, no matter what complex dependencies happened within merged ures, there are no circles in the graph and the DFS can generate an order. \
Last, the `_mg*` will be replaced with the sequence of merged ures after DFS. The sequence is the realization order specified by the users, which is kept in definition().schedule().merged_ures.
### Extended URE

The original `compute_with()` only supports Funcs that have same arguments. If we want to allow extended URE, some changes must be done during lowering. A flag called is_extended_ure is set to be true if an URE is extended. This flag will be identified and specific processing will be done. Particularly, for the extended URE that lacks the inner-most loop level of the representative URE, which means the `compute_with()` can't find that level (ScheduleFunction.cpp: build_produce_definition), the user can write the extended URE in two ways:
```
B(j) = A(20, j);
Or
B(j) = select(i == 20, A(i, j), 0);
```
For the second method, the generated IR will contain `B(B.s0.j) = select(B.s0.i == 20, A(B.s0.i, B.s0.j), 0)`. While, the `B.s0.i` is not in the symbol table. In current solution, the `B.s0.*` will be replace with `A.s0.*` (* means the loop variable in the Provide expression)(ScheduleFunction.cpp: build_provide_loop_nest). So the processed IR is:
```
...
B(A.s0.j) = select(A.s0.i == 20, A(A.s0.i, A.s0.j), 0)
...
```
During the definition, the Halide will check if all the variables are defined. For the second method, the variable `i` is not defined for `B`. This needs a little extra process to let it pass, while most of the original `defined` checks are still retained. (Function.cpp: CheckVars)
### Effects for Other Directives

The directives called by the representative URE after `merge_ures()` should also affect the merged UREs. For example:
```
A.merge_ures(B, C)
 .reorder(k, j, i)
 .set_bounds(i, 0, I,
             j, 0, J,
             k, 0, K);
```
`A` calls `reorder()` after `merge_ures()`, so `B, C` should do the same reordering. So, if `A` has merged UREs under its loop nest, the same directive should be called by these UREs. Like `set_bounds()`:
```
Func &Func::set_bounds(const std::vector<Var> &vars, const std::vector<Expr> &mins, const std::vector<Expr> &extents) {
......
    if (func.has_merged_defs()) {
        for (auto f : func.definition().schedule().merged_ures()) {
            if (!f.function().definition().schedule().is_extended_ure()) {
                f.set_bounds(vars, mins, extents); // Call the set_bounds() with same arguments.
            }
        }
    }
    return *this;
}
```
### Bounds Inference

The original Halide may raise some errors if there are mutual- or self-reference UREs during bounds inference. To avoid this, all the mutual- or self- reference UREs should be found, and the call to these Funcs will be replaced with Expr(0) during adding the image checks (AddImageChecks.cpp). For:
```
f(i, j) = select(i == 0, 0, f(i - 1, j));
g(i, j) = select(i == 0, 0, g(i - 1, j));
```
Will be converted to:
```
f(i, j) = select(i == 0, 0, Expr(0));
g(i, j) = select(i == 0, 0, Expr(0));
```
### Effects Caused by Other Directives

The only directive we consider for now is reorder after `merge_ures()` . In `merge_ures()`, we want all UREs fused under the inner-most loop level of the representative URE. If we do reordering after `merge_ures()`, the inner-most loop level may be changed. To solve this, the information kept by the `compute_with()` should be changed and the new inner-most loop level will be recorded.
### Recursive Reference

All the UREs should have initial value. In this negative example, the `f` and `g` don't have initial values:
```
Example 1:
f(i, j) = select(i == 0, g(i, j), f(i - 1, j));
g(i, j) = select(i == 0, f(i, j), g(i - 1, j));
```
This is positive case:
```
Example 2:
f(i, j) = select(i == 0, i, f(i - 1, j));
g(i, j) = select(i == 0, f(i, j), g(i - 1, j));
```
To check if every Funcs have initial values, we build an recursive tree. If a Func has path that ends with [initial values] or [other Func that has initial values], that function has initial values. If all the Funcs has initial values, the case is positive, otherwise negative. For example 2, the tree is:

![A simple idea of recursive tree](/t2s/doc/compiler_design/img/merge_ures_3.png)

For `g`, it has a path that ends with `i`, so `g` has an initial value. For `f`, it has path that ends with `g`, so `f` also has an initial value. So, this case is positive. But this is just a simple case, this tree doesn't apply to complicated cases. 

We first clarify how to construct a recursive tree. A non-leaf node is Func and a leaf node is Func or constant expression. We begin with a Func as the root node. All the Func it calls will become its children. If a child is a constant value, the child is a leaf node. If a child already appears elsewhere in the tree, this child is also turned into a leaf. The procedure will go on util the tree is completed.

Each node is assigned with a bool value, which is true if the node has an initial value. The value of a constant leaf node is always true. How to determine a non-leaf node value? For node `A = select(i == 0, B, C)`, A is true when B is true or C is true (A = B ∨ C). But for node `A = B + C`, A is true when B and C both are true (A = B ∧ C). So, for a complex node `A = select(.., B + select(.., C + D, D), E + F) + select(..., H, B)`, we can have the equivalent bool expression: A = ((B∧((C∧D)∨D))∨(E∧F))∧(H∨B). This can be converted to the Conjunctive Normal Form (CNF): A = (B∨E) ∧ (B∨F) ∧ (B∨H) ∧ (D∨E) ∧ (D∨F). A is true if and only if each entry of CNF is true. So, we can analyze A, get the bool expression and convert it to the CNF (There are known algorithms for this purpose). Based on the CNFs of the nodes, we can derive their bool values step by step (Keep a queue of true nodes, and iteratively traverse the tree based on each true node). 

Check this example:

```
Example 3:
f(i, j) = select(i == 0, i, g(i - 1, j)) + h(i, j);
g(i, j) = select(i == 0, f(i, j) + h(i, j), g(i - 1, j));
h(i, j) = select(i == 0, i, f(i - 1, j) + g(i - 1, j)) + g(i, j);
```
We can construct the tree:

![A complex case of recursive tree](/t2s/doc/compiler_design/img/merge_ures_4.png)

In the end, some nodes still can't be true, so this is a negative case.