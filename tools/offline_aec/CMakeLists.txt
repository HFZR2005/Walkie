cmake_minimum_required(VERSION 3.20)
project(offline_aec)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# This file lives in tools/offline_aec; webrtc-audio-processing is a sibling of the repo.
set(WEBRTC_AP_ROOT ${CMAKE_SOURCE_DIR}/../../../webrtc-audio-processing/pre_install/mac/arm64)

add_executable(aec_test aec_test.cpp)

target_include_directories(aec_test PRIVATE
    ${WEBRTC_AP_ROOT}/include/webrtc-audio-processing-1
    ${WEBRTC_AP_ROOT}/include
)
target_link_directories(aec_test PRIVATE
    ${WEBRTC_AP_ROOT}/lib
)

target_link_libraries(aec_test PRIVATE
    webrtc-audio-processing-1
    "-framework CoreFoundation"
)
