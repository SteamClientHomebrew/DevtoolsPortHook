cmake_minimum_required(VERSION 3.15)
project(MillenniumProxy CXX)

set(CMAKE_CXX_STANDARD 17)

set(SRC 
  src/main.cc
  src/exports.def 
)


include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/detours
)

if(WIN32 AND NOT GITHUB_ACTION_BUILD)
  execute_process(
    COMMAND reg query "HKCU\\Software\\Valve\\Steam" /v "SteamPath"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE steam_path
    ERROR_VARIABLE reg_error
  )

  if(result EQUAL 0)
    string(REGEX MATCH "[a-zA-Z]:/[^ ]+([ ]+[^ ]+)*" extracted_path "${steam_path}")
    string(REPLACE "\n" "" extracted_path "${extracted_path}")

    message(STATUS "Build Steam Path: ${extracted_path}")

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${extracted_path})
    set(LIBRARY_OUTPUT_DIRECTORY ${extracted_path})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${extracted_path})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${extracted_path})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${extracted_path})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${extracted_path})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${extracted_path})
  else()
    message(WARNING "Failed to read Steam installation path from HKCU\\Software\\Valve\\Steam.")
  endif()
endif()


add_library(MillenniumProxy SHARED ${SRC})

find_package(minhook CONFIG REQUIRED)
target_link_libraries(MillenniumProxy PRIVATE minhook::minhook version)

set_target_properties(MillenniumProxy PROPERTIES OUTPUT_NAME "version")
set_target_properties(MillenniumProxy PROPERTIES PREFIX "")
set_target_properties(MillenniumProxy PROPERTIES NO_EXPORT TRUE)
