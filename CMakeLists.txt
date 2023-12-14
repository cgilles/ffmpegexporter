#
# Copyright (c) 2010-2023 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

project(ffmpegexporter)

cmake_minimum_required(VERSION "3.16")

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)


find_package(Qt5 5.15.0
             NO_MODULE COMPONENTS
             Core
             Gui
)

include(RulesFFmpeg)

add_executable(ffmpegexporter
               ffmpegexporter.cpp
)

target_link_libraries(pgfdatabase
                      Qt5::Core
                      Qt5::Gui
                      ${MEDIAPLAYER_LIBRARIES}
)
