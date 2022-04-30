SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

ADD_EXECUTABLE(demo demo.cpp ../deps/limonp/Md5.cpp)
ADD_EXECUTABLE(load_test load_test.cpp ../deps/limonp/Md5.cpp)
ADD_SUBDIRECTORY(unittest)
