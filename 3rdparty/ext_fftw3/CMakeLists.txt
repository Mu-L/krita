SET(PREFIX_ext_fftw3 "${EXTPREFIX}" )

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i?86.*|x86.*|x86_64.*|amd64.*|AMD64.*)")
    set(SSE_OPTIMIZATIONS_STRING "-DENABLE_SSE=ON" "-DENABLE_SSE2=ON")
else()
    set(SSE_OPTIMIZATIONS_STRING "")
endif()

if (WIN32)
ExternalProject_Add( ext_fftw3
        DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
        URL http://fftw.org/fftw-3.3.9.tar.gz
        URL_HASH SHA256=bf2c7ce40b04ae811af714deb512510cc2c17b9ab9d6ddcf49fe4487eea7af3d

        PATCH_COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/patch_mingw.patch

        INSTALL_DIR ${PREFIX_ext_fftw3}
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${PREFIX_ext_fftw3} -DCMAKE_BUILD_TYPE=${GLOBAL_BUILD_TYPE} ${GLOBAL_PROFILE} ${SSE_OPTIMIZATIONS_STRING} -DBUILD_TESTS=OFF -DWITH_COMBINED_THREADS=ON

        UPDATE_COMMAND ""
)
else()
ExternalProject_Add( ext_fftw3
        DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
        URL http://fftw.org/fftw-3.3.9.tar.gz
        URL_HASH SHA256=bf2c7ce40b04ae811af714deb512510cc2c17b9ab9d6ddcf49fe4487eea7af3d

        PATCH_COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/patch_mingw.patch

        INSTALL_DIR ${PREFIX_ext_fftw3}
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${PREFIX_ext_fftw3} -DCMAKE_BUILD_TYPE=${GLOBAL_BUILD_TYPE} ${GLOBAL_PROFILE} ${SSE_OPTIMIZATIONS_STRING} -DBUILD_TESTS=OFF -DENABLE_THREADS=ON

        UPDATE_COMMAND ""
)
endif()
