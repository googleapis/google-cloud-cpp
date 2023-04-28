# ~~~
# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ~~~

include(GoogleapisConfig)
set(DOXYGEN_PROJECT_NAME "Cloud Compute API C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for the Cloud Compute API")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION} (Experimental)")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/quickstart)

unset(mocks_globs)
unset(source_globs)
unset(proto_globs)
set(service_dirs
    "accelerator_types/v1/"
    "addresses/v1/"
    "autoscalers/v1/"
    "backend_buckets/v1/"
    "backend_services/v1/"
    "disks/v1/"
    "disk_types/v1/"
    "external_vpn_gateways/v1/"
    "firewall_policies/v1/"
    "firewalls/v1/"
    "forwarding_rules/v1/"
    "global_addresses/v1/"
    "global_forwarding_rules/v1/"
    "global_network_endpoint_groups/v1/"
    "global_operations/v1/"
    "global_organization_operations/v1/"
    "global_public_delegated_prefixes/v1/"
    "health_checks/v1/"
    "http_health_checks/v1/"
    "https_health_checks/v1/"
    "image_family_views/v1/"
    "images/v1/"
    "instance_group_managers/v1/"
    "instance_groups/v1/"
    "instances/v1/"
    "instance_templates/v1/"
    "interconnect_attachments/v1/"
    "interconnect_locations/v1/"
    "interconnects/v1/"
    "license_codes/v1/"
    "licenses/v1/"
    "machine_images/v1/"
    "machine_types/v1/"
    "network_attachments/v1/"
    "network_edge_security_services/v1/"
    "network_endpoint_groups/v1/"
    "network_firewall_policies/v1/"
    "networks/v1/"
    "node_groups/v1/"
    "node_templates/v1/"
    "node_types/v1/"
    "packet_mirrorings/v1/"
    "projects/v1/"
    "public_advertised_prefixes/v1/"
    "public_delegated_prefixes/v1/"
    "region_autoscalers/v1/"
    "region_backend_services/v1/"
    "region_commitments/v1/"
    "region_disks/v1/"
    "region_disk_types/v1/"
    "region_health_checks/v1/"
    "region_health_check_services/v1/"
    "region_instance_group_managers/v1/"
    "region_instance_groups/v1/"
    "region_instances/v1/"
    "region_instance_templates/v1/"
    "region_network_endpoint_groups/v1/"
    "region_network_firewall_policies/v1/"
    "region_notification_endpoints/v1/"
    "region_operations/v1/"
    "regions/v1/"
    "region_security_policies/v1/"
    "region_ssl_certificates/v1/"
    "region_ssl_policies/v1/"
    "region_target_http_proxies/v1/"
    "region_target_https_proxies/v1/"
    "region_target_tcp_proxies/v1/"
    "region_url_maps/v1/"
    "reservations/v1/"
    "resource_policies/v1/"
    "routers/v1/"
    "routes/v1/"
    "security_policies/v1/"
    "service_attachments/v1/"
    "snapshots/v1/"
    "ssl_certificates/v1/"
    "ssl_policies/v1/"
    "subnetworks/v1/"
    "target_grpc_proxies/v1/"
    "target_http_proxies/v1/"
    "target_https_proxies/v1/"
    "target_instances/v1/"
    "target_pools/v1/"
    "target_ssl_proxies/v1/"
    "target_tcp_proxies/v1/"
    "target_vpn_gateways/v1/"
    "url_maps/v1/"
    "v1/"
    "vpn_gateways/v1/"
    "vpn_tunnels/v1/"
    "zone_operations/v1/"
    "zones/v1/")
foreach (dir IN LISTS service_dirs)
    string(REPLACE "/" "_" ns "${dir}")
    list(APPEND source_globs "${dir}*.h" "${dir}*.cc" "${dir}internal/*")
    list(APPEND proto_globs "${dir}*.proto" "${dir}internal/*.proto")
    list(APPEND mocks_globs "${dir}mocks/*.h")
    list(APPEND DOXYGEN_EXAMPLE_PATH
         "${CMAKE_CURRENT_SOURCE_DIR}/${dir}samples")
    list(APPEND DOXYGEN_EXCLUDE_SYMBOLS "compute_${ns}internal")
endforeach ()

include(GoogleCloudCppDoxygen)
google_cloud_cpp_doxygen_targets("compute" DEPENDS cloud-docs
                                 google-cloud-cpp::compute_protos)

include(GoogleCloudCppCommon)

