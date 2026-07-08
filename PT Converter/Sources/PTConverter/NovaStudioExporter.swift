import Foundation

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
                regionWAVs: [Int: URL],       // regionIndex → WAV file on disk
                audioFileWAVs: [Int: URL],     // audioFileIndex → WAV file on disk
                outputFolder: URL) throws -> URL {

        var tracksJSON = [[String: Any]]()

        for ptTrack in ptSession.tracks {
            var clipsJSON = [[String: Any]]()

            for placement in ptTrack.placements {
                guard let region = ptSession.regions.first(where: { $0.index == placement.regionIndex })
                else { continue }

                // Prefer per-clip WAV if we extracted it, otherwise point at the
                // full audio file WAV with the file-offset filled in.
                let wavURL: URL?
                let fileOffset: Int64
                let clipLength: Int64

                if let clipWAV = regionWAVs[region.index] {
                    wavURL = clipWAV
                    fileOffset = 0
                    clipLength = placement.lengthSamples > 0
                        ? placement.lengthSamples
                        : region.lengthSamples
                } else if let fileWAV = audioFileWAVs[region.fileIndex] {
                    wavURL = fileWAV
                    fileOffset = region.offsetInFileSamples
                    clipLength = placement.lengthSamples > 0
                        ? placement.lengthSamples
                        : region.lengthSamples
                } else if let anyWAV = audioFileWAVs.values.first(where: {
                    // Match by filename stem against region name (strip PT suffixes)
                    let stem = $0.deletingPathExtension().lastPathComponent.lowercased()
                    let rname = region.name.lowercased()
                    return rname.contains(stem) || stem.contains(rname.components(separatedBy: "-").first ?? rname)
                }) {
                    wavURL = anyWAV
                    fileOffset = 0
                    clipLength = placement.lengthSamples > 0 ? placement.lengthSamples : region.lengthSamples
                } else if let firstWAV = audioFileWAVs[audioFileWAVs.keys.sorted().first ?? 0] {
                    // Last resort: use first available WAV so clip at least appears
                    wavURL = firstWAV
                    fileOffset = 0
                    clipLength = placement.lengthSamples > 0 ? placement.lengthSamples : region.lengthSamples
                } else {
                    continue
                }

                guard let url = wavURL else { continue }

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
