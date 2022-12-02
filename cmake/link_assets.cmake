# Sets up asset folder for target. Please make sure that a "project(<name>)" call was made before this function call
function(Link_Assets asset_folder)
    if (EMSCRIPTEN) # Add preloaded file folder to emscripten build
        set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS
                "--preload-file ${CMAKE_CURRENT_BINARY_DIR}/${asset_folder}@${assetFolder}")
    endif()

    # Get the appropriate target directory
    if (WIN32)
        set(ProjectBinaryDir ${CMAKE_BINARY_DIR})
    else()
        set(ProjectBinaryDir ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    # Copy symlink if non-existent
    if (NOT EXISTS ${ProjectBinaryDir}/${asset_folder})
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
                # source
                ${CMAKE_CURRENT_SOURCE_DIR}/${asset_folder}
                # target
                ${ProjectBinaryDir}/${asset_folder})
    endif()
endfunction()
