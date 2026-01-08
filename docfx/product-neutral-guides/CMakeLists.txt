if (NOT GOOGLE_CLOUD_CPP_INTERNAL_DOCFX)
    return()
endif ()

set(output_dir "${PROJECT_BINARY_DIR}/product-neutral-guides/docfx")

# Define the guide files in separate lists to avoid string(SPLIT).
set(guide_target_names "getting-started")
set(guide_source_files "${PROJECT_SOURCE_DIR}/README.md")
set(guide_display_names "Getting Started")

set(copied_files "")
set(toc_items "")
list(LENGTH guide_target_names num_guides)
if(num_guides GREATER 0)
    math(EXPR last_index "${num_guides} - 1")
    foreach(i RANGE ${last_index})
        list(GET guide_target_names ${i} target_name)
        list(GET guide_source_files ${i} source_file)
        list(GET guide_display_names ${i} display_name)

        set(destination "${output_dir}/${target_name}.md")
        add_custom_command(
            OUTPUT "${destination}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${output_dir}"
            COMMAND ${CMAKE_COMMAND} -E copy "${source_file}" "${destination}"
            DEPENDS "${source_file}"
            COMMENT "Copying ${source_file} to ${destination}"
        )
        list(APPEND copied_files "${destination}")

        string(APPEND toc_items "    - name: '${display_name}'\n      href: ${target_name}.md\n")
    endforeach()
endif()

set(TOC_ITEMS ${toc_items})
set(toc_file "${output_dir}/toc.yml")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/toc.yml.in"
    "${toc_file}"
    @ONLY
)

add_custom_target(
    product-neutral-guides-docs
    DEPENDS ${copied_files}
)

add_dependencies(all-docfx product-neutral-guides-docs)
