add_subdirectory(virtual_http_file)

add_executable(http_pump main.cpp)

target_link_libraries(http_pump 
    virtual_http_file
    httplib 
)