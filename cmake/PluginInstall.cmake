##---------------------------------------------------------------------------
## Author:      Pavel Kalian (Based on the work of Sean D'Epagnier)
## Copyright:   2014
## License:     GPLv3+
##---------------------------------------------------------------------------

IF(NOT APPLE)
  TARGET_LINK_LIBRARIES( ${PACKAGE_NAME} ${wxWidgets_LIBRARIES} ${PLUGINS_LIBS} )
ENDIF(NOT APPLE)

IF(WIN32)
  SET(PARENT "opencpn")

  IF(MSVC)
#    TARGET_LINK_LIBRARIES(${PACKAGE_NAME}
#	gdiplus.lib
#	glu32.lib)
    TARGET_LINK_LIBRARIES(${PACKAGE_NAME} ${OPENGL_LIBRARIES})

    IF(STANDALONE MATCHES "BUNDLED")
        SET(OPENCPN_IMPORT_LIB "../../${CMAKE_CFG_INTDIR}/${PARENT}")
    ELSE()
        SET(OPENCPN_IMPORT_LIB "${PARENT}")
    ENDIF()
  ENDIF(MSVC)

  IF(MINGW)
# assuming wxwidgets is compiled with unicode, this is needed for mingw headers
    ADD_DEFINITIONS( " -DUNICODE" )
    TARGET_LINK_LIBRARIES(${PACKAGE_NAME} ${OPENGL_LIBRARIES})
    SET(OPENCPN_IMPORT_LIB "${PARENT}.dll")
    SET( CMAKE_SHARED_LINKER_FLAGS "-L../buildwin" )
  ENDIF(MINGW)

  TARGET_LINK_LIBRARIES( ${PACKAGE_NAME} ${OPENCPN_IMPORT_LIB} )

  IF(STANDALONE MATCHES "BUNDLED")
     ADD_DEPENDENCIES(${PACKAGE_NAME} ${PARENT})
  ENDIF(STANDALONE MATCHES "BUNDLED")
ENDIF(WIN32)

IF(UNIX)
 IF(PROFILING)
  find_library(GCOV_LIBRARY
    NAMES
    gcov
    PATHS
    /usr/lib/gcc/i686-pc-linux-gnu/4.7
    )

  SET(PLUGINS_LIBS ${PLUGINS_LIBS} ${GCOV_LIBRARY})
 ENDIF(PROFILING)
ENDIF(UNIX)

IF(APPLE)
  INSTALL(TARGETS ${PACKAGE_NAME} RUNTIME LIBRARY DESTINATION ${CMAKE_BINARY_DIR}/OpenCPN.app/Contents/SharedSupport/plugins)
 FIND_PACKAGE(ZLIB REQUIRED)
 TARGET_LINK_LIBRARIES( ${PACKAGE_NAME} ${ZLIB_LIBRARIES} )
      INSTALL(TARGETS ${PACKAGE_NAME} RUNTIME LIBRARY DESTINATION ${CMAKE_BINARY_DIR}/OpenCPN.app/Contents/PlugIns)
ENDIF(APPLE)

IF(UNIX AND NOT APPLE)
    FIND_PACKAGE(BZip2 REQUIRED)
    INCLUDE_DIRECTORIES(${BZIP2_INCLUDE_DIR})
    FIND_PACKAGE(ZLIB REQUIRED)
    INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR})
    TARGET_LINK_LIBRARIES( ${PACKAGE_NAME} ${BZIP2_LIBRARIES} ${ZLIB_LIBRARY} )
ENDIF(UNIX AND NOT APPLE)

SET(PARENT opencpn)

SET(PREFIX_DATA share)
SET(PREFIX_LIB lib)

IF(WIN32)
    MESSAGE (STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
    SET(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/../OpenCPN)
  IF(CMAKE_CROSSCOMPILING)
    INSTALL(TARGETS ${PACKAGE_NAME} RUNTIME DESTINATION "plugins")
    SET(INSTALL_DIRECTORY "plugins/${PACKAGE_NAME}")
  ELSE(CMAKE_CROSSCOMPILING)
    INSTALL(TARGETS ${PACKAGE_NAME} RUNTIME DESTINATION "plugins")
    SET(INSTALL_DIRECTORY "plugins\\\\${PACKAGE_NAME}")
  ENDIF(CMAKE_CROSSCOMPILING)

  IF(EXISTS ${PROJECT_SOURCE_DIR}/data)
    INSTALL(DIRECTORY data DESTINATION "${INSTALL_DIRECTORY}")
  ENDIF(EXISTS ${PROJECT_SOURCE_DIR}/data)
ENDIF(WIN32)

IF(UNIX AND NOT APPLE)
  SET(PREFIX_PARENTDATA ${PREFIX_DATA}/${PARENT})
  SET(PREFIX_PARENTLIB ${PREFIX_LIB}/${PARENT})
  INSTALL(TARGETS ${PACKAGE_NAME} RUNTIME LIBRARY DESTINATION ${PREFIX_PARENTLIB})

  IF(EXISTS ${PROJECT_SOURCE_DIR}/data)
    INSTALL(DIRECTORY data DESTINATION ${PREFIX_PARENTDATA}/plugins/${PACKAGE_NAME})
  ENDIF()
ENDIF(UNIX AND NOT APPLE)
