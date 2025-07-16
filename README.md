# Thesis Project – Data Flow Analysis in the OMPi Compiler
This repository contains the implementation of my thesis work, which focuses on data flow analysis within the OMPi compiler.

# Overview
Data flow analysis is a fundamental technique used in compiler design to track how variables and expressions (the data of a program) are defined and used throughout a program. Its primary goal is to optimize code and detect potential issues during compilation.

In this project, I implemented a data flow analysis mechanism inside the OMPi compiler. The analysis was built using the internal data structures of OMPi, ensuring full integration with its architecture.

`dfa.c` and `dfa.h` contain the source code for my data flow analysis implementation. The `tests` folder includes a test case along with the results produced by my mechanism for that problem.
