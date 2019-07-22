# find or download OpenVR SDK

if (${FG_ENABLE_OPENVR})
	set( FG_EXTERNAL_OPENVR_PATH "" CACHE PATH "path to OpenVR SDK" )

	# reset to default
	if (NOT EXISTS "${FG_EXTERNAL_OPENVR_PATH}/include/IL/il.h")
		message( STATUS "OpenVR SDK is not found in \"${FG_EXTERNAL_OPENVR_PATH}\"" )
		set( FG_EXTERNAL_OPENVR_PATH "${FG_EXTERNALS_PATH}/OpenVR" CACHE PATH "" FORCE )
	else ()
		message( STATUS "OpenVR SDK found in \"${FG_EXTERNAL_OPENVR_PATH}\"" )
	endif ()
	
	if (NOT EXISTS "${FG_EXTERNAL_OPENVR_PATH}/include/assimp")
		set( FG_OPENVR_REPOSITORY "https://github.com/ValveSoftware/openvr.git" )
	else ()
		set( FG_OPENVR_REPOSITORY "" )
	endif ()

	# choose platform name
	if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
		if (${CMAKE_SIZEOF_VOID_P} EQUAL 8)
			set( FG_OPENVR_PLATFORM_NAME "linux64" )
		else ()
			set( FG_OPENVR_PLATFORM_NAME "linux32" )
		endif ()

	elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
		if (${CMAKE_SIZEOF_VOID_P} EQUAL 8)
			set( FG_OPENVR_PLATFORM_NAME "osx64" )
		else ()
			set( FG_OPENVR_PLATFORM_NAME "osx32" )
		endif ()

	elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
		if (${CMAKE_SIZEOF_VOID_P} EQUAL 8)
			set( FG_OPENVR_PLATFORM_NAME "win64" )
		else ()
			set( FG_OPENVR_PLATFORM_NAME "win32" )
		endif ()

	else ()
		message( FATAL_ERROR "OpenVR does not supporte current platform ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION}" )
	endif ()
	
	set( FG_OPENVR_INSTALL_DIR "${FG_EXTERNALS_INSTALL_PATH}/OpenVR" CACHE INTERNAL "" FORCE )

	set( FG_OPENVR_LIB_DIR "${FG_EXTERNAL_OPENVR_PATH}/lib/${FG_OPENVR_PLATFORM_NAME}" )
	set( FG_OPENVR_BIN_DIR "${FG_EXTERNAL_OPENVR_PATH}/bin/${FG_OPENVR_PLATFORM_NAME}" )

	ExternalProject_Add( "External.OpenVR"
		LIST_SEPARATOR		"${FG_LIST_SEPARATOR}"
		# download
		GIT_REPOSITORY		${FG_OPENVR_REPOSITORY}
		SOURCE_DIR			"${FG_EXTERNAL_OPENVR_PATH}"
		GIT_TAG				master
		GIT_PROGRESS		1
		EXCLUDE_FROM_ALL	1
		LOG_DOWNLOAD		1
		# update
		PATCH_COMMAND		""
		UPDATE_DISCONNECTED	1
		# configure
		CONFIGURE_COMMAND	""
		LOG_CONFIGURE 		1
		# build
		BINARY_DIR			""
		BUILD_COMMAND		""
		LOG_BUILD 			1
		# install
		INSTALL_COMMAND		${CMAKE_COMMAND} -E copy_if_different
									"${FG_OPENVR_BIN_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}openvr_api${CMAKE_SHARED_LIBRARY_SUFFIX}"
									"${MAIN_BINARY_DIR}/$<CONFIG>/${CMAKE_SHARED_LIBRARY_PREFIX}openvr_api${CMAKE_SHARED_LIBRARY_SUFFIX}"
							COMMAND  ${CMAKE_COMMAND} -E copy_if_different
									"${FG_OPENVR_LIB_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}openvr_api${CMAKE_STATIC_LIBRARY_SUFFIX}"
									"${FG_OPENVR_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}openvr_api${CMAKE_STATIC_LIBRARY_SUFFIX}"
							COMMAND  ${CMAKE_COMMAND} -E copy_if_different
									"${FG_EXTERNAL_OPENVR_PATH}/headers/openvr.h"
									"${FG_OPENVR_INSTALL_DIR}/include/openvr.h"
		INSTALL_DIR 		""
		LOG_INSTALL 		1
		# test
		TEST_COMMAND		""
	)
	
	set_property( TARGET "External.OpenVR" PROPERTY FOLDER "External" )
	set( FG_GLOBAL_DEFINITIONS "${FG_GLOBAL_DEFINITIONS}" "FG_ENABLE_OPENVR" )
endif ()