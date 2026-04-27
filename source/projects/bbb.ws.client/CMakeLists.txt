# source/projects/bbb.ws.client/CMakeLists.txt

if(WIN32)
    set(PLATFORM_DEPS "")
else()
    set(PLATFORM_DEPS pthread)
endif()

bbb_add_external(
    DEPS ixwebsocket ${PLATFORM_DEPS}
)
