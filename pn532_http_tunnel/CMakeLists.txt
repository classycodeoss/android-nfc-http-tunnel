cmake_minimum_required(VERSION 3.6)
project(pn532_http_tunnel)

set(CMAKE_C_STANDARD 11)

find_path(NFC_INCLUDE nfc/nfc.h)
find_library(NFC_LIBRARY NAMES nfc)
include_directories(${NFC_INCLUDE})

find_path(CURL_INCLUDE, curl/curl.h)
find_library(CURL_LIBRARY NAMES curl)
include_directories(${CURL_INCLUDE})

set(SOURCE_FILES_HTTP_TUNNEL pn532_http_tunnel.c)
add_executable(pn532_http_tunnel ${SOURCE_FILES_HTTP_TUNNEL})
target_link_libraries(pn532_http_tunnel ${NFC_LIBRARY})
target_link_libraries(pn532_http_tunnel ${CURL_LIBRARY})
