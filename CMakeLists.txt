cmake_minimum_required(VERSION 3.5)
project(stps)

set(CARES_STATIC ON CACHE INTERNAL "")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/modules)

find_package(asio REQUIRED 1.24)
find_package(recycle REQUIRED)

set(STPS_SOURCE_FILES
    stps/callback_data.h
    stps/protocol_handshake_message.h
    stps/tcp_header.h

    stps/executor/executor.h
    stps/executor/executor.cc
    stps/executor/executor_impl.h
    stps/executor/executor_impl.cc
 
    stps/subscriber/subscriber_impl.h
    stps/subscriber/subscriber_impl.cc
    stps/subscriber/subscriber_session_impl.h
    stps/subscriber/subscriber_session_impl.cc
    stps/subscriber/subscriber_session.h
    stps/subscriber/subscriber_session.cc
    stps/subscriber/subscriber.h
    stps/subscriber/subscriber.cc


    stps/publisher/publisher_session.h
    stps/publisher/publisher_session.cc
    stps/publisher/publisher_impl.h
    stps/publisher/publisher_impl.cc
    stps/publisher/publisher.h
    stps/publisher/publisher.cc
   
       
       )

add_subdirectory(thirdparty/recycle EXCLUDE_FROM_ALL)

add_library(${PROJECT_NAME} STATIC ${STPS_SOURCE_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_compile_definitions(${PROJECT_NAME} PRIVATE ASIO_STANDALONE ASIO_DISABLE_VISIBILITY)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)
target_link_libraries(${PROJECT_NAME} PUBLIC pthread asio recycle)
target_compile_options(${PROJECT_NAME} PUBLIC -Wall -Wextra -pipe)

add_subdirectory(examples)
