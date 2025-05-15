# Copyright (c) Direct Drive Technology Co., Ltd. All rights reserved.
# Author: Zhibin Wu <zhibin.wu@directdrivetech.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.20.0)

set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Core/inc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/User/inc)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(openAMP_project)

include(${CMAKE_CURRENT_LIST_DIR}/SoftwareVersion/version.cmake)

file(GLOB USER_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/User/src/*.c ${CMAKE_CURRENT_SOURCE_DIR}/User/src/*.cpp)
file(GLOB CORE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Core/src/*.c ${CMAKE_CURRENT_SOURCE_DIR}/Core/src/*.cpp)

set(AHRS_SOURCES
#     lib/madgwick/ahrs_madgwick.c
#     lib/madgwick/FusionAhrs.c
#     lib/madgwick/FusionCompass.c
#     lib/madgwick/ahrs_madgwick.c
#     lib/madgwick/FusionOffset.c
)
# include_directories(lib/madgwick)

target_compile_definitions(app PRIVATE _GNU_SOURCE=1 DIABLO_ZEPHYR=1)
target_sources(app PRIVATE ${USER_SOURCES} ${CORE_SOURCES} ${AHRS_SOURCES} ${LIBMETAL_INCLUDE_DIR} ${OPENAMP_INCLUDE_DIR} ${PLATFORM_DIR})

# zephyr_code_relocate(FILES User/src/main.cpp LOCATION MCUSRAM1 NOCOPY NOKEEP)

zephyr_compile_options(-Werror -Wno-unused-function)
