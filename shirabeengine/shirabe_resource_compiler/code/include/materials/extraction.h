#ifndef   _SHIRABE_RESOURCECOMP_EXTRACTION_H_
#define   _SHIRABE_RESOURCECOMP_EXTRACTION_H_

#include <vector>
#include <string>
#include <cstdint>
#include <core/result.h>

#include "shadercompilationunit.h"

namespace engine::material
{
    struct SMaterialSignature;
}

namespace materials
{
    using engine::CResult;
    using engine::material::SMaterialSignature;
    
    /**
     * Read a uint32_t word SPIR-V file into a vector<uint32_t>.
     *
     * @param aFileName Filename of the file to read.
     * @return          See brief.
     */
    std::vector<uint32_t> const readSpirVFile(std::string const &aFileName);

    /**
     * @brief spirvCrossExtract
     * @param aUnit
     * @return
     */
    CResult<SMaterialSignature> spirvCrossExtract(SShaderCompilationUnit const &aUnit);
}

#endif // _SHIRABE_SHADERPRECOMP_EXTRACTION_H_
