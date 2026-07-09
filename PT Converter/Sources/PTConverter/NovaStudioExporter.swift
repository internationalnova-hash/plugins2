import Foundation
import AVFoundation

// ---------------------------------------------------------------------------
// Writes a .novastudio JSON session file that Nova Studio can open directly.
// The format mirrors NovaStudio::Session::saveToFile() in Session.cpp.
// ---------------------------------------------------------------------------

struct NovaExportOptions {
    var sessionName: String
    var sampleRate: Double
    var tempo: Double
}

class NovaStudioExporter {

    // -----------------------------------------------------------------------
    // High-level: given the parsed PT session + the map of
    // (regionIndex → converted WAV URL), write the .novastudio file.
    // -----------------------------------------------------------------------
    func export(ptSession: PTSession,
                regionWAVs: [Int: URL],
                audioFileWAVs: [Int: URL],
                diskWAVs: [(name: String, url: URL, lengthSamples: Int64)],
                outputFolder: URL) throws -> URL {

        // Build a stem → (url, length) map from disk WAVs for name-based matching
        var stemToWAV = [(stem: String, url: URL, length: Int64)]()
        for dw in diskWAVs {
            let stem = dw.url.deletingPathExtension().lastPathComponent.lowercased()
            stemToWAV.append((stem, dw.url, dw.lengthSamples))
        }

        func findWAVByName(_ name: String) -> (url: URL, length: Int64)? {
            let lower = name.lowercased()
            // 1. Exact stem match
            if let m = stemToWAV.first(where: { $0.stem == lower }) { return (m.url, m.length) }
            // 2. Stem contains the name (e.g. WAV "Lead Vox-cm.2" matches region "Lead Vox-cm.2")
            if let m = stemToWAV.first(where: { $0.stem.contains(lower) }) { return (m.url, m.length) }
            // 3. Name contains the stem
            if let m = stemToWAV.first(where: { lower.contains($0.stem) }) { return (m.url, m.length) }
            // 4. Match on first word(s) before a dash or dot
            let prefix = lower.components(separatedBy: CharacterSet(charactersIn: "-._")).first ?? lower
            if prefix.count >= 3, let m = stemToWAV.first(where: { $0.stem.contains(prefix) }) { return (m.url, m.length) }
            return nil
        }

        var tracksJSON = [[String: Any]]()
        // Files already claimed by a track — prevents multiple tracks from getting
        // the same audio file when the parser cannot distinguish per-track assignments.
        var assignedTrackFiles = Set<String>()

        for (trackIdx, ptTrack) in ptSession.tracks.enumerated() {
            var clipsJSON = [[String: Any]]()

            for placement in ptTrack.placements {
                let region = ptSession.regions.first(where: { $0.index == placement.regionIndex })
                let regionName = region?.name ?? ptTrack.name

                var wavURL: URL? = nil
                var fileOffset: Int64 = 0
                var clipLength: Int64 = placement.lengthSamples
                var usedIndexFallback = false

                // Helper: only accept a candidate if it hasn't been used by another track
                func accept(_ url: URL) -> Bool {
                    return !assignedTrackFiles.contains(url.path)
                }

                // 1a. Track name match — most reliable for modern PT sessions where
                //     region names don't correspond to audio file names.
                if let match = findWAVByName(ptTrack.name), accept(match.url) {
                    wavURL = match.url
                    if clipLength == 0 { clipLength = match.length }
                }
                // 1b. Region name match
                if wavURL == nil, let match = findWAVByName(regionName), accept(match.url) {
                    wavURL = match.url
                    if clipLength == 0 { clipLength = match.length }
                }
                // 2. Per-clip extracted WAV
                if wavURL == nil, let r = region, let clipWAV = regionWAVs[r.index], accept(clipWAV) {
                    wavURL = clipWAV
                    if clipLength == 0 { clipLength = r.lengthSamples }
                }
                // 3. Audio file index map (skip fileIndex -1 = deep-scan placeholder)
                if wavURL == nil, let r = region, r.fileIndex >= 0,
                   let fileWAV = audioFileWAVs[r.fileIndex], accept(fileWAV) {
                    wavURL = fileWAV
                    fileOffset = r.offsetInFileSamples
                    if clipLength == 0 { clipLength = r.lengthSamples }
                }
                // 4. Track-index fallback: sequential assignment so every track gets
                //    a unique file even when paths 1-3 produce no usable result.
                if wavURL == nil, !stemToWAV.isEmpty {
                    // Walk forward from trackIdx until we find an unused file
                    for offset in 0 ..< stemToWAV.count {
                        let candidate = stemToWAV[(trackIdx + offset) % stemToWAV.count]
                        if accept(candidate.url) {
                            wavURL = candidate.url
                            if clipLength == 0 { clipLength = candidate.length }
                            usedIndexFallback = true
                            break
                        }
                    }
                }

                guard let url = wavURL else { continue }
                assignedTrackFiles.insert(url.path)

                // Read the actual WAV file length and sample rate once.
                // For stem exports (fileOffset == 0) always use the real file length —
                // PT placement data from binary scans can be garbage. Scale to session
                // sample rate so Nova Studio's timeline (session-rate samples) is correct.
                if let af = try? AVAudioFile(forReading: url) {
                    let fileSR = af.processingFormat.sampleRate
                    if fileOffset == 0 || clipLength == 0 {
                        // Use actual file length when no offset or length is unknown
                        let available = af.length - max(0, fileOffset)
                        clipLength = max(0, available)
                    }
                    if fileSR > 0 && fileSR != ptSession.sampleRate {
                        clipLength = Int64(Double(clipLength) * ptSession.sampleRate / fileSR)
                    }
                }
                guard clipLength > 0 else { continue }

                // For index-fallback tracks, force startSample = 0. Stem exports place
                // all clips at the timeline origin; the binary-scan positions are unreliable.
                let startSample = usedIndexFallback ? 0 : placement.startInTimelineSamples

                let clip: [String: Any] = [
                    "file":               url.path,
                    "startSample":        Double(startSample),
                    "lengthSamples":      Double(clipLength),
                    "fileOffsetSamples":  Double(fileOffset),
                    "gainDb":             0.0,
                    "isMidi":             false,
                    "muted":              false,
                    "locked":             false,
                    "aligned":            false,
                    "isPreview":          false,
                    "alignmentOffsetSamples": 0.0,
                    "originalFile":       url.path,
                    "guideTrackIndex":    -1,
                    "guideClipIndex":     -1,
                    "sourceTrackIndex":   -1,
                    "sourceClipIndex":    -1,
                    "alignVersion":       0,
                    "alignmentTimestamp": "",
                    "alignmentPhraseSensitivity": 0.75,
                    "alignmentConsonantPriority": 0.5,
                    "alignmentSourceGuidePath": "",
                    "alignmentWarpPoints": [] as [Any],
                    "clipColor": "ff4c6af5"
                ]
                clipsJSON.append(clip)
            }

            let trackJSON: [String: Any] = [
                "name":       ptTrack.name,
                "type":       "Audio",
                "isStereo":   ptTrack.isStereo,
                "volumeDb":   -18.0,
                "pan":        0.0,
                "muted":      false,
                "solo":       false,
                "armed":      false,
                "inputBus":   "",
                "outputBus":  "Main Out",
                "auxInputBusIndex": -1,
                "locked":     false,
                "groupName":  "",
                "colour":     "ff4c6af5",
                "sendLevels": [-100.0, -100.0, -100.0, -100.0, -100.0, -100.0],
                "sendBusIndex": [-1, -1, -1, -1, -1, -1],
                "sendPreFader": [false, false, false, false, false, false],
                "automationLanes": [] as [Any],
                "clips":      clipsJSON
            ]
            tracksJSON.append(trackJSON)
        }

        // Tempo map
        var tempoMapJSON = [[String: Any]]()
        if ptSession.tempoMap.isEmpty {
            // Always include at least one tempo point
            tempoMapJSON.append(["timeSeconds": 0.0, "bpm": ptSession.tempoMap.first.map { $0.bpm } ?? 120.0])
        } else {
            for tp in ptSession.tempoMap {
                let secs = Double(tp.positionSamples) / ptSession.sampleRate
                tempoMapJSON.append(["timeSeconds": secs, "bpm": tp.bpm])
            }
        }

        // Markers
        var markersJSON = [[String: Any]]()
        for m in ptSession.markers {
            let secs = Double(m.positionSamples) / ptSession.sampleRate
            markersJSON.append(["name": m.name, "timeSeconds": secs])
        }

        let root: [String: Any] = [
            "projectName":             ptSession.name,
            "tempoBpm":                tempoMapJSON.first?["bpm"] as? Double ?? 120.0,
            "sampleRate":              ptSession.sampleRate,
            "tracks":                  tracksJSON,
            "macros":                  [] as [Any],
            "markers":                 markersJSON,
            "tempoMap":                tempoMapJSON,
            "novaAlignSettings":       [:] as [String: Any],
            "novaAlignPreviewEnabled": true
        ]

        let jsonData = try JSONSerialization.data(withJSONObject: root, options: [.prettyPrinted, .sortedKeys])
        let sessionFile = outputFolder.appendingPathComponent(ptSession.name + ".novastudio")
        try jsonData.write(to: sessionFile)
        return sessionFile
    }

