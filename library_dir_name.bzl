def google_cloud_cpp_library_dir_name(library):
    if library.startswith("compute_"):
        return "compute"
    return library
