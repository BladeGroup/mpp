# vim: syntax=cmake
# ----------------------------------------------------------------------------
# mpp built-in unit test case
# ----------------------------------------------------------------------------
# macro for adding mpp sub-module unit test
macro(add_mpp_test module)
    set(test_name ${module}_test)
    string(TOUPPER ${test_name} test_tag)
    #message(STATUS "moduule   : ${module}")
    #message(STATUS "test_name : ${test_name}")
    #message(STATUS "test_tag  : ${test_tag}")

    option(${test_tag} "Build mpp ${module} unit test" ON)
    if(${test_tag})
        add_executable(${test_name} ${test_name}.c mpp_event_trigger.c mpp_parse_cfg.c)
        target_link_libraries(${test_name} rockchip_mpp utils)
        set_target_properties(${test_name} PROPERTIES FOLDER "test")
        #install(TARGETS ${test_name} RUNTIME DESTINATION ${TEST_INSTALL_DIR})
        #add_test(NAME ${test_name} COMMAND ${test_name})
    endif()
endmacro()

pkg_search_module(LIBDRM REQUIRED libdrm)
pkg_search_module(X11 REQUIRED x11)
pkg_search_module(X11XCB REQUIRED x11-xcb)
pkg_search_module(DRI2 REQUIRED xcb-dri2)

# mpi unit test
add_mpp_test(mpi)

# mpi decoder unit test
add_mpp_test(mpi_dec)
target_sources(mpi_dec_test PUBLIC drm-utils.c)
target_include_directories(mpi_dec_test PUBLIC
  ${LIBDRM_INCLUDE_DIRS} ${X11_INCLUDE_DIRS} ${X11XCB_INCLUDE_DIRS} ${DRI2_INCLUDE_DIRS})
target_link_libraries(mpi_dec_test
  ${LIBDRM_LIBRARIES} ${X11_LIBRARIES} ${X11XCB_LIBRARIES} ${DRI2_LIBRARIES})

# mpi encoder unit test
add_mpp_test(mpi_enc)

# mpi rc unit test
add_mpp_test(mpi_rc)

# new mpi rc unit test
add_mpp_test(mpi_rc2)

macro(add_legacy_test module)
    set(test_name ${module}_test)
    string(TOUPPER ${test_name} test_tag)
    #message(STATUS "moduule   : ${module}")
    #message(STATUS "test_name : ${test_name}")
    #message(STATUS "test_tag  : ${test_tag}")

    option(${test_tag} "Build legacy ${module} unit test" ON)
    if(${test_tag})
        add_executable(${test_name} ${test_name}.c)
        target_link_libraries(${test_name} rockchip_vpu utils)
        set_target_properties(${test_name} PROPERTIES FOLDER "test")
        #install(TARGETS ${test_name} RUNTIME DESTINATION ${TEST_INSTALL_DIR})
        #add_test(NAME ${test_name} COMMAND ${test_name})
    endif()
endmacro()

# legacy vpu_api unit test
add_legacy_test(vpu_api)
