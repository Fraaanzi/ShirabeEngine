#ifndef __SHIRABE_GAPICONFIG_H__
#define __SHIRABE_GAPICONFIG_H__

#include <platform/platform.h>

#ifdef SHIRABE_PLATFORM_WINDOWS
    #define GAPI_USE_DX 
    #ifdef GAPI_USE_DX
        #define GAPI_DX_VERSION 11.0
    #endif
#endif

namespace engine {
	namespace gfxapi {

	}
}

#endif
