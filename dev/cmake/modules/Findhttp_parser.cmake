# - Try to find http_parser
#
# Defines the following variables:
#
# http_parser_FOUND - system has http_parser
# http_parser_INCLUDE_DIR - the http_parser include directory
# http_parser_LIBRARIES - Link these to use http_parser

# Find the header and library
FIND_PATH(http_parser_INCLUDE_DIR NAMES http_parser.h)
FIND_LIBRARY(http_parser_LIBRARY NAMES http_parser libhttp_parser)

# Handle the QUIETLY and REQUIRED arguments and set http_parser_FOUND
# to TRUE if all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(http_parser REQUIRED_VARS http_parser_INCLUDE_DIR http_parser_LIBRARY)

# Hide advanced variables
MARK_AS_ADVANCED(http_parser_INCLUDE_DIR http_parser_LIBRARY)

# Set standard variables
IF (http_parser_FOUND)
	SET(http_parser_LIBRARIES ${http_parser_LIBRARY})
	SET(http_parser_INCLUDE_DIRS ${http_parser_INCLUDE_DIR})
	MESSAGE(STATUS "http_parser libraries: ${http_parser_LIBRARIES}")
	MESSAGE(STATUS "http_parser include dir: ${http_parser_INCLUDE_DIRS}")
ENDIF()

