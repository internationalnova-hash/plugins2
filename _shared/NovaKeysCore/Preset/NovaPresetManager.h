#pragma once

#include "NovaPreset.h"
#include <juce_core/juce_core.h>
#include <vector>

// JSON load/save + factory browser (spec sections 15-17): scans a directory
// of *.novapreset files, supports search/category-filter/favorites/prev-
// next navigation. Pure message-thread/file-I/O class - never touched from
// the audio thread (preset SWITCHING while notes are playing is handled by
// whatever owns the live NovaKeysEngine, not this class).
class NovaPresetManager
{
public:
    void scanDirectory (const juce::File& directory);
    void clear();

    int getNumPresets() const noexcept { return (int) presets.size(); }
    const NovaPreset* getPreset (int index) const noexcept;

    int getSelectedIndex() const noexcept { return selectedIndex; }
    const NovaPreset* getSelectedPreset() const noexcept { return getPreset (selectedIndex); }
    void setSelectedIndex (int index) noexcept;
    void selectNext() noexcept;
    void selectPrevious() noexcept;

    // "All" first, then every distinct category found, in first-seen order.
    juce::StringArray getCategories() const;

    // Indices into the preset list matching the given filters (empty query
    // = no text filter; empty category or "All" = no category filter).
    std::vector<int> getFilteredIndices (const juce::String& searchQuery, const juce::String& category, bool favoritesOnly) const;

    // Updates the in-memory favorite flag AND rewrites that preset's file
    // to disk, so favorite state persists across sessions (spec section 8).
    bool setFavorite (int index, bool isFavorite);

    static bool loadPresetFile (const juce::File& file, NovaPreset& outPreset);
    static bool savePresetFile (const NovaPreset& preset, const juce::File& file);

    static juce::var presetToVar (const NovaPreset& preset);
    static bool presetFromVar (const juce::var& v, NovaPreset& outPreset);

private:
    struct LoadedPreset { NovaPreset preset; juce::File sourceFile; };
    std::vector<LoadedPreset> presets;
    int selectedIndex = -1;
};
