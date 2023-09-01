add_executable(generate_profiles generate_profiles.cpp)
target_link_libraries(generate_profiles powerdevilcore)
ecm_mark_as_test(generate_profiles)

function (add_profilegenerator_test)
    set(options MOBILE CANNOT_SUSPEND_TO_RAM CANNOT_SUSPEND_TO_DISK)
    set(oneValueArgs NAME EXPECTED_PROFILESRC)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "" ${ARGN})

    set(expected_profilesrc "${CMAKE_CURRENT_SOURCE_DIR}/${ARG_EXPECTED_PROFILESRC}")
    set(out_profilesrc "${CMAKE_CURRENT_BINARY_DIR}/${ARG_EXPECTED_PROFILESRC}")
    set(helper_args "")

    if (ARG_MOBILE)
        set(helper_args "${helper_args} --mobile")
    endif()
    if (ARG_CANNOT_SUSPEND_TO_RAM)
        set(helper_args "${helper_args} --cannot-suspend-to-ram")
    endif()

    add_test(
        NAME ${ARG_NAME}
        COMMAND bash -c "$<TARGET_FILE:generate_profiles> ${helper_args} \"${out_profilesrc}\" && echo \"powermanagementprofilesrc diff (should be empty):\" && diff -u \"${expected_profilesrc}\" \"${out_profilesrc}\""
    )
endfunction()

add_profilegenerator_test(
    NAME profilegenerator_pc_can_suspend_to_ram
    EXPECTED_PROFILESRC default_powermanagementprofilesrc_pc_can_suspend_to_ram
)

add_profilegenerator_test(
    NAME profilegenerator_pc_cannot_suspend_to_ram
    EXPECTED_PROFILESRC default_powermanagementprofilesrc_pc_cannot_suspend_to_ram
    CANNOT_SUSPEND_TO_RAM
)

add_profilegenerator_test(
    NAME profilegenerator_mobile_can_suspend_to_ram
    EXPECTED_PROFILESRC default_powermanagementprofilesrc_mobile_can_suspend_to_ram
    MOBILE
)
