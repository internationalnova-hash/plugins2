/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace nova_pitch_BinaryData
{
    extern const char*   index_html;
    const int            index_htmlSize = 32976;

    extern const char*   n_logo_png;
    const int            n_logo_pngSize = 90199;

    extern const char*   index_js;
    const int            index_jsSize = 50460;

    extern const char*   fallbackboot_js;
    const int            fallbackboot_jsSize = 1345;

    extern const char*   index_js2;
    const int            index_js2Size = 1851;

    extern const char*   check_native_interop_js;
    const int            check_native_interop_jsSize = 796;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 6;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
