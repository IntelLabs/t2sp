# Design of Gathering

By Yunshan Jia (jiayunshan@pku.edu.cn)

## Assumptions

1.  For `F.gather(A, i, GatherStrategy)`, we assume `A` is passed to `F` in a channel and `F` reads the channel unconditionally, and loop `i` is a space loop (an unrolled or vectorized loop, which is usually with a static constant extent).
2. We assume that Func `F` calls gather only once in the current design.

## Parameter Storage

We plan to use class `GatherItem` to save args for `F.gather(A, i, GatherStrategy)`. GatherItem contains the func whose values will be gathered (`A`), loopvar (`i`), GatherStrategy (`Up` or `Down`), and a boolean `valid` that is initialized as false but set true by `F.gather`. GatherItem will be declared in Gather.h as well as added to DefinitionContents as a new member variable to save gather args until they were handled in gather_data() in lower.

## Compilation order

Gathering requires loop `i` to be either unrolled or vectorized. As we will see, unrolling and vectorization have major impact on the final code to be generated out of gathering. The same impact should exist for scattering as well.

In the compiler, we should order the optimizations in this order:

```
    vectorization
    scattering
    gathering
    unrolling
```
That is, do vectorization first, and do unrolling last. Between them, do scattering and gathering (their order does not matter). Otherwise, if we put scattering/gathering before vectorization, the vectorizer might not be able to analyze the loop body that becomes complicated after scattering/gathering. Unrolling needs little analysis, and thus we can do it at the very end. 

## Brief description

We distinguish two cases (with `GatherStrategy::Down` for example): 

### Unrolling + gathering: `F.unroll(i).gather(A, i, GatherStrategy::Down)`
In this case, gathering is only a special way to order the scalars that are sent to the consumer Func of `F`. 

Let us start with this IR:

```
            Code 1: starting IR (unrolled case)
    for outer loops
        unroll for i = 0; i < I; i++
          unroll/vectorize for j = 0; j < J; j++       // Not exist if loop j was vectorized away
                some work1
                value(i, j) = read_channel("A", i, j); // Or value(i, j:0:J)=read_channel("A", i, j:0:J) if loop j was vectorized away, where j:0:J is a Ramp node.
                                                       // Let us remember this as LHS = RHS to be simple.
                some work2
```
The innermost loop(s) are always space loops. The results A are sent to F over channels, whose dimensions correspond to the space loops. We will gather along one of the space loops, e.g. loop i.

Note that there is no if...else around read_channel. We intentionally let the core systolic array to pass results out only through channels. Thus Func `F` is driven by the data in the channels. This makes our work simpler than scattering.

For simplicity, here we show the loops with min=0. But it can be any value.

Let `resultType` be the type of LHS.

Unrolling duplicates instructions, and thus leads to MIMD code. However, as we said, we will delay this physical unrolling to the very last step. Therefore, the IR is still the same as Code 1 when gathering happens.

Now with `gather(A, i, GatherStrategy::Down)`, we expect the following:

```
             Code 2: Unrolling + gathering results
    decl resultType FIFO[I][J] (or FIFO[I]) if loop j is unrolled (or vectorized);  // Global channels between PEs due to the unrolled loop(s).
    for outer loops
        unroll for i = 0; i < I; i++
            for t = i; t < I; t++
                unroll/vectorize for j = 0; j < J; j++     // Not exist if loop j was vectorized away
                    decl resultType tmp;                   // A temporary local to each PE.
                    if(t == i){                            // My turn
                        tmp = RHS;
                    } else {                               // Get previous PEs' data
                        tmp = read_channel(FIFO[i+1][j] or FIFO[i+1] if loop j was vectorized away).
                    }
                    if (i > 0) {                           // Pass down to the next PE
                        write_channel(FIFO[i][j] or FIFO[i] if loop j was vectorized away, tmp);
                    } else {                               // Send to the consumer Func of F
                        LHS(with i replaced with t) =tmp;  // PE(0, j) represents PE(t, j) to write the output
                    }
```
It might seem complicated when we consider the case loop `j` was unrolled or vectorized. But as the comments above show, we do not have to worry if loop `j` was vectorized at all: such a loop already disappears from the IR. We simply gather all unrolled loops, and from the unrolled loops, we determine the dimensions of FIFOs, and indexing of the FIFOs in reads and writes. In short, gathering is orthogonal to vectorization, and is composable with vectorization without any special handling. That makes our code generation simple.

### Vectorization + gathering: `F.vectorize(i).gather(A, i, GatherStrategy::Down)`
In this case, gathering is only a special way to compose vectors and order them that are sent to the consumer Func of `F`. 

Let us start with this IR:

```
            Code 3: starting IR (vectorization case)
    for outer loops
        //vectorize for i = 0; i < I; i++                    // Loop i was vectorized away
          unroll/vectorize for j = 0; j < J; j++             // Not exist if loop j was vectorized.
                some work1
                value(i:0:I, j)=read_channel("A", i:0:I, j); // Or value(i:0:I, j:0:J)=read_channel("A", i:0:I, j:0:J) if loop j was also vectorized away
                                                             // Let us remember this as LHS = RHS to be simple.
                some work2
```

Let `resultType` be the type of LHS. Note that in this type, the `i` dimension has `I` number of elements.

With `gather(A, i, GatherStrategy::Down)`, we expect the following:

