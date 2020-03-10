include(vcpkg_common_functions)

vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH
    SOURCE_PATH
    REPO
    googleapis/google-cloud-cpp-common
    REF
    v0.22.1
    SHA512
    1ebf4929c218ba3f217562499bae10ba90d51e07414479f242b80980e7f8bc11ddab66e820a50316d7c187da23ab58d8c1bbe0cd80be1ab36a9fa384338de288
    HEAD_REF
    master)

vcpkg_configure_cmake(
    SOURCE_PATH ${SOURCE_PATH} PREFER_NINJA DISABLE_PARALLEL_CONFIGURE OPTIONS
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
