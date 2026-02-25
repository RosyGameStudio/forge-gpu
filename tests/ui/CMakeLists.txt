add_executable(test_ui test_ui.c)
target_include_directories(test_ui PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(test_ui PRIVATE SDL3::SDL3)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET test_ui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:test_ui>
    )
endif()

# Add as a CTest test
add_test(NAME ui_ttf_parser COMMAND test_ui)
