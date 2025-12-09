# Use stable GNOME release tarballs instead of GitLab-generated archives
# GitLab keeps regenerating tarballs with different hashes (vcpkg issue #47992)
# download.gnome.org tarballs are uploaded once and never regenerated
vcpkg_download_distfile(ARCHIVE
    URLS "https://download.gnome.org/sources/atk/2.38/atk-2.38.0.tar.xz"
         "https://ftp.acc.umu.se/pub/gnome/sources/atk/2.38/atk-2.38.0.tar.xz"
    FILENAME "atk-2.38.0.tar.xz"
    SHA512 dffd0a0814a9183027c38a985d86cb6544858e9e7d655843e153440467957d6bc1abd9c9479a57078aea018053410438a30a9befb7414dc79020b223cd2c774b
)

vcpkg_extract_source_archive(SOURCE_PATH
    ARCHIVE "${ARCHIVE}"
)

if("introspection" IN_LIST FEATURES)
    list(APPEND OPTIONS_RELEASE -Dintrospection=true)
    vcpkg_get_gobject_introspection_programs(PYTHON3 GIR_COMPILER GIR_SCANNER)
else()
    list(APPEND OPTIONS_RELEASE -Dintrospection=false)
endif()

vcpkg_configure_meson(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS_RELEASE
        ${OPTIONS_RELEASE}
    OPTIONS_DEBUG
        -Dintrospection=false
    ADDITIONAL_BINARIES
        "glib-genmarshal='${CURRENT_HOST_INSTALLED_DIR}/tools/glib/glib-genmarshal'"
        "glib-mkenums='${CURRENT_HOST_INSTALLED_DIR}/tools/glib/glib-mkenums'"
        "g-ir-compiler='${GIR_COMPILER}'"
        "g-ir-scanner='${GIR_SCANNER}'"
)
vcpkg_install_meson(ADD_BIN_TO_PATH)

vcpkg_copy_pdbs()

vcpkg_fixup_pkgconfig()

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/atk-1.0/atk/atkmisc.h" "ifdef ATK_STATIC_COMPILATION" "if 1")
endif()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
