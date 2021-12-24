add_library(virtual_http_file "")

target_link_libraries(virtual_http_file 
    httplib 
    cppf
)

target_sources(virtual_http_file
  PRIVATE
    http_downloader.cpp
    virtual_http_file.cpp
  )

target_include_directories(virtual_http_file
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
  )