CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(imc-plugin C)

#INCLUDE(FindPkgConfig)

SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(EXEC_PREFIX "\${prefix}")
SET(LIBDIR "\${prefix}/lib")
SET(INCLUDEDIR "\${prefix}/include")

# Set required packages
INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED glib-2.0 tcore dlog db-util)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include/)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -Werror -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wdeclaration-after-statement -Wmissing-declarations -Wredundant-decls -Wcast-align")

ADD_DEFINITIONS("-DFEATURE_DLOG_DEBUG")
ADD_DEFINITIONS("-DTCORE_LOG_TAG=\"IMC\"")

MESSAGE(${CMAKE_C_FLAGS})
MESSAGE(${CMAKE_EXE_LINKER_FLAGS})

SET(SRCS
		src/desc.c
		src/s_modem.c
		src/s_common.c
		src/s_network.c
		src/s_sim.c
		src/s_sat.c
		src/s_call.c
		src/s_ss.c
		src/s_ps.c
		src/s_sms.c
)



# library build
ADD_LIBRARY(imc-plugin SHARED ${SRCS})
TARGET_LINK_LIBRARIES(imc-plugin ${pkgs_LDFLAGS})
SET_TARGET_PROPERTIES(imc-plugin PROPERTIES PREFIX "" OUTPUT_NAME imc-plugin)


# install
INSTALL(TARGETS imc-plugin
		LIBRARY DESTINATION lib/telephony/plugins)

INSTALL(FILES ${CMAKE_SOURCE_DIR}/res/wiki_mcc_mnc_oper_list.sql DESTINATION /tmp RENAME mcc_mnc_oper_list.sql)

