# Test Case – Basic example

## Purpose
This test evaluates how the data flow analysis mechanism handles two fundamental issues:
1. `firstprivate` variables should **not be written** before being read.
2. `private` variables should **not be read** before being written.

## Test Description
The image `Test_case.png` shows a simple C program where variable `a` is declared in a firstprivate clause and is written before it is read, which is incorrect. Similarly, variable `b` is declared in a private clause and is read before it is written, which also violates expected behavior.


##  Workflow of the Mechanism
The analysis process begins at the root of the Abstract Syntax Tree (AST) or the entry node of the Control Flow Graph (CFG) and recursively traverses all connected nodes. During this traversal, when a node representing an expression, declaration, or statement is encountered, specialized callback functions are triggered to handle the necessary processing. These functions are responsible for constructing the data sets required to solve the specific analysis problem.

## Results

The image `Results.png` effectively illustrates the problem and presents the solution in a user-friendly manner.
