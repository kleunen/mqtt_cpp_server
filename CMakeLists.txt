cmake_minimum_required(VERSION 3.15)
project(MQTTSubscription)

set(CMAKE_CXX_STANDARD 17)

set(BOOST_ROOT "C:/local/boost_1_69_0" )
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED)

if (CMAKE_COMPILER_IS_MINGW)
   # Note: new - fixes "file too big"
   add_compile_options(-D_GLIBCXX_DEBUG -Wall -Og)
endif (CMAKE_COMPILER_IS_MINGW)

include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/mqtt_cpp/include SYSTEM ${Boost_INCLUDE_DIRS})
add_executable(MQTTSubscription main.cpp subscription_map.h retained_topic_map.h path_tokenizer.h precomp.h)
add_executable(MQTTSubscriptionTest main_test.cpp subscription_map.h)

if(WIN32)
 target_link_libraries(MQTTSubscriptionTest wsock32 ws2_32)
 target_link_libraries(MQTTSubscription wsock32 ws2_32)
endif()