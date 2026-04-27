# source/projects/bbb.ws.client/CMakeLists.txt

if(WIN32)
    set(PLATFORM_DEPS wsock32 ws2_32 shlwapi crypt32 bcrypt)
else()
    set(PLATFORM_DEPS pthread)
endif()

bbb_add_external(
    DEPS ixwebsocket ${PLATFORM_DEPS}
)