include(CompileProtos)
google_cloud_cpp_find_proto_include_dir(PROTO_INCLUDE_DIR)

# google_cloud_cpp_load_protolist( proto_list
# "${PROJECT_SOURCE_DIR}/external/googleapis/protolists/sql.list")
google_cloud_cpp_load_protodeps(
    proto_deps
    "${PROJECT_SOURCE_DIR}/external/googleapis/protodeps/compute.deps")

file(GLOB proto_files RELATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}"
     ${proto_globs})
list(SORT proto_files)

google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_compute_protos
    # cmake-format: sort
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/accelerator_types/v1/accelerator_types.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/addresses/v1/addresses.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/autoscalers/v1/autoscalers.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/backend_buckets/v1/backend_buckets.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/backend_services/v1/backend_services.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/disks/v1/disks.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/disk_types/v1/disk_types.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/external_vpn_gateways/v1/external_vpn_gateways.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/firewall_policies/v1/firewall_policies.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/firewalls/v1/firewalls.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/forwarding_rules/v1/forwarding_rules.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/global_addresses/v1/global_addresses.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/global_forwarding_rules/v1/global_forwarding_rules.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/global_network_endpoint_groups/v1/global_network_endpoint_groups.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/global_operations/v1/global_operations.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/global_organization_operations/v1/global_organization_operations.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/global_public_delegated_prefixes/v1/global_public_delegated_prefixes.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/health_checks/v1/health_checks.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/http_health_checks/v1/http_health_checks.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/https_health_checks/v1/https_health_checks.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/image_family_views/v1/image_family_views.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/images/v1/images.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/instance_group_managers/v1/instance_group_managers.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/instance_groups/v1/instance_groups.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/instances/v1/instances.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/instance_templates/v1/instance_templates.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/interconnect_attachments/v1/interconnect_attachments.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/interconnect_locations/v1/interconnect_locations.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/interconnects/v1/interconnects.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/license_codes/v1/license_codes.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/licenses/v1/licenses.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/machine_images/v1/machine_images.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/machine_types/v1/machine_types.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/network_attachments/v1/network_attachments.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/network_edge_security_services/v1/network_edge_security_services.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/network_endpoint_groups/v1/network_endpoint_groups.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/network_firewall_policies/v1/network_firewall_policies.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/networks/v1/networks.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/node_groups/v1/node_groups.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/node_templates/v1/node_templates.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/node_types/v1/node_types.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/packet_mirrorings/v1/packet_mirrorings.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/projects/v1/projects.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/public_advertised_prefixes/v1/public_advertised_prefixes.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/public_delegated_prefixes/v1/public_delegated_prefixes.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_autoscalers/v1/region_autoscalers.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_backend_services/v1/region_backend_services.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_commitments/v1/region_commitments.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_disks/v1/region_disks.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_disk_types/v1/region_disk_types.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_health_check_services/v1/region_health_check_services.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_health_checks/v1/region_health_checks.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_instance_group_managers/v1/region_instance_group_managers.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_instance_groups/v1/region_instance_groups.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_instances/v1/region_instances.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_instance_templates/v1/region_instance_templates.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_network_endpoint_groups/v1/region_network_endpoint_groups.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_network_firewall_policies/v1/region_network_firewall_policies.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_notification_endpoints/v1/region_notification_endpoints.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_operations/v1/region_operations.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_security_policies/v1/region_security_policies.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_ssl_certificates/v1/region_ssl_certificates.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_ssl_policies/v1/region_ssl_policies.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/regions/v1/regions.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_target_http_proxies/v1/region_target_http_proxies.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_target_https_proxies/v1/region_target_https_proxies.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_target_tcp_proxies/v1/region_target_tcp_proxies.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/region_url_maps/v1/region_url_maps.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/reservations/v1/reservations.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/resource_policies/v1/resource_policies.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/routers/v1/routers.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/routes/v1/routes.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/security_policies/v1/security_policies.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/service_attachments/v1/service_attachments.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/snapshots/v1/snapshots.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/ssl_certificates/v1/ssl_certificates.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/ssl_policies/v1/ssl_policies.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/subnetworks/v1/subnetworks.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/target_grpc_proxies/v1/target_grpc_proxies.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/target_http_proxies/v1/target_http_proxies.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/target_https_proxies/v1/target_https_proxies.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/target_instances/v1/target_instances.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/target_pools/v1/target_pools.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/target_ssl_proxies/v1/target_ssl_proxies.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/target_tcp_proxies/v1/target_tcp_proxies.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/target_vpn_gateways/v1/target_vpn_gateways.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/url_maps/v1/url_maps.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/v1/internal/common.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/vpn_gateways/v1/vpn_gateways.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/vpn_tunnels/v1/vpn_tunnels.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/zone_operations/v1/zone_operations.proto
    ${PROJECT_SOURCE_DIR}/google/cloud/compute/zones/v1/zones.proto
    PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}"
    "${PROJECT_SOURCE_DIR}")
