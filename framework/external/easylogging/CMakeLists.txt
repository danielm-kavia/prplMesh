project(elpp VERSION ${prplmesh_VERSION})

set(ELPP_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_library(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/easylogging++.cc)

# GCC 13 started diagnosing some patterns in easylogging++ with -Wstringop-overflow.
# prplMesh builds with -Werror globally, so avoid turning this specific warning into an error
# for the easylogging++ target only.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-error=stringop-overflow)
endif()

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        ELPP_THREAD_SAFE
        ELPP_FORCE_USE_STD_THREAD
        ELPP_NO_DEFAULT_LOG_FILE
        ELPP_STL_LOGGING
        ELPP_LOG_UNORDERED_MAP
        ELPP_LOG_UNORDERED_SET
        ELPP_SYSLOG
    PRIVATE
        AUTO_INITIALIZE_EASYLOGGINGPP
    )

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${prplmesh_VERSION} SOVERSION ${prplmesh_VERSION_MAJOR})
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    )

install(TARGETS ${PROJECT_NAME} EXPORT elppConfig
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/easylogging++.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT elppConfig DESTINATION lib/cmake/${PROJECT_NAME})
