include_directories(${QUAZIP_INCLUDE_DIRS})

add_subdirectory(tests)

set(kritastore_LIB_SRCS
    KoDirectoryStore.cpp
    KoStoreDevice.cpp
    KoStore.cpp
    KoXmlNS.cpp
    KoXmlWriter.cpp
    KoQuaZipStore.cpp
    StoreDebug.cpp
)

kis_add_library(kritastore SHARED ${kritastore_LIB_SRCS})
generate_export_header(kritastore BASE_NAME kritastore)

target_link_libraries(kritastore 
    PRIVATE
        kritaversion 
        kritaglobal 
        KF${KF_MAJOR}::ConfigCore
        Qt${QT_MAJOR_VERSION}::Xml 
        Qt${QT_MAJOR_VERSION}::Gui 
        ${QUAZIP_LIBRARIES}
)

set_target_properties(kritastore PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)
install(TARGETS kritastore ${INSTALL_TARGETS_DEFAULT_ARGS} )

