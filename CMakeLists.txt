cmake_minimum_required(VERSION 3.9)

# Set global property (all targets are impacted)
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")

# set(CMAKE_C_COMPILER               clang)
# set(CMAKE_CXX_COMPILER             clang++)
# set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "/usr/bin/iwyu;-Xiwyu;any;-Xiwyu;iwyu;-Xiwyu;args")

project(shirabedevelopment)

add_subdirectory(shirabeengine)
        
