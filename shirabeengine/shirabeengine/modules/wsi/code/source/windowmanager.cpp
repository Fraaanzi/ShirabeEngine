#include <algorithm>
#include <functional>

#include <platform/platform.h>
#include <base/string.h>

#include "wsi/windowmanager.h"

#ifdef SHIRABE_PLATFORM_WINDOWS
    #include "wsi/windows/windowsdisplay.h"
    #include "wsi/windows/windowswindowfactory.h"
#elif defined SHIRABE_PLATFORM_LINUX
    #include "wsi/x11/x11display.h"
    #include "wsi/x11/x11windowfactory.h"
#endif // SHIRABE_PLATFORM_WINDOWS

namespace engine
{
    namespace wsi
    {
        //<-----------------------------------------------------------------------------
        //
        //<-----------------------------------------------------------------------------
        CWindowManager::CWindowManager()
            : mWindows(),
              mWindowFactory(nullptr)
        {

        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        CWindowManager::~CWindowManager()
        {
            if(!mWindows.empty())
            {
                for(Shared<IWindow> const &window : mWindows)
                {
                    CLog::Warning(logTag(), CString::format("Pending, non-finalized window instance found with name '{}'", window->name()));
                }

                mWindows.clear();
            }
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        CWindowManager::EWindowManagerError CWindowManager::initialize(
                os::SApplicationEnvironment     const &aApplicationEnvironment,
                Shared<IWindowFactory> const &aFactory)
        {
            mWindowFactory = aFactory;

            return EWindowManagerError::Ok;
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        CWindowManager::EWindowManagerError CWindowManager::deinitialize()
        {
            mWindowFactory = nullptr;
            // if (!<condition>) {
            // 	Log::Error(logTag(), "Failed to initialize the window factory.");
            // 	return EWindowManagerError::InitializationFailed;
            // }

            return EWindowManagerError::Ok;
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        CWindowManager::EWindowManagerError CWindowManager::update()
        {
            EWindowManagerError error = EWindowManagerError::Ok;

            for(Shared<IWindow> const &window : mWindows)
            {
                if(CheckEngineError(window->update()))
                {
                    CLog::Warning(logTag(), CString::format("Window '{}' failed to update.", window->name()));

                    error = EWindowManagerError::UpdateFailed;
                    continue;
                }
            }

            return error;
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        Shared<IWindow> CWindowManager::createWindow(
                std::string const &aName,
                CRect       const &aInitialBounds)
        {
            Shared<IWindow> window = mWindowFactory->createWindow(aName, aInitialBounds);
            if(!window)
            {
                CLog::Warning(logTag(), CString::format("Failed to create window '{}' with bounds x/y/w/h --> {}/{}/{}/{}",
                                                      aName.c_str(),
                                                      aInitialBounds.position.x(), aInitialBounds.position.y(),
                                                      aInitialBounds.size.x(),     aInitialBounds.size.y()));
                return nullptr;
            }

            mWindows.push_back(window);

            return window;
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        Shared<IWindow> CWindowManager::getWindowByName(std::string const &aName)
        {
            for(Shared<IWindow> const &window : mWindows)
            {
                if(window->name().compare(aName) == 0)
                {
                    return window;
                }
            }

            return nullptr;
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        Shared<IWindow> CWindowManager::getWindowByHandle(wsi::CWindowHandleWrapper::Handle_t const &aHandle)
        {
            auto const predicate = [&] (Shared<IWindow> const &aCompare) -> bool
            {
                return aCompare->handle() == aHandle;
            };

            IWindowList::iterator it = std::find_if(mWindows.begin(), mWindows.end(), predicate);

            return (it == mWindows.end()) ? nullptr : (*it);

            // One line version:
            // return ((it = std::find_if(_windows.begin(), m_windows.end(), [&](const IWindowPtr& cmp) -> bool { return cmp->handle() == handle; })) == m_windows.end) ? nullptr : (*it);

            // Below code can also be used. But above is a educational demonstration of std::find_if.
            // for (const IWindowPtr& pWindow : m_windows) {
            // 	if (pWindow->handle() == handle)
            // 		return pWindow;
            // }
            // return nullptr;
        }
        //<-----------------------------------------------------------------------------
    }
}