```
             Code 4: Vectorization + gathering results
    decl resultType FIFO[I][J] (or FIFO[I]) if loop j is unrolled (or vectorized); // Global channels between PEs due to the unrolled loop(s).
    for outer loops
        unroll for i = 0; i < I; i++                                               // Add back loop i and unroll
            unroll/vectorize for j = 0; j < J; j++                                 // Not exist if loop j was vectorized away
                declare resultType tmp;                                            // A temporary local to each PE. Remember that its i'th dimension has I elements.
                some work1
                if (i < I – 1) {                                                   // Get and merge with previous PEs' data
                    tmp = read_channel(FIFO[i+1][j] of FIFO[i+1] if loop j was vectorized away)
                }
                tmp[i] = RHS(with i'th dimenion devectorized from i:0:I back to i) // This PE's data
                if (i > 0) {                                                      // Pass down to the next PE
                    write_channel(FIFO[i][j] of FIFO[i] if loop j was vectorized away, tmp);
                } else {                                                          // Send to the consumer Func of F
                    LHS = tmp;
                }
```

### Summary of the unrolling/vectorization of loop `i` + gathering along `i`
In either case, we do the following:
+ find out the LHS and RHS `value(...)=read_channel(...)`.
+ get `resultType` from the LHS.
+ find out all unrolled loops, and from them, determine FIFOs dimensions.
+ create a temporary variable with `resultType`. This is the type of the data passed along the FIFOs and eventually to the consumer Func of `F`.

When loop `i` is unrolled, we
+ insert a count loop `t` to transfer data one by one from this PE and the previous PEs.

When loop `i` is vectorized, we
+ insert back loop `i` as unrolled, combine this and previous PEs' data in a vector, and pass to the next PE.

## Main Pseudocode

**For simplicity, we only illustrate with ** `F.gather(A, i, GatherStrategy::Up)`

Func.h:

```C++
    Func &gather(Func A, VarOrRVar loop, GatherStrategy strategy){
        assert this Func has one and only one definition, and the gather_item in the definition is invalid
        add GatherItem(A.name(), loop, strategy) in DefinitionContents
    }
```

Gather.cpp

```C++
class TestGathering : public IRVisitor{

    string caller_name;
    string gather_loop_name;
    GatherStrategy strategy;

    // we are in producer of A
    bool in_gather_func;

    void visit(const For* op) override{
        // find gather loop
        if(in_gather_func && ends_with(op->name, "." + gather_loop_name)) {
            user_assert(op->for_type == ForType::Unrolled)
            look for loop min and extent // for realize channel
            assert found min and extent
            found_loop = op; // record gather loop
        }
        IRVisitor::visit(op);
        return ;
    }

    void visit(const Call* op) override{
        if(in_gather_func && op->is_intrinsic(Call::read_channel) && channel name == func_name){
            user_assert(found_call == nullptr)
            found_call = op; // record read channel
        }
        IRVisitor::visit(op);
        return ;
    }

    // just to locate the caller
    void visit(const ProducerConsumer* op) override{
        if (op->is_producer && op->name == caller_name){
            in_gather_func = true;
            IRVisitor::visit(op);
            in_gather_func = false;
        } else {
            IRVisitor::visit(op);
        }
        return ;
    }

public:
    const Call* found_call;
    const For* found_loop;
    int loop_min;
    int loop_extent;
};

// Intermediate struct to record all the information needed in gathering.
class GatherArgInfo{
public:
    int loop_min;
    int loop_extent;
    GatherStrategy strategy;
    const Call* call_node;
    const For* gather_loop;
};

class DataGathering : public IRMutator{
    using IRMutator::visit;
    // key is caller name.
    const map<string, GatherArgInfo> &gather_arg_info;
    const map<string, Function> &env;
    string producer_name;

    Stmt visit(const For* op) override{
        if(op is gathering loop i){
            build new assistant loop t as new_body
            return For::make(op->name, op->min, op->extent, op->for_type, op->device_api, mutate(new_body));
        }

        if(in innermost loop){
            modify loop_body,see Brief description
        }
    }

    // locate callers, insert Realize
    Stmt visit(const ProducerConsumer* op) override{
        if (op->is_producer){
            producer_name = op->name;
            Stmt updated_body = mutate(op->body);
            build Realize nodes for new FIFO array as final updated_body
            producer_name = "";
            return ProducerConsumer::make(op->name, op->is_producer, updated_body);
        }
        Stmt updated_body = mutate(op->body);
        return ProducerConsumer::make(op->name, op->is_producer, updated_body);
    }

};

Stmt gather_data(Stmt s, const map<string, Function> &env){
    // get gather arguments and test, the key is caller name
    map<string, GatherArgInfo> gather_arg_info;
    for (auto e : env) {
        string caller_name = e.second.name();
        GatherItem t = e.second.definition().gather_item();
        if (t.valid) {
            TestGathering ts(caller_name, t.func_name, t.loop_name, t.strategy, env);
            s.accept(&ts);
            user_assert(ts.found_loop != nullptr)
            user_assert(ts.found_call != nullptr)
            record information from ts to gather_arg_info
        }
    }

    // gather processing
    if (!gather_arg_info.empty()){
        DataGathering ds(gather_arg_info, env);
        s = ds.mutate(s);
    }

    return s;
}
```

##### Error checking for `F.gather(A, i, strategy)`
There are currently several types of error checking, which occur in the following locations:
1. Assumption tested when `F.gather()` is invoked:
    1. Func `F` is defined, has only one definition, and has no valid GatherItem yet
2. Assumptions were tested in gather_data(), before the real processing:
    1. `read_channel(A, dimensions)` is found once and only once in Func F, and without any condition around it.
    2. All the above `dimensions` are space loops (ForType::Unroll with constant extents) and `i` is one of them.