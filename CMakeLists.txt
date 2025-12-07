# CMakeLists.txt (RAIZ DO PROJETO)
cmake_minimum_required(VERSION 3.5)

# O arquivo correto é project.cmake (com 'c'), e não project.make
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(oneshot_read)