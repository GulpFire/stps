cmake_minimum_required(VERSION 3.0)
project(hello_world)

add_executable(${PROJECT_NAME}_publisher ${PROJECT_NAME}_publisher/${PROJECT_NAME}_publisher.cc)
target_compile_features(${PROJECT_NAME}_publisher PUBLIC cxx_std_17)
target_link_libraries(${PROJECT_NAME}_publisher PUBLIC stps)
target_compile_options(${PROJECT_NAME}_publisher PUBLIC -Wall -Wextra -pipe)

add_executable(${PROJECT_NAME}_subscriber ${PROJECT_NAME}_subscriber/${PROJECT_NAME}_subscriber.cc)
target_compile_features(${PROJECT_NAME}_subscriber PUBLIC cxx_std_17)
target_link_libraries(${PROJECT_NAME}_subscriber PUBLIC stps)
target_compile_options(${PROJECT_NAME}_subscriber PUBLIC -Wall -Wextra -pipe)
