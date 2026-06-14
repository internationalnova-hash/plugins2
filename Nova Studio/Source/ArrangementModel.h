#pragma once

#include <JuceHeader.h>
#include <optional>
#include "Session.h"
#include "TimelineModel.h"

namespace NovaStudio
{
    class ArrangementModel : public juce::ChangeBroadcaster
    {
    public:
        enum class EditMode
        {
            Slip,
            Grid,
            Shuffle,
            Spot
        };

        enum class EditTool
        {
            Selector,
            Grabber,
            Trim,
            Split,
            Fade,
            Zoom
        };

        struct AlignSettings
        {
            float amount = 1.0f;
            float naturalness = 0.75f;
            float tightness = 0.5f;
            float phraseSensitivity = 0.75f;
            float consonantPriority = 0.5f;
            int maxShiftMs = 1000;
        };

        ArrangementModel(Session& session, TimelineModel& timelineModel);
        ~ArrangementModel() override;

        const Session& getSession() const noexcept;
        Session& getSession() noexcept;

        void setEditMode(EditMode mode) noexcept;
        EditMode getEditMode() const noexcept;

        void setEditTool(EditTool tool) noexcept;
        EditTool getEditTool() const noexcept;

        void setSnapToGrid(bool enabled) noexcept;
        bool isSnapToGridEnabled() const noexcept;

        bool selectClip(int trackIndex, int clipIndex);
        bool addClipToSelection(int trackIndex, int clipIndex);
        bool toggleClipSelection(int trackIndex, int clipIndex);
        void clearSelection() noexcept;
        bool hasSelection() const noexcept;

        int getSelectedTrackIndex() const noexcept;
        int getSelectedClipIndex() const noexcept;
        Clip* getSelectedClip() noexcept;
        const Clip* getSelectedClip() const noexcept;
        const juce::Array<juce::Point<int>>& getSelectedClips() const noexcept;

        bool moveSelectedClipToSample(int64_t samplePosition);
        bool moveSelectedClipBySamples(int64_t deltaSamples);
        bool splitSelectedClip(int64_t samplePosition);
        bool splitSelectedClipsAtSample(int64_t samplePosition);
        bool trimSelectedClipStart(int64_t samplePosition);
        bool trimSelectedClipEnd(int64_t samplePosition);
        bool duplicateSelectedClip();
        bool deleteSelectedClip();
        bool copySelectedClip();
        bool pasteClipboardClip(int trackIndex, int64_t samplePosition);
        bool toggleSelectedClipMute();
        // Offline (non-realtime) sample editing — operate on the audio region the
        // selected clip currently plays, bouncing the result to a new file and
        // swapping it in via replaceSelectedClipWithUndo (fully undoable).
        bool normalizeSelectedClip(float targetPeakDb = -0.3f);
        bool reverseSelectedClip();

        bool setSelectedClipGain(float gainDb);
        bool setSelectedClipFadeIn(int64_t samples);
        bool setSelectedClipFadeOut(int64_t samples);
        bool replaceSelectedClipWithUndo(const Clip& newClip, const juce::String& transactionName);
        bool lockSelectedClip(bool shouldLock);

        bool setGuideClip(int trackIndex, int clipIndex);
        void clearGuideClip() noexcept;
        bool isGuideClip(int trackIndex, int clipIndex) const noexcept;

        bool toggleAlignTargetClip(int trackIndex, int clipIndex);
        bool isAlignTargetClip(int trackIndex, int clipIndex) const noexcept;
        const juce::Array<juce::Point<int>>& getAlignTargetClips() const noexcept;
        bool hasGuideClip() const noexcept;
        int getGuideTrackIndex() const noexcept;
        int getGuideClipIndex() const noexcept;
        bool previewSelectedTargetsToGuide(const AlignSettings& settings);
        bool commitAlignmentPreview(bool createNewVersion);
        void clearAlignmentPreview() noexcept;
        bool hasAlignmentPreview() const noexcept;
        bool isPreviewClip(int trackIndex, int clipIndex) const noexcept;
        bool alignSelectedTargetsToGuide(const AlignSettings& settings, bool createNewVersion = true);
        void setPreviewBypassed(bool bypassed) noexcept;
        void revertAlignedClip(int trackIndex, int clipIndex);

        // Undo helpers exposed for UndoableAction classes
        void replaceClipWithoutUndo(int trackIndex, int clipIndex, const Clip& newClip);
        void insertClipWithoutUndo(int trackIndex, int clipIndex, const Clip& clip);
        void removeClipWithoutUndo(int trackIndex, int clipIndex);

        int64_t getSnappedSamplePosition(int64_t samplePosition) const noexcept;
        bool moveClipsBySamples(const juce::Array<juce::Point<int>>& targets, int64_t deltaSamples);
        bool moveClipToTrack(int sourceTrack, int clipIndex, int destTrack, int64_t newStartSample);

        // Undo/Redo helpers
        void undo();
        void redo();
        bool canUndo() const;
        bool canRedo() const;

    private:
        Session& session;
        TimelineModel& timelineModel;
        juce::UndoManager undoManager;
        EditMode editMode = EditMode::Slip;
        EditTool editTool = EditTool::Selector;
        bool snapToGrid = true;
        int selectedTrackIndex = -1;
        int selectedClipIndex = -1;
        juce::Array<juce::Point<int>> selectedClips;
        int guideTrackIndex = -1;
        int guideClipIndex = -1;
        juce::Array<juce::Point<int>> alignTargetClips;
        juce::Array<juce::File> previewTemporaryFiles;
        juce::AudioFormatManager editFormatManager;
        juce::Array<juce::File> editedTemporaryFiles;
        bool readClipRegionToBuffer(const Clip& clip, juce::AudioBuffer<float>& dest, double& sampleRateOut);
        juce::File writeBufferToTempWavFile(const juce::AudioBuffer<float>& buffer, double sampleRate);
        std::optional<Clip> clipboardClip;

        int64_t snapResolutionSamples() const noexcept;
        Clip* getClipReference(int trackIndex, int clipIndex) const noexcept;
        Clip* getGuideClip() const noexcept;
        static juce::Point<int> normalizeClipIndex(int trackIndex, int clipIndex) noexcept;
        
    };
}
