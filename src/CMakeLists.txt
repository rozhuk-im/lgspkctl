
set(LGSPKCTL_BIN	lgspkctl.c
			../lib/liblcb/src/net/socket.c
			../lib/liblcb/src/net/socket_address.c)

add_executable(lgspkctl ${LGSPKCTL_BIN})
set_target_properties(lgspkctl PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(lgspkctl ${CMAKE_REQUIRED_LIBRARIES} ${CMAKE_EXE_LINKER_FLAGS})

install(TARGETS lgspkctl RUNTIME DESTINATION bin)
