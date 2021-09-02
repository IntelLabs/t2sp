### Design of Scattering

By Yunshan Jia (jiayunshan@pku.edu.cn)



#### Parameter Storage

We plan to use class `ScatterItem` to save args for each invocation about scatter(). ScatterItem contains func name, loopvar, and Scatterstrategy, and it will be declared in Scatter.h. A vector&lt;ScatterItem&gt; named `scatterItems` will be added to DefinitionContents as a new member variable to save those args until they get handled in scatterData() in lower.

#### Processing flow

##### Brief description

Our main job is to insert a new loop and new FIFOs to achieve data transfer in a specific direction. Specifically, the data originally transferred from producer to consumer will be all transferred to one end of a specific loop in consumer, and the rest of the transmission will be completed within this loop in a linear direction through the new FIFOs.

IR before:
```
    for outer loops
        for i = min; i < max; i++
            for inner loops
                some work1
                if (f(i))
                    some work2
                    x = read(A, i);
                    if (g(i))
                        y = read(B, i + k);
                        some work3 with y
                    z = read(C);
                    some work4 with x, z
                some work5
```

IR after:
```
    // F.scatter(A, i);
    // F.scatter(B, i);

    for outer loops
        for i = min; i < max; i++
            for t = i; t < max; t++
                for inner loops
                    declare A.temp;
                    declare B.temp;
                    // read data
                    if f(t){ // cond of A(i)
                        A.temp = select(i == min, A(t), read_fifo(FIFO_A[i - 1]))
                        if (t != i) // forward data
                            write_fifo(FIFO_A[i], A.temp);
                    }
                    if f(t) && g(t) { // cond of B(i + k)
                        B.temp = select(i == min, B(t + k), read_fifo(FIFO_B[i - 1]))
                        if (t != i) // forward data
                             write_fifo(FIFO_B[i], B.temp);
                    }
                    if (t == i){ // do the work of iteration i
                        // origin loop body begin
                        some work1
                        if (f(i))
                            some work2
                            x = A.temp;
                            if (g(i))
                                y = B.temp;
                                some work3 with y
                            z = read(C);
                            some work4 with x, z
                        some work5
                        // origin loop body end
                    }

```

And this figure can help us understand better. Assume loop i has 3 iterations unrolled into 3 PEs, and the transformed code will look like this:

<img src="https://gitlab.com/T2S/T2S/uploads/5a2850462a58c0c2c70a9953470a369d/image.png" width=350>



##### Main Pseudocode

1. **about naming: all `func` below refers to `a` in A.scatter(a, i, Up).**
2. **For simplicity, we only describe the case of Scatterstrategy::Up.**

Func.h:
```
    Func &scatter(Func f, VarOrRVar loop, ScatterStrategy strategy){
        assert this->defined();
        assert f.defined();
        scatterItmes.push_back(ScatterItems(f.name(), loop, strategy));
    }
```

