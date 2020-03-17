include(vcpkg_common_functions)

vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH
    SOURCE_PATH
    REPO
    googleapis/google-cloud-cpp-common
    REF
    v0.23.0
    SHA512
    76b610b8d345f49c101b7de885287df6289f5f4349c59b5c38683fc299c5cbdb7b763700cdc3e75460802bf2da36f1b75e8ff5e660a622451d23523f34e5a001
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
