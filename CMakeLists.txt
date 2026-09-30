cmake_minimum_required(VERSION 3.10)
project(EasyMPX CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
find_package(Threads REQUIRED)

add_executable(FMPXgen
  "FMPX generator/FMPX generator.cpp"
  "FMPX generator/MPXEncoder.cpp"
  "FMPX generator/RDSEncoder.cpp"
  "FMPX generator/Resampler.cpp"
  "FMPX generator/WavFile.cpp"
)
target_include_directories(FMPXgen PRIVATE ${PORTAUDIO_INCLUDE_DIRS})
target_link_directories(FMPXgen PRIVATE ${PORTAUDIO_LIBRARY_DIRS})
target_link_libraries(FMPXgen PRIVATE ${PORTAUDIO_LIBRARIES} Threads::Threads)
