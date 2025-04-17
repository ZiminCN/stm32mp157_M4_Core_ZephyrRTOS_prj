#!bin/bash

clang-format -i Core/inc/*.h*
clang-format -i Core/src/*.c*

clang-format -i User/inc/*.h*
clang-format -i User/src/*.c*