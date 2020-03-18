include(vcpkg_common_functions)

vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH
    SOURCE_PATH
    REPO
    googleapis/google-cloud-cpp-common
    REF
    v0.24.0
    SHA512
    f75b8b11cad5428696c95bd1ea4cc3cb05d3e8655626ff9929ac92b4d0f23d9ad42c45b58d3641fb4077279b59fc2da7b1c5f63f7d40d8098a32209b22394fd2
    HEAD_REF
    master)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS test BUILD_TESTING)

vcpkg_configure_cmake(
    SOURCE_PATH
    ${SOURCE_PATH}
    PREFER_NINJA
    DISABLE_PARALLEL_CONFIGURE
    OPTIONS
    ${FEATURE_OPTIONS}
    -DGOOGLE_CLOUD_CPP_ENABLE_MACOS_OPENSSL_CHECK=OFF)

vcpkg_install_cmake(ADD_BIN_TO_PATH)

file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/include)
vcpkg_fixup_cmake_targets(CONFIG_PATH lib/cmake TARGET_PATH share)

file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/share)
file(
    INSTALL ${SOURCE_PATH}/LICENSE
    DESTINATION ${CURRENT_PACKAGES_DIR}/share/google-cloud-cpp-common
    RENAME copyright)

vcpkg_copy_pdbs()
