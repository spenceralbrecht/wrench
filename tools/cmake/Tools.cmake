# install wrench-init tool
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/tools/wrench/wrench-init
        DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