    // -----------------------------------------------------------------------
    // Fallback export: when PTX parsing yielded no tracks but we have audio files,
    // create one track per audio file with clips starting at position 0.
    // -----------------------------------------------------------------------
    func exportFromAudioFiles(sessionName: String,
                              sampleRate: Double,
                              wavFiles: [(name: String, url: URL, lengthSamples: Int64)],
                              startPositions: [String: Int64] = [:],
                              outputFolder: URL) throws -> URL {
        var tracksJSON = [[String: Any]]()

        for (idx, wav) in wavFiles.enumerated() {
            let startSample = Double(startPositions[wav.name] ?? 0)
            let clip: [String: Any] = [
                "file":               wav.url.path,
                "startSample":        startSample,
                "lengthSamples":      Double(wav.lengthSamples),
                "fileOffsetSamples":  0.0,
                "gainDb":             0.0,
                "isMidi":             false,
                "muted":              false,
                "locked":             false,
                "aligned":            false,
                "isPreview":          false,
                "alignmentOffsetSamples": 0.0,
                "originalFile":       wav.url.path,
                "guideTrackIndex":    -1,
                "guideClipIndex":     -1,
                "sourceTrackIndex":   -1,
                "sourceClipIndex":    -1,
                "alignVersion":       0,
                "alignmentTimestamp": "",
                "alignmentPhraseSensitivity": 0.75,
                "alignmentConsonantPriority": 0.5,
                "alignmentSourceGuidePath": "",
                "alignmentWarpPoints": [] as [Any],
                "clipColor": "ff4c6af5"
            ]
            let trackJSON: [String: Any] = [
                "name":       wav.name,
                "type":       "Audio",
                "isStereo":   true,
                "volumeDb":   -18.0,
                "pan":        0.0,
                "muted":      false,
                "solo":       false,
                "armed":      false,
                "inputBus":   "",
                "outputBus":  "Main Out",
                "auxInputBusIndex": -1,
                "locked":     false,
                "groupName":  "",
                "colour":     "ff4c6af5",
                "sendLevels": [-100.0, -100.0, -100.0, -100.0, -100.0, -100.0],
                "sendBusIndex": [-1, -1, -1, -1, -1, -1],
                "sendPreFader": [false, false, false, false, false, false],
                "automationLanes": [] as [Any],
                "clips":      [clip]
            ]
            tracksJSON.append(trackJSON)
            _ = idx
        }

        let root: [String: Any] = [
            "projectName": sessionName,
            "tempoBpm":    120.0,
            "sampleRate":  sampleRate,
            "tracks":      tracksJSON,
            "macros":      [] as [Any],
            "markers":     [] as [Any],
            "tempoMap":    [["timeSeconds": 0.0, "bpm": 120.0]] as [[String: Any]],
            "novaAlignSettings": [:] as [String: Any],
            "novaAlignPreviewEnabled": true
        ]

        let jsonData = try JSONSerialization.data(withJSONObject: root, options: [.prettyPrinted, .sortedKeys])
        let sessionFile = outputFolder.appendingPathComponent(sessionName + ".novastudio")
        try jsonData.write(to: sessionFile)
        return sessionFile
    }
}