external_googleapis_set_version_and_alias(compute_protos)
target_link_libraries(google_cloud_cpp_compute_protos PUBLIC ${proto_deps})

file(
    GLOB source_files
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    ${source_globs})
list(SORT source_files)
add_library(google_cloud_cpp_compute ${source_files})
target_include_directories(
    google_cloud_cpp_compute
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_compute
    PUBLIC google-cloud-cpp::grpc_utils
           google-cloud-cpp::common
           google-cloud-cpp::rest_internal
           google-cloud-cpp::rest_protobuf_internal
           google-cloud-cpp::compute_protos)
google_cloud_cpp_add_common_options(google_cloud_cpp_compute)
set_target_properties(
    google_cloud_cpp_compute
    PROPERTIES EXPORT_NAME google-cloud-cpp::experimental-compute
               VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_compute
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::experimental-compute ALIAS
            google_cloud_cpp_compute)

# Create a header-only library for the mocks. We use a CMake `INTERFACE` library
# for these, a regular library would not work on macOS (where the library needs
# at least one .o file). Unfortunately INTERFACE libraries are a bit weird in
# that they need absolute paths for their sources.
file(
    GLOB relative_mock_files
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    ${mocks_globs})
list(SORT relative_mock_files)
set(mock_files)
foreach (file IN LISTS relative_mock_files)
    list(APPEND mock_files "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
endforeach ()
add_library(google_cloud_cpp_compute_mocks INTERFACE)
target_sources(google_cloud_cpp_compute_mocks INTERFACE ${mock_files})
target_link_libraries(
    google_cloud_cpp_compute_mocks
    INTERFACE google-cloud-cpp::experimental-compute GTest::gmock_main
              GTest::gmock GTest::gtest)
set_target_properties(
    google_cloud_cpp_compute_mocks
    PROPERTIES EXPORT_NAME google-cloud-cpp::experimental-compute_mocks)
target_include_directories(
    google_cloud_cpp_compute_mocks
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
              $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_compute_mocks
                       INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

include(CTest)
if (BUILD_TESTING)
    add_executable(compute_quickstart "quickstart/quickstart.cc")
    target_link_libraries(compute_quickstart
                          PRIVATE google-cloud-cpp::experimental-compute)
    google_cloud_cpp_add_common_options(compute_quickstart)
    add_test(
        NAME compute_quickstart
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:compute_quickstart> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_TEST_ZONE)
    set_tests_properties(compute_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_compute-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_compute"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_compute google_cloud_cpp_compute_protos
    EXPORT google_cloud_cpp_compute-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_SKIP
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)
# With CMake-3.12 and higher we could avoid this separate command (and the
# duplication).
install(
    TARGETS google_cloud_cpp_compute google_cloud_cpp_compute_protos
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_proto_library_protos("google_cloud_cpp_compute_protos"
                                              "${PROJECT_SOURCE_DIR}")
google_cloud_cpp_install_proto_library_headers(
    "google_cloud_cpp_compute_protos")
google_cloud_cpp_install_headers("google_cloud_cpp_compute"
                                 "include/google/cloud/compute")
google_cloud_cpp_install_headers("google_cloud_cpp_compute_mocks"
                                 "include/google/cloud/compute")

google_cloud_cpp_add_pkgconfig(
    compute
    "The Cloud Compute API C++ Client Library"
    "Provides C++ APIs to use the Cloud Compute API."
    "google_cloud_cpp_rest_internal"
    " google_cloud_cpp_rest_protobuf_internal"
    " google_cloud_cpp_grpc_utils"
    " google_cloud_cpp_common"
    " google_cloud_cpp_compute_protos")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_compute-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_compute-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_compute-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_compute-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_compute"
    COMPONENT google_cloud_cpp_development)

external_googleapis_install_pc("google_cloud_cpp_compute_protos")

# google-cloud-cpp::sql must be defined before we can add the samples.
foreach (dir IN LISTS service_dirs)
    if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
        google_cloud_cpp_add_samples_relative("compute" "${dir}samples/")
    endif ()
endforeach ()
