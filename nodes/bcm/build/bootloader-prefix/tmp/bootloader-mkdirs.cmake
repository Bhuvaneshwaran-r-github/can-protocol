# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/aumraj05/tools/esp/esp-idf/components/bootloader/subproject"
  "/mnt/bhuvi/Projects/rpi/can-protocol/test4/nodes/bcm/build/bootloader"
  "/mnt/bhuvi/Projects/rpi/can-protocol/test4/nodes/bcm/build/bootloader-prefix"
  "/mnt/bhuvi/Projects/rpi/can-protocol/test4/nodes/bcm/build/bootloader-prefix/tmp"
  "/mnt/bhuvi/Projects/rpi/can-protocol/test4/nodes/bcm/build/bootloader-prefix/src/bootloader-stamp"
  "/mnt/bhuvi/Projects/rpi/can-protocol/test4/nodes/bcm/build/bootloader-prefix/src"
  "/mnt/bhuvi/Projects/rpi/can-protocol/test4/nodes/bcm/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/mnt/bhuvi/Projects/rpi/can-protocol/test4/nodes/bcm/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/mnt/bhuvi/Projects/rpi/can-protocol/test4/nodes/bcm/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
