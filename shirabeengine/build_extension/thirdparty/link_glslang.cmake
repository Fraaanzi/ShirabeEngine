function(linkLibrary)
    
    set(DIR ${THIRD_PARTY_DIR}/_deploy/glslang/${SHIRABE_PLATFORM_PREFIX}${SHIRABE_PLATFORM_ADDRESS_SIZE}/${SHIRABE_PLATFORM_CONFIG})

    LogStatus(MESSAGES "GlSlang-Cross-Dir: ${DIR}")
	
    # -I
    append_parentscope(
        SHIRABE_PROJECT_INCLUDEPATH
        ${DIR}/include
        )

    # -L
    append_parentscope(
        SHIRABE_PROJECT_LIBRARY_DIRECTORIES
        ${DIR}/lib
        )

    # -l
    append_parentscope(
        SHIRABE_PROJECT_LIBRARY_TARGETS
        glslang
        OGLCompiler
        HLSL
        SPIRV
        SPVRemapper
        OSDependent)

    install(DIRECTORY ${DIR}/bin DESTINATION ${SHIRABE_PROJECT_PUBLIC_DEPLOY_DIR}/bin/tools/glslang USE_SOURCE_PERMISSIONS)
endfunction(linkLibrary)
