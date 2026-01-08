if (NOT GOOGLE_CLOUD_CPP_INTERNAL_DOCFX)
    return()
endif ()

find_package(Perl REQUIRED)
set(output_dir "${PROJECT_BINARY_DIR}/product-neutral-guides/docfx")

# Define the guide files in separate lists to avoid string(SPLIT).
set(guide_target_names
    "getting-started"
    "core-concepts"
    "client-configuration"
    "occ"
    "troubleshooting"
)
set(guide_source_files
    "${PROJECT_SOURCE_DIR}/README.md"
    "${PROJECT_SOURCE_DIR}/doc/product-neutral-guides/core-concepts.md"
    "${PROJECT_SOURCE_DIR}/doc/product-neutral-guides/client-configuration.md"
    "${PROJECT_SOURCE_DIR}/doc/product-neutral-guides/occ.md"
    "${PROJECT_SOURCE_DIR}/doc/product-neutral-guides/troubleshooting.md"
)
set(guide_display_names
    "Getting Started"
    "Core Concepts"
    "Client Configuration"
    "Optimistic Concurrency Control (OCC)"
    "Troubleshooting"
)

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
        set(url_prefix "https://github.com/googleapis/google-cloud-cpp/blob/main")
        # Use the perl regex. The single quotes around the s|...|g
        # expression are for the shell, which passes it to perl.
        set(perl_command "${PERL_EXECUTABLE} -pe 's|\\[(.*?)\\]\\((/?)([^:h#][^)]*)\\)|[\$1](${url_prefix}/\$3)|g'")
        add_custom_command(
            OUTPUT "${destination}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${output_dir}"
            COMMAND sh -c "${perl_command} '${source_file}' > '${destination}'"
            DEPENDS "${source_file}"
            COMMENT "Copying and transforming ${source_file} to ${destination}"
            VERBATIM
        )
        list(APPEND copied_files "${destination}")

        string(REPLACE "'" "''" escaped_display_name "${display_name}")
        string(APPEND toc_items "    - name: '${escaped_display_name}'\n      href: ${target_name}.md\n")
    endforeach()
endif()

set(TOC_ITEMS ${toc_items})
set(toc_file "${output_dir}/toc.yml")
set(configured_toc_in "${PROJECT_BINARY_DIR}/product-neutral-guides/toc.yml.configured")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/toc.yml.in"
    "${configured_toc_in}"
    @ONLY
)

# At build time, copy the configured file to its final destination. This command
# depends on the markdown files, ensuring the correct build order.
add_custom_command(
    OUTPUT "${toc_file}"
    COMMAND ${CMAKE_COMMAND} -E copy "${configured_toc_in}" "${toc_file}"
    DEPENDS ${copied_files}
    MAIN_DEPENDENCY "${configured_toc_in}"
    COMMENT "Generating table of contents"
)

add_custom_target(
    product-neutral-guides-docs
    DEPENDS ${copied_files} "${toc_file}"
)

add_dependencies(all-docfx product-neutral-guides-docs)
