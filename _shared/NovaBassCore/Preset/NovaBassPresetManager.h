#pragma once

#include "NovaBassPreset.h"
#include <juce_core/juce_core.h>
#include <vector>

namespace nova::bass
{

// JSON load/save + factory browser, ported in shape from Nova Keys'
// NovaPresetManager (that scan/filter/favorite-persistence pattern is
// already proven correct - only the serialized field set changes here).
// Pure message-thread/file-I/O class - never touched from the audio
// thread.
class NovaBassPresetManager
{
public:
    void scanDirectory (const juce::File& directory);
    void clear();

    int getNumPresets() const noexcept { return (int) presets.size(); }
    const NovaBassPreset* getPreset (int index) const noexcept;

    int getSelectedIndex() const noexcept { return selectedIndex; }
    const NovaBassPreset* getSelectedPreset() const noexcept { return getPreset (selectedIndex); }
    void setSelectedIndex (int index) noexcept;
    void selectNext() noexcept;
    void selectPrevious() noexcept;

    // "All" first, then every distinct category found, in first-seen order.
    juce::StringArray getCategories() const;

    std::vector<int> getFilteredIndices (const juce::String& searchQuery, const juce::String& category, bool favoritesOnly) const;

    // Updates the in-memory favorite flag AND rewrites that preset's file
    // to disk, so favorite state persists across sessions.
    bool setFavorite (int index, bool isFavorite);

    static bool loadPresetFile (const juce::File& file, NovaBassPreset& outPreset);
    static bool savePresetFile (const NovaBassPreset& preset, const juce::File& file);

    static juce::var presetToVar (const NovaBassPreset& preset);
    static bool presetFromVar (const juce::var& v, NovaBassPreset& outPreset);

private:
    struct LoadedPreset { NovaBassPreset preset; juce::File sourceFile; };
    std::vector<LoadedPreset> presets;
    int selectedIndex = -1;
};

} // namespace nova::bass
