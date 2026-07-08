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

        for ptTrack in ptSession.tracks {
            var clipsJSON = [[String: Any]]()

            for placement in ptTrack.placements {
                let region = ptSession.regions.first(where: { $0.index == placement.regionIndex })
                let regionName = region?.name ?? ptTrack.name

                var wavURL: URL? = nil
                var fileOffset: Int64 = 0
                var clipLength: Int64 = placement.lengthSamples

                // 1. Named disk WAV match (primary path for scanRegionEntries results)
                if let match = findWAVByName(regionName) {
                    wavURL = match.url
                    if clipLength == 0 { clipLength = match.length }
                }
                // 2. Per-clip extracted WAV
                if wavURL == nil, let r = region, let clipWAV = regionWAVs[r.index] {
                    wavURL = clipWAV
                    if clipLength == 0 { clipLength = r.lengthSamples }
                }
                // 3. Audio file index map
                if wavURL == nil, let r = region, let fileWAV = audioFileWAVs[r.fileIndex] {
                    wavURL = fileWAV
                    fileOffset = r.offsetInFileSamples
                    if clipLength == 0 { clipLength = r.lengthSamples }
                }
                // 4. Any disk WAV at all (last resort)
                if wavURL == nil, let first = stemToWAV.first {
                    wavURL = first.url
                    if clipLength == 0 { clipLength = first.length }
                }

                guard let url = wavURL else { continue }

                // Read length from file if still unknown
                if clipLength == 0, let af = try? AVAudioFile(forReading: url) {
                    clipLength = af.length
                }
                guard clipLength > 0 else { continue }

                // lengthSamples from disk WAVs is in the file's native sample rate
                // (48 kHz after AudioConverter). Scale to session sample rate so Nova
                // Studio's timeline (which counts in session-rate samples) is correct.
                // Without this, a 96 kHz session shows every clip at half its real length.
                if let af = try? AVAudioFile(forReading: url) {
                    let fileSR = af.processingFormat.sampleRate
                    if fileSR > 0 && fileSR != ptSession.sampleRate {
                        clipLength = Int64(Double(clipLength) * ptSession.sampleRate / fileSR)
                    }
                }

                let clip: [String: Any] = [
                    "file":               url.path,
                    "startSample":        Double(placement.startInTimelineSamples),
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
                              outputFolder: URL) throws -> URL {
        var tracksJSON = [[String: Any]]()

        for (idx, wav) in wavFiles.enumerated() {
            let clip: [String: Any] = [
                "file":               wav.url.path,
                "startSample":        0.0,
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
