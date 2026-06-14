#include "ArrangementModel.h"
#include "NovaAlignProcessor.h"

namespace NovaStudio
{
    ArrangementModel::ArrangementModel(Session& sessionRef, TimelineModel& timelineRef)
        : session(sessionRef), timelineModel(timelineRef)
    {
    }

    ArrangementModel::~ArrangementModel() = default;

    // Undoable action implementations
    struct ReplaceClipAction : public juce::UndoableAction
    {
        ArrangementModel* model;
        int trackIndex;
        int clipIndex;
        Clip oldClip;
        Clip newClip;

        ReplaceClipAction(ArrangementModel* m, int t, int c, const Clip& oldC, const Clip& newC)
            : model(m), trackIndex(t), clipIndex(c), oldClip(oldC), newClip(newC) {}

        bool perform() override
        {
            model->replaceClipWithoutUndo(trackIndex, clipIndex, newClip);
            return true;
        }

        bool undo() override
        {
            model->replaceClipWithoutUndo(trackIndex, clipIndex, oldClip);
            return true;
        }

        int getSizeInUnits() override { return 1; }
    };

    struct InsertClipAction : public juce::UndoableAction
    {
        ArrangementModel* model;
        int trackIndex;
        int clipIndex;
        Clip clip;

        InsertClipAction(ArrangementModel* m, int t, int c, const Clip& cl)
            : model(m), trackIndex(t), clipIndex(c), clip(cl) {}

        bool perform() override
        {
            model->insertClipWithoutUndo(trackIndex, clipIndex, clip);
            return true;
        }

        bool undo() override
        {
            model->removeClipWithoutUndo(trackIndex, clipIndex);
            return true;
        }

        int getSizeInUnits() override { return 1; }
    };

    struct RemoveClipAction : public juce::UndoableAction
    {
        ArrangementModel* model;
        int trackIndex;
        int clipIndex;
        Clip clip;

        RemoveClipAction(ArrangementModel* m, int t, int c, const Clip& cl)
            : model(m), trackIndex(t), clipIndex(c), clip(cl) {}

        bool perform() override
        {
            model->removeClipWithoutUndo(trackIndex, clipIndex);
            return true;
        }

        bool undo() override
        {
            model->insertClipWithoutUndo(trackIndex, clipIndex, clip);
            return true;
        }

        int getSizeInUnits() override { return 1; }
    };

    const Session& ArrangementModel::getSession() const noexcept
    {
        return session;
    }

    Session& ArrangementModel::getSession() noexcept
    {
        return session;
    }

    void ArrangementModel::setEditMode(EditMode mode) noexcept
    {
        editMode = mode;
        sendChangeMessage();
    }

    ArrangementModel::EditMode ArrangementModel::getEditMode() const noexcept
    {
        return editMode;
    }

    void ArrangementModel::setEditTool(EditTool tool) noexcept
    {
        editTool = tool;
        sendChangeMessage();
    }

    ArrangementModel::EditTool ArrangementModel::getEditTool() const noexcept
    {
        return editTool;
    }

    void ArrangementModel::setSnapToGrid(bool enabled) noexcept
    {
        snapToGrid = enabled;
        sendChangeMessage();
    }

    bool ArrangementModel::isSnapToGridEnabled() const noexcept
    {
        return snapToGrid;
    }

    bool ArrangementModel::selectClip(int trackIndex, int clipIndex)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return false;

        const auto& track = session.getTrack(trackIndex);
        if (!isPositiveAndBelow(clipIndex, track.clips.size()))
            return false;

