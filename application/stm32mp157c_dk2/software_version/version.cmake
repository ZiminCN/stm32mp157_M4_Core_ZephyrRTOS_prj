execute_process(
COMMAND
${PYTHON_EXECUTABLE}
${CMAKE_CURRENT_SOURCE_DIR}/software_version/version.py
OUTPUT_VARIABLE
WORDSIZE
OUTPUT_STRIP_TRAILING_WHITESPACE
)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/software_version/SOFTWARE_VERSION ver)

string(REGEX MATCH "VERSION_MAJOR=\'([0-9]*)\'" _ ${ver})
set(VERSION_MAJOR ${CMAKE_MATCH_1})

string(REGEX MATCH "VERSION_BOARD_TYPE=\'([0-9]*)\'" _ ${ver})
set(VERSION_BOARD_TYPE ${CMAKE_MATCH_1})

string(REGEX MATCH "VERSION_HW=\'([0-9]*)\'" _ ${ver})
set(VERSION_HW ${CMAKE_MATCH_1})

string(REGEX MATCH "VERSION_MINOR=\'([0-9]*)\'" _ ${ver})
set(VERSION_MINOR ${CMAKE_MATCH_1})

string(REGEX MATCH "BUILD_DATE=\'([0-9]*)\'" _ ${ver})
set(BUILD_DATE ${CMAKE_MATCH_1})

string(REGEX MATCH "BUILD_TIMESTAMP=\'([0-9]*)\'" _ ${ver})
set(BUILD_TIMESTAMP ${CMAKE_MATCH_1})

string(REGEX MATCH "VERSION_=\'([0-9]*)\'" _ ${ver})
set(VERSION_ ${CMAKE_MATCH_1})

set(VERSION_STRING "\"${BUILD_DATE}\-${VERSION_MAJOR}.${VERSION_BOARD_TYPE}.${VERSION_HW}.${VERSION_MINOR}-${CONFIG_BOARD}\"")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/software_version/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/software_version/app_version.h)
