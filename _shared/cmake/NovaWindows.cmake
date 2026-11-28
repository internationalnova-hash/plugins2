# Windows support shared by the store plugins. Does nothing on macOS/Linux.
#   include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/cmake/NovaWindows.cmake)
#   nova_windows_webview2(NovaTape)
#
# The plugin UIs are WebBrowserComponent pages that use withResourceProvider /
# native functions. On Windows those only exist with the WebView2 (Edge) backend;
# without it JUCE falls back to the old Internet Explorer control and the editor
# doesn't compile. This links WebView2 statically into the plugin's shared code
# target, same as juce_add_plugin(... NEEDS_WEBVIEW2 TRUE) would.
#
# The WebView2 SDK comes from NuGet: pass -DJUCE_WEBVIEW2_PACKAGE_LOCATION=<dir
# holding Microsoft.Web.WebView2.*> (CI does), or install it the way JUCE's
# FindWebView2.cmake describes.

function(nova_windows_webview2 target)
    if(NOT WIN32)
        return()
    endif()

    if(NOT TARGET juce_webview2)
        if(NOT ("${JUCE_CMAKE_UTILS_DIR}" IN_LIST CMAKE_MODULE_PATH))
            list(APPEND CMAKE_MODULE_PATH "${JUCE_CMAKE_UTILS_DIR}")
        endif()
        find_package(WebView2 REQUIRED)
    endif()

    target_link_libraries(${target} PRIVATE juce::juce_webview2)

    # Factory presets use designated initializers ({ .name = ... }), which
    # Apple clang accepts in C++17 but MSVC only in C++20. Windows only, so
    # the Mac builds stay exactly as they are.
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_compile_definitions(${target}
        PUBLIC
            JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
            _USE_MATH_DEFINES=1
    )
endfunction()
