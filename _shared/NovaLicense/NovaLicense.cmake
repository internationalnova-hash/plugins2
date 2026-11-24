# Nova license activation for a plugin target (every store build).
#   include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
#   nova_license_enable(NovaTape)
# Then: NovaLicenseGate in the processor, NovaLicenseOverlay::attachTo in the editor
# (see NovaLicenseGate.h).

set(NOVA_LICENSE_SHARED_DIR ${CMAKE_CURRENT_LIST_DIR})

function(nova_license_enable target)
    # Plugins that already had a license screen compile NovaLicense.cpp themselves
    get_target_property(_srcs ${target} SOURCES)
    set(_has FALSE)
    foreach(_s IN LISTS _srcs)
        if(_s MATCHES "NovaLicense\\.cpp$")
            set(_has TRUE)
        endif()
    endforeach()
    if(NOT _has)
        target_sources(${target} PRIVATE ${NOVA_LICENSE_SHARED_DIR}/NovaLicense.cpp)
        target_link_libraries(${target} PRIVATE juce::juce_cryptography juce::juce_product_unlocking)
    endif()
    target_include_directories(${target} PRIVATE ${NOVA_LICENSE_SHARED_DIR})

    get_target_property(_defs ${target} COMPILE_DEFINITIONS)
    get_target_property(_idefs ${target} INTERFACE_COMPILE_DEFINITIONS)
    if(NOT "${_defs};${_idefs}" MATCHES "NOVA_LICENSE_SERVER_URL")
        target_compile_definitions(${target} PRIVATE NOVA_LICENSE_SERVER_URL="https://www.getnovaaudio.com")
    endif()
    target_compile_definitions(${target} PRIVATE NOVA_LICENSE_GATE=1 NOVA_STORE_URL="https://www.getnovaaudio.com")
endfunction()

# Test programs that compile a plugin's processor: license code present, always unlocked.
function(nova_license_test_target target)
    get_target_property(_srcs ${target} SOURCES)
    set(_has FALSE)
    foreach(_s IN LISTS _srcs)
        if(_s MATCHES "NovaLicense\\.cpp$")
            set(_has TRUE)
        endif()
    endforeach()
    if(NOT _has)
        target_sources(${target} PRIVATE ${NOVA_LICENSE_SHARED_DIR}/NovaLicense.cpp)
    endif()
    target_include_directories(${target} PRIVATE ${NOVA_LICENSE_SHARED_DIR})
    target_link_libraries(${target} PRIVATE juce::juce_cryptography juce::juce_product_unlocking)
    target_compile_definitions(${target} PRIVATE NOVA_LICENSE_DISABLED=1)
endfunction()
