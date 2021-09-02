# Test script
test.sh: runs each unit test with specified places. Prints success/failure stats.

# Isolate 1 operand from a single Func definition into 1 producer
1. isolate-producer-a-A.cpp: isolate a from A(i) = a(i)
2. isolate-producer-a-A-2.cpp: isolate a from A(i) = a(i) * 2
3. isolate-producer-a-A-3-negative.cpp: isolate a from A(i) = a(i) * a(i+1). Multiple occurances of a
4. isolate-producer-a-A-15-negative.cpp: isolate nothing.
5. isolate-producer-a-A-16-negative.cpp: isolate into no producer.
6. isolate-producer-a-A-17-negative.cpp: isolate an operand from a Func that does not contain such an operand 
7. isolate-producer-a-A-19-negative.cpp: isolate an operand into an already-defined producer

# Isolate 1 operand from a single Func definition into more than 1 producer
1. isolate-producer-a-A-4.cpp: isolate a from A(i) = a(i) * 2 into 2 producers
2. isolate-producer-a-A-5.cpp: isolate a from A(i) = a(i) * 2 into 4 producers

# Isolate more than 1 operand from a single Func definition into 1 producer
1. isolate-producer-a-A-6.cpp: isolate a and b from A(i) = a(i) * b(i) into 1 producer

# Isolate more than 1 operand from a single Func definition into more than 1 producer
1. isolate-producer-a-A-7.cpp: isolate a and b from A(i) = a(i) * b(i) into 4 producers
2. isolate-producer-a-A-8.cpp: isolate a and b from A(i) = a(i) * b(i) into 4 prodcers, further isolate a to another 2 producers

# Isolate 1 conditioned operand from a single Func definition into 1 producer
1. isolate-producer-a-A-condition-9.cpp: isolate a from A(i) = select(i <= 2, a(i), i * 2) * 2 into 1 producer
2. isolate-producer-a-A-condition-10.cpp: isolate a from A(i) = select(i <= 2, a(i) + 1, i * 2) * 2 into 1 producer

# Isolate 1 operand from merged UREs into 1 producer
1. isolate-producer-a-A-condition-merge-ures-11.cpp: isolate an operand from Func A, which is merged after Func B, into 1 producer
2. isolate-producer-a-A-condition-merge-ures-12.cpp: isolate an operand from Func A, which is merged after Func C and B, into 1 producer
3. isolate-producer-a-A-condition-merge-ures-negative-13.cpp: isolate the condition of a Select.
4. isolate-producer-a-A-condition-merge-ures-negative-14.cpp: isolate an operand inside the condition of a Select.
5. isolate-producer-a-A-condition-merge-ures-17.cpp: isolate an operand from a merged Func.

# Isolate 1 operand with dependees from merged UREs 
1. isolate-producer-a-A-condition-merge-ures-15.cpp: isolate an operand with 1 dependee into 1 producer
2. isolate-producer-a-A-condition-merge-ures-16.cpp: isolate an operand with 2 dependees into 1 producer
3. isolate-producer-a-A-condition-merge-ures-18.cpp: isolate an operand in a merged Func with 2 dependees into 1 producer
4. isolate-producer-a-A-condition-merge-ures-negative-18.cpp: isolate an operand from a Func that is cyclically dependent on another Func.

# Isolate 1 output from a simple Func
1. isolate-consumer-1.cpp: isolate the result of A(i)=i*2 to 1 consumer
2. isolate-consumer-2.cpp: isolate the result of A(i)=i*2 to multiple consumers

# Isolate 1 output from merged UREs
1. isolate-consumer-3-merge-ures.cpp: isolate the result of out(i) = A(i) = select(i == 0, i, A(i - 1)) to 1 consumer
2. isolate-consumer-4-merge-ures.cpp: isolate the result of out(i) = A(i) = select(i == 0, i, A(i - 1)) to multiple consumers

# TODO: Isolate 2 outputs with the same dimensions from merged UREs
Reference: Halide/test/correctness/multiple_outputs.cpp

# TODO: Isolate 2 outputs with different dimensions from merged UREs

