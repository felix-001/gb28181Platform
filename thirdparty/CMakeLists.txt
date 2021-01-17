include(ExternalProject)

message(${PROJECT_BINARY_DIR}/thirdparty/lib)

ExternalProject_Add(
    libexosip2-5.2
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libexosip2-5.2.0
    CONFIGURE_COMMAND ./configure  --prefix=${PROJECT_BINARY_DIR}/thirdparty OSIP_CFLAGS=-I${CMAKE_CURRENT_SOURCE_DIR}/libosip2-5.2.0/include OSIP_LIBS="-L${PROJECT_BINARY_DIR}/thirdparty/lib -losip2 -losipparser2" --enable-openssl=no
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    BUILD_IN_SOURCE 1
)

ExternalProject_Add(
    libosip2-5.2
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libosip2-5.2.0
    CONFIGURE_COMMAND ./configure  --prefix=${PROJECT_BINARY_DIR}/thirdparty
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    BUILD_IN_SOURCE 1
)


ExternalProject_Add(
    mxml-3.2
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mxml-3.2
    CONFIGURE_COMMAND ./configure  --prefix=${PROJECT_BINARY_DIR}/thirdparty --enable-debug=yes
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    BUILD_IN_SOURCE 1
)