        selectedTrackIndex = trackIndex;
        selectedClipIndex = clipIndex;
        selectedClips.clear();
        selectedClips.add(juce::Point<int>(trackIndex, clipIndex));
        sendChangeMessage();
        return true;
    }

    void ArrangementModel::clearSelection() noexcept
    {
        selectedTrackIndex = -1;
        selectedClipIndex = -1;
        selectedClips.clear();
        sendChangeMessage();
    }

    bool ArrangementModel::hasSelection() const noexcept
    {
        return selectedTrackIndex >= 0 && selectedClipIndex >= 0;
    }

    int ArrangementModel::getSelectedTrackIndex() const noexcept
    {
        return selectedTrackIndex;
    }

    int ArrangementModel::getSelectedClipIndex() const noexcept
    {
        return selectedClipIndex;
    }

    Clip* ArrangementModel::getSelectedClip() noexcept
    {
        return getClipReference(selectedTrackIndex, selectedClipIndex);
    }

    const Clip* ArrangementModel::getSelectedClip() const noexcept
    {
        return getClipReference(selectedTrackIndex, selectedClipIndex);
    }

    const juce::Array<juce::Point<int>>& ArrangementModel::getSelectedClips() const noexcept
    {
        return selectedClips;
    }

    bool ArrangementModel::addClipToSelection(int trackIndex, int clipIndex)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return false;
        const auto& track = session.getTrack(trackIndex);
        if (!isPositiveAndBelow(clipIndex, track.clips.size()))
            return false;
        juce::Point<int> p(trackIndex, clipIndex);
        if (!selectedClips.contains(p))
            selectedClips.add(p);
        selectedTrackIndex = selectedClips.getFirst().x;
        selectedClipIndex = selectedClips.getFirst().y;
        sendChangeMessage();
        return true;
    }

    bool ArrangementModel::toggleClipSelection(int trackIndex, int clipIndex)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return false;
        const auto& track = session.getTrack(trackIndex);
        if (!isPositiveAndBelow(clipIndex, track.clips.size()))
            return false;
        juce::Point<int> p(trackIndex, clipIndex);
        const int idx = selectedClips.indexOf(p);
        if (idx >= 0)
            selectedClips.remove(idx);
        else
            selectedClips.add(p);

        if (!selectedClips.isEmpty())
        {
            selectedTrackIndex = selectedClips.getFirst().x;
            selectedClipIndex = selectedClips.getFirst().y;
        }
        else
        {
            selectedTrackIndex = -1;
            selectedClipIndex = -1;
        }

        sendChangeMessage();
        return true;
    }

    bool ArrangementModel::trimSelectedClipStart(int64_t samplePosition)
    {
        Clip* clip = getSelectedClip();
        if (clip == nullptr || clip->locked)
            return false;
        const int64_t newStart = juce::jlimit<int64_t>(clip->startSample, clip->startSample + clip->lengthSamples - 1, samplePosition);
        const int64_t lengthDelta = newStart - clip->startSample;
        if (lengthDelta <= 0)
            return false;

        Clip oldClip = *clip;
        Clip newClip = *clip;
        newClip.startSample = newStart;
        newClip.lengthSamples = juce::jmax<int64_t>(1, newClip.lengthSamples - lengthDelta);

        undoManager.beginNewTransaction("Trim Clip Start");
        undoManager.perform(new ReplaceClipAction(this, selectedTrackIndex, selectedClipIndex, oldClip, newClip));
        return true;
    }

    bool ArrangementModel::trimSelectedClipEnd(int64_t samplePosition)
    {
        Clip* clip = getSelectedClip();
        if (clip == nullptr || clip->locked)
            return false;
        const int64_t newEnd = juce::jlimit<int64_t>(clip->startSample + 1, clip->startSample + clip->lengthSamples, samplePosition);
        const int64_t newLength = newEnd - clip->startSample;
        if (newLength <= 0)
            return false;

        Clip oldClip = *clip;
        Clip newClip = *clip;
        newClip.lengthSamples = newLength;

        undoManager.beginNewTransaction("Trim Clip End");
        undoManager.perform(new ReplaceClipAction(this, selectedTrackIndex, selectedClipIndex, oldClip, newClip));
        return true;
    }

    bool ArrangementModel::duplicateSelectedClip()
    {
        // Support duplicating multiple selected clips atomically.
        juce::Array<juce::Point<int>> targets;
        if (!selectedClips.isEmpty())
            targets = selectedClips;
        else if (hasSelection())
            targets.add(juce::Point<int>(selectedTrackIndex, selectedClipIndex));

        if (targets.isEmpty())
            return false;

        // Build duplicates and insert actions
        struct MultiInsertAction : public juce::UndoableAction
        {
            ArrangementModel* model;
            juce::Array<int> trackIndices;
            juce::Array<int> insertIndices;
            juce::Array<Clip> clips;

            MultiInsertAction(ArrangementModel* m) : model(m) {}

            bool perform() override
            {
                for (int i = 0; i < clips.size(); ++i)
                    model->insertClipWithoutUndo(trackIndices.getUnchecked(i), insertIndices.getUnchecked(i), clips.getUnchecked(i));
                return true;
            }

            bool undo() override
            {
                for (int i = clips.size() - 1; i >= 0; --i)
                    model->removeClipWithoutUndo(trackIndices.getUnchecked(i), insertIndices.getUnchecked(i));
                return true;
            }

            int getSizeInUnits() override { return 1; }
        };

        auto* action = new MultiInsertAction(this);

        // Insert in ascending track/clip order so indices are stable per-track
        {
            std::vector<juce::Point<int>> vec;
            vec.reserve((size_t)targets.size());
            for (auto& p : targets) vec.push_back(p);
            std::sort(vec.begin(), vec.end(), [](const juce::Point<int>& a, const juce::Point<int>& b) {
                if (a.x != b.x) return a.x < b.x;
                return a.y < b.y;
            });
            targets.clear();
            for (auto& p : vec) targets.add(p);
        }

        for (int i = 0; i < targets.size(); ++i)
        {
            const auto p = targets.getReference(i);
            Clip* clip = getClipReference(p.x, p.y);
            if (clip == nullptr) continue;
            Clip duplicate = *clip;
            duplicate.startSample = getSnappedSamplePosition(clip->startSample + snapResolutionSamples());
            action->trackIndices.add(p.x);
            action->insertIndices.add(p.y + 1);
            action->clips.add(duplicate);
        }

        if (action->clips.isEmpty()) { delete action; return false; }

        undoManager.beginNewTransaction("Duplicate Clip(s)");
        undoManager.perform(action);
        return true;
    }

    bool ArrangementModel::deleteSelectedClip()
    {
        juce::Array<juce::Point<int>> targets;
        if (!selectedClips.isEmpty())
            targets = selectedClips;
        else if (hasSelection())
            targets.add(juce::Point<int>(selectedTrackIndex, selectedClipIndex));

        if (targets.isEmpty())
            return false;

        // Normalize and sort targets descending per-track so removals don't shift earlier indices.
        {
            std::vector<juce::Point<int>> vec;
            vec.reserve((size_t)targets.size());
            for (auto& p : targets) vec.push_back(p);
            std::sort(vec.begin(), vec.end(), [](const juce::Point<int>& a, const juce::Point<int>& b) {
                if (a.x != b.x) return a.x < b.x;
                return a.y < b.y;
            });
            targets.clear();
            for (auto& p : vec) targets.add(p);
        }

        struct MultiRemoveAction : public juce::UndoableAction
        {
            ArrangementModel* model;
            juce::Array<int> trackIndices;
            juce::Array<int> clipIndices;
            juce::Array<Clip> clips;

            MultiRemoveAction(ArrangementModel* m) : model(m) {}

            bool perform() override
            {
                // remove from highest indices first
                for (int i = clips.size() - 1; i >= 0; --i)
                    model->removeClipWithoutUndo(trackIndices.getUnchecked(i), clipIndices.getUnchecked(i));
                return true;
            }

            bool undo() override
            {
                // re-insert in ascending order
                for (int i = 0; i < clips.size(); ++i)
                    model->insertClipWithoutUndo(trackIndices.getUnchecked(i), clipIndices.getUnchecked(i), clips.getUnchecked(i));
                return true;
            }

            int getSizeInUnits() override { return 1; }
        };

        auto* action = new MultiRemoveAction(this);

        for (int i = targets.size() - 1; i >= 0; --i)
        {
            const auto p = targets.getReference(i);
            if (!isPositiveAndBelow(p.x, session.getNumTracks()))
                continue;
            auto& track = session.getTrack(p.x);
            if (!isPositiveAndBelow(p.y, track.clips.size()))
                continue;
            action->trackIndices.add(p.x);
            action->clipIndices.add(p.y);
            action->clips.add(track.clips.getReference(p.y));
        }

        if (action->clips.isEmpty()) { delete action; return false; }

        undoManager.beginNewTransaction("Delete Clip(s)");
        undoManager.perform(action);
        clearSelection();
        return true;
    }

    bool ArrangementModel::copySelectedClip()
    {
        Clip* clip = getSelectedClip();
        if (clip == nullptr)
            return false;

        clipboardClip = *clip;
        return true;
    }

    bool ArrangementModel::pasteClipboardClip(int trackIndex, int64_t samplePosition)
    {
        if (!clipboardClip.has_value())
            return false;

        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return false;

        Clip newClip = clipboardClip.value();
        newClip.startSample = juce::jmax<int64_t>(0, samplePosition);

        undoManager.beginNewTransaction("Paste Clip");
        auto& track = session.getTrack(trackIndex);
        const int insertAt = track.clips.size();
        undoManager.perform(new InsertClipAction(this, trackIndex, insertAt, newClip));
        // Select the newly pasted clip
        selectedTrackIndex = trackIndex;
        selectedClipIndex = insertAt;
        selectedClips.clear();
        selectedClips.add(juce::Point<int>(trackIndex, insertAt));
        return true;
    }

    bool ArrangementModel::setGuideClip(int trackIndex, int clipIndex)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return false;

        const auto& track = session.getTrack(trackIndex);
        if (!isPositiveAndBelow(clipIndex, track.clips.size()))
            return false;

        guideTrackIndex = trackIndex;
        guideClipIndex = clipIndex;
        sendChangeMessage();
        return true;
    }

    void ArrangementModel::clearGuideClip() noexcept
    {
        guideTrackIndex = -1;
        guideClipIndex = -1;
        sendChangeMessage();
    }

    bool ArrangementModel::isGuideClip(int trackIndex, int clipIndex) const noexcept
    {
        return guideTrackIndex == trackIndex && guideClipIndex == clipIndex;
    }

    bool ArrangementModel::toggleAlignTargetClip(int trackIndex, int clipIndex)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return false;

        const auto& track = session.getTrack(trackIndex);
        if (!isPositiveAndBelow(clipIndex, track.clips.size()))
            return false;

        juce::Point<int> clipRef(trackIndex, clipIndex);
        const int existing = alignTargetClips.indexOf(clipRef);
        if (existing >= 0)
        {
            alignTargetClips.remove(existing);
        }
        else
        {
            alignTargetClips.add(clipRef);
        }

        sendChangeMessage();
        return true;
    }

    bool ArrangementModel::isAlignTargetClip(int trackIndex, int clipIndex) const noexcept
    {
        return alignTargetClips.contains(juce::Point<int>(trackIndex, clipIndex));
    }

    const juce::Array<juce::Point<int>>& ArrangementModel::getAlignTargetClips() const noexcept
    {
        return alignTargetClips;
    }

    bool ArrangementModel::hasGuideClip() const noexcept
    {
        return guideTrackIndex >= 0 && guideClipIndex >= 0;
    }

    int ArrangementModel::getGuideTrackIndex() const noexcept
    {
        return guideTrackIndex;
    }

    int ArrangementModel::getGuideClipIndex() const noexcept
    {
        return guideClipIndex;
    }

    bool ArrangementModel::previewSelectedTargetsToGuide(const AlignSettings& settings)
    {
        clearAlignmentPreview();

        Clip* guideClip = getGuideClip();
        if (guideClip == nullptr || alignTargetClips.isEmpty())
            return false;

        for (int i = alignTargetClips.size() - 1; i >= 0; --i)
        {
            const auto clipRef = alignTargetClips.getReference(i);
            Clip* targetClip = getClipReference(clipRef.x, clipRef.y);
            if (targetClip == nullptr || targetClip->locked)
                continue;

            juce::File outputFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                        .getChildFile("NovaAlign_Preview_" + targetClip->file.getFileName());
            outputFile = outputFile.getNonexistentSibling();
            int64_t shiftSamples = 0;

            NovaAlignProcessor::AlignSettings alignSettings;
            alignSettings.amount = settings.amount;
            alignSettings.naturalness = settings.naturalness;
            alignSettings.tightness = settings.tightness;
            alignSettings.phraseSensitivity = settings.phraseSensitivity;
            alignSettings.consonantPriority = settings.consonantPriority;
            alignSettings.maxShiftMs = settings.maxShiftMs;

            if (!NovaAlignProcessor::alignTargetClip(*guideClip, *targetClip, outputFile, alignSettings, shiftSamples))
                continue;

            Clip alignedClip = *targetClip;
            alignedClip.sourceTrackIndex = clipRef.x;
            alignedClip.sourceClipIndex = clipRef.y;
            alignedClip.guideTrackIndex = guideTrackIndex;
            alignedClip.guideClipIndex = guideClipIndex;
            alignedClip.originalFile = targetClip->file;
            alignedClip.file = outputFile;
            alignedClip.aligned = true;
            alignedClip.isPreview = true;
            alignedClip.alignmentOffsetSamples = shiftSamples;
            alignedClip.alignmentTimestamp = juce::Time::getCurrentTime().toString(true, true);
            alignedClip.alignmentPhraseSensitivity = settings.phraseSensitivity;
            alignedClip.alignmentConsonantPriority = settings.consonantPriority;
            alignedClip.alignmentSourceGuidePath = guideClip->file.getFullPathName();
            alignedClip.startSample = guideClip->startSample;
            alignedClip.clipColor = targetClip->clipColor.withAlpha(0.85f);

            auto& track = session.getTrack(clipRef.x);
            track.clips.insert(clipRef.y + 1, alignedClip);
            previewTemporaryFiles.add(outputFile);
        }

        sendChangeMessage();
        return !previewTemporaryFiles.isEmpty();
    }

    bool ArrangementModel::commitAlignmentPreview(bool createNewVersion)
    {
        // Collect all preview clips to commit
        struct ClipCommitInfo { int trackIndex; int clipIndex; Clip before; Clip after; };
        juce::Array<ClipCommitInfo> commits;

        for (int trackIndex = 0; trackIndex < session.getNumTracks(); ++trackIndex)
        {
            auto& track = session.getTrack(trackIndex);
            for (int clipIndex = 0; clipIndex < track.clips.size(); ++clipIndex)
            {
                Clip& clip = track.clips.getReference(clipIndex);
                if (!clip.isPreview)
                    continue;

                Clip before = clip;
                Clip after = clip;
                after.isPreview = false;
                after.aligned = true;
                after.alignVersion = createNewVersion ? juce::jmax(1, clip.alignVersion + 1) : juce::jmax(1, clip.alignVersion);

                commits.add({ trackIndex, clipIndex, before, after });
            }
        }

        if (commits.isEmpty())
            return false;

        // Create a single undoable action that applies all commits atomically
        struct CommitPreviewAction : public juce::UndoableAction
        {
            ArrangementModel* model;
            juce::Array<ClipCommitInfo> changes;

            CommitPreviewAction(ArrangementModel* m, const juce::Array<ClipCommitInfo>& ch)
                : model(m), changes(ch) {}

            bool perform() override
            {
                for (auto& info : changes)
                    model->replaceClipWithoutUndo(info.trackIndex, info.clipIndex, info.after);
                return true;
            }

            bool undo() override
            {
                for (int i = changes.size() - 1; i >= 0; --i)
                {
                    auto& info = changes.getReference(i);
                    model->replaceClipWithoutUndo(info.trackIndex, info.clipIndex, info.before);
                }
                return true;
            }

            int getSizeInUnits() override { return 1; }
        };

        undoManager.beginNewTransaction("Commit Alignment Preview");
        undoManager.perform(new CommitPreviewAction(this, commits));
        return true;
    }

    void ArrangementModel::clearAlignmentPreview() noexcept
    {
        for (auto& tempFile : previewTemporaryFiles)
        {
            if (tempFile.existsAsFile())
                tempFile.deleteFile();
        }
        previewTemporaryFiles.clear();

        for (int trackIndex = 0; trackIndex < session.getNumTracks(); ++trackIndex)
        {
            auto& track = session.getTrack(trackIndex);
            for (int clipIndex = track.clips.size() - 1; clipIndex >= 0; --clipIndex)
            {
                if (track.clips.getReference(clipIndex).isPreview)
                    track.clips.remove(clipIndex);
            }
        }

        sendChangeMessage();
    }

    bool ArrangementModel::hasAlignmentPreview() const noexcept
    {
        return !previewTemporaryFiles.isEmpty();
    }

    void ArrangementModel::setPreviewBypassed(bool bypassed) noexcept
    {
        // Interpret bypassed as 'play original' when true. Store the preference in session.
        session.setPreviewPlaybackEnabled(!bypassed);
        sendChangeMessage();
    }

    void ArrangementModel::revertAlignedClip(int trackIndex, int clipIndex)
    {
        Clip* clip = getClipReference(trackIndex, clipIndex);
        if (clip == nullptr)
            return;

        if (!clip->aligned)
            return;

        if (!clip->originalFile.existsAsFile())
            return;

        Clip before = *clip;
        Clip after = *clip;
        after.file = after.originalFile;
        after.aligned = false;
        after.alignVersion = 0;
        after.alignmentOffsetSamples = 0;
        after.alignmentTimestamp = "";
        after.alignmentWarpPoints.clear();

        struct RevertAlignedAction : public juce::UndoableAction
        {
            ArrangementModel* model;
            int tIdx, cIdx;
            Clip before;
            Clip after;

            RevertAlignedAction(ArrangementModel* m, int t, int c, const Clip& b, const Clip& a)
                : model(m), tIdx(t), cIdx(c), before(b), after(a) {}

            bool perform() override
            {
                model->replaceClipWithoutUndo(tIdx, cIdx, after);
                return true;
            }

            bool undo() override
            {
                model->replaceClipWithoutUndo(tIdx, cIdx, before);
                return true;
            }

            int getSizeInUnits() override { return 1; }
        };

        undoManager.beginNewTransaction("Revert Aligned Clip");
        undoManager.perform(new RevertAlignedAction(this, trackIndex, clipIndex, before, after));
    }

    bool ArrangementModel::isPreviewClip(int trackIndex, int clipIndex) const noexcept
    {
        const Clip* clip = getClipReference(trackIndex, clipIndex);
        return clip != nullptr && clip->isPreview;
    }

    bool ArrangementModel::alignSelectedTargetsToGuide(const AlignSettings& settings, bool createNewVersion)
    {
        Clip* guideClip = getGuideClip();
        if (guideClip == nullptr || alignTargetClips.isEmpty())
            return false;

        for (int i = alignTargetClips.size() - 1; i >= 0; --i)
        {
            const auto clipRef = alignTargetClips.getReference(i);
            Clip* targetClip = getClipReference(clipRef.x, clipRef.y);
            if (targetClip == nullptr || targetClip->locked)
                continue;

            juce::File outputFile = targetClip->file.getParentDirectory().getChildFile("Aligned_" + targetClip->file.getFileName());
            outputFile = outputFile.getNonexistentSibling();
            int64_t shiftSamples = 0;

            NovaAlignProcessor::AlignSettings alignSettings;
            alignSettings.amount = settings.amount;
            alignSettings.naturalness = settings.naturalness;
            alignSettings.tightness = settings.tightness;
            alignSettings.phraseSensitivity = settings.phraseSensitivity;
            alignSettings.consonantPriority = settings.consonantPriority;
            alignSettings.maxShiftMs = settings.maxShiftMs;

            if (!NovaAlignProcessor::alignTargetClip(*guideClip, *targetClip, outputFile, alignSettings, shiftSamples))
                continue;

            Clip alignedClip = *targetClip;
            alignedClip.sourceTrackIndex = clipRef.x;
            alignedClip.sourceClipIndex = clipRef.y;
            alignedClip.guideTrackIndex = guideTrackIndex;
            alignedClip.guideClipIndex = guideClipIndex;
            alignedClip.originalFile = targetClip->file;
            alignedClip.file = outputFile;
            alignedClip.aligned = true;
            alignedClip.isPreview = false;
            alignedClip.alignmentOffsetSamples = shiftSamples;
            alignedClip.alignmentTimestamp = juce::Time::getCurrentTime().toString(true, true);
            alignedClip.alignmentPhraseSensitivity = settings.phraseSensitivity;
            alignedClip.alignmentConsonantPriority = settings.consonantPriority;
            alignedClip.alignmentSourceGuidePath = guideClip->file.getFullPathName();
            alignedClip.startSample = guideClip->startSample;
            alignedClip.clipColor = targetClip->clipColor.withAlpha(0.9f);

            if (createNewVersion)
                alignedClip.alignVersion = juce::jmax(1, targetClip->alignVersion + 1);
            else
                alignedClip.alignVersion = juce::jmax(1, targetClip->alignVersion);

            auto& track = session.getTrack(clipRef.x);
            track.clips.insert(clipRef.y + 1, alignedClip);
        }

        alignTargetClips.clear();
        sendChangeMessage();
        return true;
    }

    Clip* ArrangementModel::getGuideClip() const noexcept
    {
        return getClipReference(guideTrackIndex, guideClipIndex);
    }

    bool ArrangementModel::moveSelectedClipBySamples(int64_t deltaSamples)
    {
        Clip* clip = getSelectedClip();
        if (clip == nullptr || clip->locked)
            return false;

        const int64_t target = clip->startSample + deltaSamples;
        return moveSelectedClipToSample(target);
    }

    bool ArrangementModel::moveClipsBySamples(const juce::Array<juce::Point<int>>& targets, int64_t deltaSamples)
    {
        if (targets.isEmpty())
            return false;

        struct MultiReplaceAction : public juce::UndoableAction
        {
            ArrangementModel* model;
            juce::Array<int> trackIndices;
            juce::Array<int> clipIndices;
            juce::Array<Clip> beforeClips;
            juce::Array<Clip> afterClips;

            MultiReplaceAction(ArrangementModel* m) : model(m) {}

            bool perform() override
            {
                for (int i = 0; i < afterClips.size(); ++i)
                    model->replaceClipWithoutUndo(trackIndices.getUnchecked(i), clipIndices.getUnchecked(i), afterClips.getUnchecked(i));
                return true;
            }

            bool undo() override
            {
                for (int i = beforeClips.size() - 1; i >= 0; --i)
                    model->replaceClipWithoutUndo(trackIndices.getUnchecked(i), clipIndices.getUnchecked(i), beforeClips.getUnchecked(i));
                return true;
            }

            int getSizeInUnits() override { return 1; }
        };

        auto* action = new MultiReplaceAction(this);

        for (int i = 0; i < targets.size(); ++i)
        {
            const auto p = targets.getReference(i);
            if (!isPositiveAndBelow(p.x, session.getNumTracks()))
                continue;
            auto& track = session.getTrack(p.x);
            if (!isPositiveAndBelow(p.y, track.clips.size()))
                continue;

            Clip before = track.clips.getReference(p.y);
            Clip after = before;
            after.startSample = juce::jmax<int64_t>(0, before.startSample + deltaSamples);

            action->trackIndices.add(p.x);
            action->clipIndices.add(p.y);
            action->beforeClips.add(before);
            action->afterClips.add(after);
        }

        if (action->afterClips.isEmpty()) { delete action; return false; }

        undoManager.beginNewTransaction("Move Clips");
        undoManager.perform(action);
        return true;
    }

    bool ArrangementModel::moveSelectedClipToSample(int64_t samplePosition)
    {
        Clip* clip = getSelectedClip();
        if (clip == nullptr || clip->locked)
            return false;
        Clip oldClip = *clip;
        Clip newClip = *clip;
        newClip.startSample = getSnappedSamplePosition(juce::jmax<int64_t>(0, samplePosition));

        undoManager.beginNewTransaction("Move Clip");
        undoManager.perform(new ReplaceClipAction(this, selectedTrackIndex, selectedClipIndex, oldClip, newClip));
        return true;
    }

    bool ArrangementModel::splitSelectedClip(int64_t samplePosition)
    {
        Clip* clip = getSelectedClip();
        if (clip == nullptr || clip->locked)
            return false;
        const int64_t splitSample = juce::jlimit<int64_t>(clip->startSample + 1, clip->startSample + clip->lengthSamples - 1, samplePosition);
        if (splitSample <= clip->startSample || splitSample >= clip->startSample + clip->lengthSamples)
            return false;

        Clip oldClip = *clip;
        Clip firstPart = *clip;
        Clip secondPart = *clip;

        firstPart.lengthSamples = splitSample - clip->startSample;
        secondPart.startSample = splitSample;
        secondPart.lengthSamples = oldClip.lengthSamples - firstPart.lengthSamples;

        struct SplitClipAction : public juce::UndoableAction
        {
            ArrangementModel* model;
            int trackIndex;
            int clipIndex;
            Clip before;
            Clip first;
            Clip second;

            SplitClipAction(ArrangementModel* m, int t, int c, const Clip& b, const Clip& f, const Clip& s)
                : model(m), trackIndex(t), clipIndex(c), before(b), first(f), second(s) {}

            bool perform() override
            {
                model->replaceClipWithoutUndo(trackIndex, clipIndex, first);
                model->insertClipWithoutUndo(trackIndex, clipIndex + 1, second);
                return true;
            }

            bool undo() override
            {
                model->removeClipWithoutUndo(trackIndex, clipIndex + 1);
                model->replaceClipWithoutUndo(trackIndex, clipIndex, before);
                return true;
            }

            int getSizeInUnits() override { return 1; }
        };

        auto* action = new SplitClipAction(this, selectedTrackIndex, selectedClipIndex, oldClip, firstPart, secondPart);
        undoManager.beginNewTransaction("Split Clip");
        undoManager.perform(action);
        // Update selection to the two resulting clips
        selectedClips.clear();
        selectedClips.add(juce::Point<int>(selectedTrackIndex, selectedClipIndex));
        selectedClips.add(juce::Point<int>(selectedTrackIndex, selectedClipIndex + 1));
        selectedTrackIndex = selectedTrackIndex;
        selectedClipIndex = selectedClipIndex; // keep focus on first part
        return true;
    }

    bool ArrangementModel::splitSelectedClipsAtSample(int64_t samplePosition)
    {
        // Collect targets: either the explicit multi-selection, or single selection
        juce::Array<juce::Point<int>> targets;
        if (!selectedClips.isEmpty())
            targets = selectedClips;
        else if (hasSelection())
            targets.add(juce::Point<int>(selectedTrackIndex, selectedClipIndex));

        if (targets.isEmpty())
            return false;

        // Normalize ordering: sort by track asc, clipIndex desc so edits don't shift following indices
        std::vector<juce::Point<int>> vec;
        vec.reserve((size_t)targets.size());
        for (auto& p : targets) vec.push_back(p);
        std::sort(vec.begin(), vec.end(), [](const juce::Point<int>& a, const juce::Point<int>& b) {
            if (a.x != b.x) return a.x < b.x;
            return a.y > b.y; // descending per-track
        });
        targets.clear();
        for (auto& p : vec) targets.add(p);

        struct SplitInfo { int trackIndex; int clipIndex; Clip before; Clip first; Clip second; };
        juce::Array<SplitInfo> splits;

        for (int i = 0; i < targets.size(); ++i)
        {
            const auto p = targets.getReference(i);
            if (!isPositiveAndBelow(p.x, session.getNumTracks()))
                continue;
            auto& track = session.getTrack(p.x);
            if (!isPositiveAndBelow(p.y, track.clips.size()))
                continue;

            Clip& clip = track.clips.getReference(p.y);
            if (clip.locked)
                continue;

            const int64_t start = clip.startSample;
            const int64_t end = clip.startSample + clip.lengthSamples;
            const int64_t splitSample = juce::jlimit<int64_t>(start + 1, end - 1, samplePosition);
            if (splitSample <= start || splitSample >= end)
                continue; // does not overlap playhead

            Clip before = clip;
            Clip first = clip;
            Clip second = clip;
            first.lengthSamples = splitSample - start;
            second.startSample = splitSample;
            second.lengthSamples = before.lengthSamples - first.lengthSamples;

            SplitInfo info; info.trackIndex = p.x; info.clipIndex = p.y; info.before = before; info.first = first; info.second = second;
            splits.add(info);
        }

        if (splits.isEmpty())
            return false;

        // Create undoable multi-split action
        struct MultiSplitAction : public juce::UndoableAction
        {
            ArrangementModel* model;
            juce::Array<SplitInfo> changes;

            MultiSplitAction(ArrangementModel* m, const juce::Array<SplitInfo>& ch) : model(m), changes(ch) {}

            bool perform() override
            {
                // perform in order (already sorted track asc, clipIndex desc)
                for (int i = 0; i < changes.size(); ++i)
                {
                    const auto& c = changes.getReference(i);
                    model->replaceClipWithoutUndo(c.trackIndex, c.clipIndex, c.first);
                    model->insertClipWithoutUndo(c.trackIndex, c.clipIndex + 1, c.second);
                }
                return true;
            }

            bool undo() override
            {
                // undo in reverse order
                for (int i = changes.size() - 1; i >= 0; --i)
                {
                    const auto& c = changes.getReference(i);
                    model->removeClipWithoutUndo(c.trackIndex, c.clipIndex + 1);
                    model->replaceClipWithoutUndo(c.trackIndex, c.clipIndex, c.before);
                }
                return true;
            }

            int getSizeInUnits() override { return 1; }
        };

        auto* action = new MultiSplitAction(this, splits);
        undoManager.beginNewTransaction("Split Clip(s)");
        undoManager.perform(action);

        // Update selection to include both parts for each split (first and second)
        selectedClips.clear();
        for (int i = 0; i < splits.size(); ++i)
        {
            const auto& s = splits.getReference(i);
            selectedClips.add(juce::Point<int>(s.trackIndex, s.clipIndex));
            selectedClips.add(juce::Point<int>(s.trackIndex, s.clipIndex + 1));
        }

        if (!selectedClips.isEmpty())
        {
            selectedTrackIndex = selectedClips.getFirst().x;
            selectedClipIndex = selectedClips.getFirst().y;
        }

        return true;
    }

    bool ArrangementModel::toggleSelectedClipMute()
    {
        Clip* clip = getSelectedClip();
        if (clip == nullptr)
            return false;

        clip->muted = !clip->muted;
        sendChangeMessage();
        return true;
    }

    bool ArrangementModel::replaceSelectedClipWithUndo(const Clip& newClip, const juce::String& transactionName)
    {
        Clip* clip = getSelectedClip();
        if (clip == nullptr || clip->locked)
            return false;

        Clip oldClip = *clip;
        undoManager.beginNewTransaction(transactionName);
        undoManager.perform(new ReplaceClipAction(this, selectedTrackIndex, selectedClipIndex, oldClip, newClip));
        return true;
    }

    bool ArrangementModel::setSelectedClipGain(float gainDb)
    {
        Clip* clip = getSelectedClip();
        if (clip == nullptr)
            return false;

        Clip newClip = *clip;
        newClip.gainDb = gainDb;
        return replaceSelectedClipWithUndo(newClip, "Adjust Clip Gain");
    }

    bool ArrangementModel::setSelectedClipFadeIn(int64_t samples)
    {
        Clip* clip = getSelectedClip();
        if (clip == nullptr)
            return false;

        Clip newClip = *clip;
        newClip.fadeInSamples = juce::jmax<int64_t>(0, juce::jmin<int64_t>(samples, clip->lengthSamples - 1));
        return replaceSelectedClipWithUndo(newClip, "Adjust Fade In");
    }

    bool ArrangementModel::setSelectedClipFadeOut(int64_t samples)
    {
        Clip* clip = getSelectedClip();
        if (clip == nullptr)
            return false;

        Clip newClip = *clip;
        newClip.fadeOutSamples = juce::jmax<int64_t>(0, juce::jmin<int64_t>(samples, clip->lengthSamples - 1));
        return replaceSelectedClipWithUndo(newClip, "Adjust Fade Out");
    }

    bool ArrangementModel::lockSelectedClip(bool shouldLock)
    {
        Clip* clip = getSelectedClip();
        if (clip == nullptr)
            return false;

        clip->locked = shouldLock;
        sendChangeMessage();
        return true;
    }

    int64_t ArrangementModel::getSnappedSamplePosition(int64_t samplePosition) const noexcept
    {
        if (!snapToGrid)
            return juce::jmax<int64_t>(0, samplePosition);

        const int64_t resolution = snapResolutionSamples();
        if (resolution <= 0)
            return juce::jmax<int64_t>(0, samplePosition);

        const int64_t snapped = ((samplePosition + resolution / 2) / resolution) * resolution;
        return juce::jmax<int64_t>(0, snapped);
    }

    int64_t ArrangementModel::snapResolutionSamples() const noexcept
    {
        const double sampleRate = timelineModel.getSampleRate();
        const double bpm = timelineModel.getTempo();
        if (sampleRate <= 0 || bpm <= 0)
            return 1;

        const double secondsPerBeat = 60.0 / bpm;
        return static_cast<int64_t>(std::round(secondsPerBeat * sampleRate));
    }

    Clip* ArrangementModel::getClipReference(int trackIndex, int clipIndex) const noexcept
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return nullptr;

        auto& track = const_cast<Session&>(session).getTrack(trackIndex);
        if (!isPositiveAndBelow(clipIndex, track.clips.size()))
            return nullptr;

        return &track.clips.getReference(clipIndex);
    }

    void ArrangementModel::replaceClipWithoutUndo(int trackIndex, int clipIndex, const Clip& newClip)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return;

        auto& track = session.getTrack(trackIndex);
        if (!isPositiveAndBelow(clipIndex, track.clips.size()))
            return;

        track.clips.set(clipIndex, newClip);
        sendChangeMessage();
    }

    void ArrangementModel::insertClipWithoutUndo(int trackIndex, int clipIndex, const Clip& clip)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return;

        auto& track = session.getTrack(trackIndex);
        const int insertAt = juce::jlimit(0, track.clips.size(), clipIndex);
        track.clips.insert(insertAt, clip);
        sendChangeMessage();
    }

    void ArrangementModel::removeClipWithoutUndo(int trackIndex, int clipIndex)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return;

        auto& track = session.getTrack(trackIndex);
        if (!isPositiveAndBelow(clipIndex, track.clips.size()))
            return;

        track.clips.remove(clipIndex);
        sendChangeMessage();
    }

    void ArrangementModel::undo()
    {
        if (undoManager.canUndo())
            undoManager.undo();
    }

    void ArrangementModel::redo()
    {
        if (undoManager.canRedo())
            undoManager.redo();
    }

    bool ArrangementModel::canUndo() const
    {
        return undoManager.canUndo();
    }

    bool ArrangementModel::canRedo() const
    {
        return undoManager.canRedo();
    }
}