Scatter.cpp
```
class ScatterTest : public IRMutator{

    // caller, name of A in A.scatter(a, i, Up)
    const string function_name;
    // other origin arguments
    string funcs_name;
    string scatter_loop_name;
    ScatterStrategy strategy;

    // we are in producer of A
    bool in_scatter_func(false);

    Stmt visit(const For* op)
        // find scatter loop
        if(in_scatter_func and op->name == scatter_loop_name)) {
            assert loop is unroll loop or serial loop
            assert unroll loop or strategy == Down
            record find loop
        }
        ...
    }

    Expr visit(const Call* op){
        if (in_scatter_func and op read from func){
            assert haven't found read node before
            record find call node
        }
        ...
    }

    // just to locate the caller
    Stmt visit(const ProducerConsumer* op){
        if (op->name == function_name and op->is_producer){
            in_scatter_func = true;
            Stmt produce = mutate(op->body);
            in_scatter_func = false;
            ...
        }
        else ...
    }

};

class DataScattering : public IRMutator{

    // caller, name of A in A.scatter(a, i, Up)
    const string function_name;
    // other origin arguments
    string funcs_name;
    string scatter_loop_name;
    ScatterStrategy strategy;
    Expr call_node;

    // we are in producer of A
    bool in_scatter_func(false);

    // found scatter loop created before
    bool have_scatter_loop(false);

    Stmt visit(const For* op)
        if(in_scatter_func){
            // find scatter loop
            if(op->name == scatter_loop_name)) {
                if (body != new scatter loop){
                    add new serial scatter loop t;
                } else {
                    have_scatter_loop = true;
                }
            }

            if(in innermost loop){
                SliceExprTree slicer(call_node->name, Expr(), "in scattering ...");
                if(!have_scatter_loop){
                    slicer.mutate(op->body);
                    record lets and condition in slicer // omitted lets in this block to simplify the description
                    // see Brief Description
                    replace call_node with x.temp in op->body(use SliceMutateExprTree or substitute)
                    make Expr new_call_node: replace i with t in call_node
                    make Expr select_read: select(i == min, new_call_node, read_FIFO[i])
                    make Stmt write_node: if t != i then write_FIFO[i]
                    make dataRW if(condtion){ x.temp = select_read, write_node}
                    make update body: dataRW, if t == i then loop body
                    get updated body
                } else {
                    /** similar to above, we can make changes based on the original stmt structure like:
                     *  declare x.temp Block;
                     *  read/write data Block;
                     *  if (t == i){
                     *      updated loop body
                     *  }
                     */
                     Decompose the original structure, perform appropriate replacement
                     and insertion operations, and reassemble again
                }
            } else ...
        } else  ...

    }

    // just to locate the caller
    Stmt visit(const ProducerConsumer* op){
        if (op->name == function_name and op->is_producer){
            in_scatter_func = true;
            Stmt produce = mutate(op->body);
            in_scatter_func = false;
            ...
        }
        else ...
    }

};

Stmt scatterData(Stmt s, const std::map<std::string, Function> &env){
    // get scatter arguments
    vector<pair(string callerName, ScatterItem)> scatterArgsPerFunc;
    for (auto e : env) {
        string callerName = e.second.name();
        vector<ScatterItem> t = e.second.definition().scatterArgs();
        for (auto item : t)
            scatterArgsPerFunc.push_back(pair(callerName, item));
    }

    // test assumptions
    vector<Expr> found_call;
    // for each unroll loop, we record the ScatterStrategy argu for the first scatter invocation,
    // if different ScatterStrategy appears later, we will throw an Error.
    map<loopvar, ScatterStrategy> single_strategy_check;
    for(auto item : scatterArgsPerFunc){
        assert only one strategy was used in item.second.loopvar (check single_strategy_check and item.second.strategy);
        TestScattering ts(item.first, item.second.loopvar, item.second.strategy, item.second.funcs);
        ts.mutate(s);
        assert find loop in ts
        assert find call node in ts
        record find call node into found_call
    }

    // scatter
    for(int i = 0; i < scatterArgsPerFunc.size(); i++){
        DataScattering ds(scatterArgsPerFunc[i].first,            // function_name
                          scatterArgsPerFunc[i].second.loop_name,
                          scatterArgsPerFunc[i].second.strategy,
                          scatterArgsPerFunc[i].second.funcs,
                          found_call[i]);
        s = ds.mutate(s);
    }

    return s;
}
```



##### Error Checking(A.scatter(func, i, strategy))
There are currently several types of error checking, which occur in the following locations:
1. assert related func was defined in function scatter().
2. other assumptions were tested in scatterData(), before the real processing:
   1. assert func was found in A.
   2. exclude cases that func appears multi times with different args.
   3. assert loop existence with available type.
   4. It's ok to scatter unroll loop with ScatterStrategy::Up or Down, but since serial loop will be compiled into something like `for(T t = min; t < max; t++)` in C and OpenCL (see [CodeGen_C.cpp](https://gitlab.com/T2S/t2s-os/-/blob/master/Halide/src/CodeGen_C.cpp)), we only allow ScatterStrategy::Up for serial loop, otherwise an error will be thrown.
   5. assert only 1 Strategy was used for all the scatters along any single loop