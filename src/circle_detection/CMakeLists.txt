cmake_minimum_required(VERSION 3.0.2)
project(circle_detection)

## Compile as C++11, supported in ROS Kinetic and newer
# add_compile_options(-std=c++11)

## Find catkin macros and libraries
find_package(catkin REQUIRED COMPONENTS
  roscpp
  sensor_msgs
  image_transport
  cv_bridge
  message_filters
)
find_package(OpenCV REQUIRED)
## Find PCL
find_package(PCL REQUIRED)

###################################
## catkin specific configuration ##
###################################
catkin_package(
  INCLUDE_DIRS include
#  LIBRARIES circle_detection
   CATKIN_DEPENDS cv_bridge image_transport message_filters roscpp sensor_msgs
#  DEPENDS system_lib
)
# add_definitions(${PCL_DEFINITIONS}
#               ${OpenCV_DEFINITIONS})


###########
## Build ##
###########

## Specify additional locations of header files
include_directories(
  include
  ${catkin_INCLUDE_DIRS}
  ${PCL_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
  /usr/include/eigen3
  #./src/include/circle_detector.h
)

add_library(${PROJECT_NAME} SHARED
  ./src/circle_detector.cpp)

add_dependencies(${PROJECT_NAME}  ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})

target_link_libraries(${PROJECT_NAME} 
  ${catkin_LIBRARIES}
  ${PCL_LIBRARIES}
  ${OpenCV_LIBRARIES}
)
## Declare a C++ executable
add_executable(${PROJECT_NAME}_node src/main.cpp)
message(STATUS "===== PCL_DEFINITIONS: ${PCL_DEFINITIONS} =====")
message(STATUS "===== OPENCV_DEFINITIONS: ${OpenCV_DEFINITIONS} =====")
# ## Add dependencies to the executable
# add_dependencies(${PROJECT_NAME}_node  ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})

# ## Specify libraries to link a library or executable target against
target_link_libraries(${PROJECT_NAME}_node 
  ${catkin_LIBRARIES}
  ${PCL_LIBRARIES}
  ${OpenCV_LIBRARIES}
  ${PROJECT_NAME}
)

## Add PCL definitions and link directories
link_directories(${PCL_LIBRARY_DIRS})

#############
## Install ##
#############

# (Your installation commands, if any)
install(TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_GLOBAL_BIN_DESTINATION}
)

install(DIRECTORY include/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
  FILES_MATCHING PATTERN "*.h"
  PATTERN ".svn" EXCLUDE
)

#############
## Testing ##
#############

# (Your testing commands, if any)
