### Design of Flattening

By Brendan Sullivan (bzs5@cornell.edu), Nitish Srivastava (nks45@cornell.edu)

#### Helper Functions

The functions bool isPowerOfTwo(int n) and nextPowerOf2(unsigned int n) are used throughout, since flattening creates a loop whose extent is a power of two.  Even if the sub-loops are not powers of two, we still allocate them as the next power of two, then correct for this.  

#### Constant vs Dynamic vs Merging

Flattening works for loops with either constant and dynamic or dynamic extents, except they are handled differently.  Const is handled first, then dynamic, then both are merged.  Merging is what puts the loops all back together, compiling the list made by the flattening.  

Both of these classes are called from helper functions, ConstLoopFlattening and DynamicLoopFlattening.  These classes allocate space for the loop variables.  They also check that the device_api is OpenCl, and for dynamic loops, rename the loop variables.  

##### Overview of Constant
FlattenConstLoops begins by defining several variables:
non_pow2_loops is a vector of the names of the said loops
non_pow2_loops_extents contains their extents
non_pow2_loops_level contains their levels  

FlattenConstLoops takes one initial loop the outermost loop, changing its extent to be a power of two if it isn't already.  
This loop must be a constant serial loop.  
```
if (!isPowerOfTwo(final_extent)) {
    			some_non_pow2 = true;
    			non_pow2_loops.push_back(op->name);
    			non_pow2_loops_extents.push_back(op->extent.as<IntImm>()->value);
    			non_pow2_loops_level.push_back(0);
    			final_extent = static_cast<int>(nextPowerOf2(static_cast<unsigned int>(final_extent)));
    			debug(2) << "Converted " << op->extent.as<IntImm>()->value << " to " << final_extent << "\n";
    		}

```
If the loop has a constant extent, it is pushed_back onto flattened_loops, with its extent on extents_prod_top_to_bot.  Extents_prod_top_to_bot contains extents from every loop level after multiplying that loops extent by the product of the prior ones.  Then we loop through the lower levels via accessing them through changing the pointer:
```
cop = cop->body.as<For>();
```
Now flattened_loops should have every loop with a constant extent.  At this point the body is mutated again:
```
// At this point the body is not a For loop, but however, it can consist of for loops, so mutate the body
    		body = mutate(body);
```
Now if there have been any non-power of two loops, we need to add to the end:
```
provide loop_name._no_loop_counter.temp(0) = call loop_name._no_loop_counter.temp(0) + 1
```
Next comes the correction in the extents with the non-power of two loops.  For each, we need to add the product of the offset (the difference between the original extent and the next power of two) times the product of all the extents of the loops inside it.  Since there is only one loop after flattening, we check for a certain number of bits to all be one.  

At this point, all the loops are remade as let statements, getting the value back from the flattened part.  We remake the original loop with extent final_extent, the product of all the extents.  


##### Overview of Dynamic
Dynamic begins in the same way as constant.  However, there are small changes, for example flattened_loops is now a vector of DynamicForContainer_t.  

When iterating through the inner loops, we no longer are able to check if they are powers of two, so we don't worry about this step.  A major change happens at the end, when we need to find a new way to retrieve the variables.  This is accomplished by the following code:
```
for (size_t i = 0; i < flattened_loops.size(); i++) {
    			Expr cond = EQ::make(Call::make(Int(32), flattened_loops[i].name + ".temp", {IntImm::make(Int(32), 0)}, Call::Intrinsic),
    					flattened_loops[i].extent);
    			Stmt reinit = Provide::make(flattened_loops[i].name + ".temp", {IntImm::make(Int(32),0)}, {IntImm::make(Int(32), 0)});
    			if (i!=0) {
    				s = Block::make(reinit, s);
    			} else {
    				s = reinit;
    			}
    			s = IfThenElse::make(cond, s, Stmt());
    			Stmt increment = Provide::make(flattened_loops[i].name + ".temp",  {Add::make(
    					                Call::make(Int(32), flattened_loops[i].name + ".temp", {IntImm::make(Int(32), 0)}, Call::Intrinsic),
    			                        IntImm::make(Int(32),1))}, {IntImm::make(Int(32), 0)});
    			s = Block::make(increment, s);
    		}
```
 The code iterates through the loops from outermost to innermost.  Cond checks if the loop variable equals the extent.  Reinit will re-initialize the loop, and for the outermost loop, and the first runthrough of the loop in this code segment, the statement s is initiailized to be such a statement.  The IfThenElse then combines the two, with no else condition, and an incrementation of the variable is put before it.  After the first iteration, the code looks like this in pseudocode:
 ```
Outer variable += 1
if (outer variable = outer extent)
    re-initialize outer variable

 ```
 This code segment is wrapped by the loop to the inside of it, which makes it look like this:

 ```
inner variable += 1
if (inner variable = inner extent)
    Re-initialize inner variable
    Outer variable += 1
    if (outer loop variable = outer extent)
        re-initialize outer loop variable

 ```
 This pattern continues, which allows us to use the loop variables within the loop.  This block is put after the loop body in the remade loop.  
##### Overview of Merging
This is where the merging of the loop bodies occurs.  We travel through the loops, seeing if there are non-for statements before the loop enclosed.  If so, we add them to non_for_stmts_after_a_loop.  Otherwise, we record the mins, maxes, extents, bodies, and names for the loops.  The bodies are mutated first.  We now have three cases for each level: there are either zero, one, or two+ loops.  For zero, we trivially return the body.  For one loop, we put the first statements, non_for_stmts_after_a_loop[0], before the inner loop block, then the non_for_stmts_after_a_loop[1] after.  

Two or more parallel loops is very complicated.  The code generates a series of if-else statements that merge the loop bodies together.  The last one is treated differently as it is the base case of the recursive generation of the if-else tree.  

##### Notes
