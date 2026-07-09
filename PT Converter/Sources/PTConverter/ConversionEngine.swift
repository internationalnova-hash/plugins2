import Foundation
import AVFoundation

// ---------------------------------------------------------------------------
// Orchestrates the full PT → Nova Studio pipeline.
// ---------------------------------------------------------------------------

struct ConversionLog: Identifiable {
    enum Level { case info, warning, success, error }
    let id = UUID()
    let level: Level
    let message: String
}

enum ExportFormat: String, CaseIterable, Identifiable {
    case novaStudio = "Nova Studio"
    case wavStems   = "WAV Stems"
    case aacStems   = "AAC Stems"
    var id: String { rawValue }
}

@MainActor
class ConversionEngine: ObservableObject {

    @Published var log: [ConversionLog] = []
    @Published var progress: Double = 0
    @Published var isRunning = false
    @Published var outputSessionURL: URL?
    @Published var outputFolderURL: URL?
    @Published var exportFormat: ExportFormat = .novaStudio

    private let parser    = PTXParser()
    private let converter = AudioConverter()
    private let exporter  = NovaStudioExporter()

    // -----------------------------------------------------------------------
    func convert(sessionFolder: URL) async {
        guard !isRunning else { return }
        isRunning = true
        log = []
        progress = 0
        outputSessionURL = nil
        outputFolderURL = nil

        do {
            try await runConversion(sessionFolder: sessionFolder)
        } catch {
            addLog(.error, error.localizedDescription)
        }
        isRunning = false
    }

    // -----------------------------------------------------------------------
    private func runConversion(sessionFolder: URL) async throws {
        // ── 1. Find the .ptx file ───────────────────────────────────────────
        addLog(.info, "Looking for .ptx session file…")
        guard let ptxURL = findPTX(in: sessionFolder) else {
            throw NSError(domain: "PTConverter", code: 1,
                          userInfo: [NSLocalizedDescriptionKey: "No .ptx session file found in the dropped folder."])
        }
        addLog(.info, "Found: \(ptxURL.lastPathComponent)")
        progress = 0.05

        // ── 2. Parse ────────────────────────────────────────────────────────
        addLog(.info, "Parsing session…")
        var ptSession: PTSession?
        var parseError: Error?
        do {
            ptSession = try parser.parse(url: ptxURL)
        } catch {
            parseError = error
        }

        if let e = parseError {
            addLog(.warning, "PTX parse note: \(e.localizedDescription) — using audio-file scan")
        }
        progress = 0.15

        // ── 3. Resolve audio files on disk ──────────────────────────────────
        let audioFilesFolder = sessionFolder.appendingPathComponent("Audio Files")
        let diskAudioFiles: [URL]
        if FileManager.default.fileExists(atPath: audioFilesFolder.path) {
            diskAudioFiles = (try? FileManager.default.contentsOfDirectory(
                at: audioFilesFolder,
                includingPropertiesForKeys: nil,
                options: .skipsHiddenFiles)) ?? []
        } else {
            diskAudioFiles = (try? FileManager.default.contentsOfDirectory(
                at: sessionFolder,
                includingPropertiesForKeys: nil,
                options: .skipsHiddenFiles))?.filter { isAudioFile($0) } ?? []
        }

        let audioExtensions: Set<String> = ["wav", "aif", "aiff", "sd2", "mp3", "m4a", "caf"]
        let allDiskAudio = diskAudioFiles.filter { audioExtensions.contains($0.pathExtension.lowercased()) }
        addLog(.info, "Found \(allDiskAudio.count) audio file(s) in session folder")
        progress = 0.20

        // ── 4. Set up output folder ─────────────────────────────────────────
        let sessName = ptSession?.name
            ?? ptxURL.deletingPathExtension().lastPathComponent
        let desktop = FileManager.default.urls(for: .desktopDirectory, in: .userDomainMask).first!
        let outRoot  = desktop.appendingPathComponent("\(sessName) (Nova Studio)")
        let outAudio = outRoot.appendingPathComponent("Audio")
        try FileManager.default.createDirectory(at: outAudio, withIntermediateDirectories: true)
        addLog(.info, "Output → \(outRoot.path)")
        progress = 0.25

        // ── 5. Stem export ──────────────────────────────────────────────────
        if exportFormat == .wavStems || exportFormat == .aacStems {
            try await runStemExport(
                ptSession: ptSession,
                allDiskAudio: allDiskAudio,
                sessName: sessName,
                sessionFolder: sessionFolder,
                outRoot: outRoot,
                outAudio: outAudio
            )
            return
        }

        // ── 6. Convert audio files (Nova Studio path) ───────────────────────
        var audioFileWAVs = [Int: URL]()
        var fallbackWAVs  = [(name: String, url: URL, lengthSamples: Int64)]()

        let parsedFiles: [(index: Int, name: String, src: URL)]
        if let ps = ptSession, !ps.audioFiles.isEmpty {
            parsedFiles = ps.audioFiles.compactMap { af in
                guard let src = converter.findAudioFile(named: af.filename,
                                                        sessionFolder: sessionFolder)
                else {
                    addLog(.warning, "Audio file not found on disk: \(af.filename)")
                    return nil
                }
                return (af.index, af.filename, src)
            }
        } else {
            parsedFiles = []
        }

        let parsedURLs = Set(parsedFiles.map { $0.src })
        let extraDiskFiles: [(index: Int, name: String, src: URL)] = allDiskAudio
            .filter { !parsedURLs.contains($0) }
            .enumerated()
            .map { (parsedFiles.count + $0.offset, $0.element.lastPathComponent, $0.element) }

        let filesToConvert = parsedFiles + extraDiskFiles

        let audioStep = filesToConvert.isEmpty ? 0.0 : 0.50 / Double(filesToConvert.count)
        for (i, entry) in filesToConvert.enumerated() {
            addLog(.info, "Converting [\(i+1)/\(filesToConvert.count)] \(entry.name)…")
            do {
                let result = try converter.convertToWAV(src: entry.src, outDir: outAudio)
                audioFileWAVs[entry.index] = result.outputURL
                fallbackWAVs.append((name: entry.src.deletingPathExtension().lastPathComponent,
                                     url: result.outputURL,
                                     lengthSamples: result.lengthSamples))
                addLog(.success, "✓ \(result.outputURL.lastPathComponent)")
            } catch {
                addLog(.warning, "Could not convert \(entry.name): \(error.localizedDescription)")
            }
            progress = 0.25 + audioStep * Double(i + 1)
        }

        progress = 0.75
        addLog(.info, "Building Nova Studio session…")

        // ── 7. Write .novastudio file ───────────────────────────────────────
        // Strategy: group converted audio files by base stem (track name),
        // pick the best take per group (processed takes > highest take number).
        // This mirrors how working PT converters reconstruct the track layout
        // without relying on unreliable PTX playlist/region block parsing.
        let stemWAVs = stemGroupedWAVs(from: fallbackWAVs)
        let sampleRate = ptSession?.sampleRate ?? 44100
        addLog(.info, "\(stemWAVs.count) track(s) from \(fallbackWAVs.count) file(s)")
        let sessionFile = try exporter.exportFromAudioFiles(
            sessionName: sessName,
            sampleRate: sampleRate,
            wavFiles: stemWAVs,
            outputFolder: outRoot
        )

        progress = 1.0
        outputSessionURL = sessionFile
        outputFolderURL  = outRoot
        addLog(.success, "Done! Session saved to Desktop.")
        addLog(.success, sessionFile.lastPathComponent)
    }

    // -----------------------------------------------------------------------
    // MARK: – Stem export (WAV or AAC)
    // Produces one stem file per PT track, all the same total length, all
    // starting at sample 0. This matches what the reference app exports.
    // -----------------------------------------------------------------------
    private func runStemExport(
        ptSession: PTSession?,
        allDiskAudio: [URL],
        sessName: String,
        sessionFolder: URL,
        outRoot: URL,
        outAudio: URL
    ) async throws {

        let isAAC = exportFormat == .aacStems
        addLog(.info, "Exporting \(isAAC ? "AAC" : "WAV") stems…")

        // Determine which tracks we have and which audio file(s) each maps to.
        // We use the parsed PT tracks when available, or group files by stem otherwise.
        struct StemTrack {
            let name: String
            let sources: [URL]   // audio files for this track (ordered by take)
        }

        var stemTracks = [StemTrack]()

        if let ps = ptSession, !ps.tracks.isEmpty {
            // Use parsed track layout: each placement's regionIndex holds the audio file index
            // (for v90 sessions the parser resolves region→fileIndex in the placement itself).
            for ptTrack in ps.tracks {
                var sources = [URL]()
                var addedIndices = Set<Int>()

                // Primary: use the file index stored in each placement's regionIndex field
                for placement in ptTrack.placements {
                    let fileIdx = placement.regionIndex
                    guard !addedIndices.contains(fileIdx) else { continue }
                    // Look up the audio file by index first
                    if let af = ps.audioFiles.first(where: { $0.index == fileIdx }),
                       let diskURL = converter.findAudioFile(named: af.filename,
                                                             sessionFolder: sessionFolder) {
                        sources.append(diskURL)
                        addedIndices.insert(fileIdx)
                        continue
                    }
                    // Also try via region → file mapping
                    if let region = ps.regions.first(where: { $0.index == fileIdx }),
                       let af = ps.audioFiles.first(where: { $0.index == region.fileIndex }),
                       let diskURL = converter.findAudioFile(named: af.filename,
                                                             sessionFolder: sessionFolder) {
                        if !sources.contains(diskURL) { sources.append(diskURL) }
                        addedIndices.insert(fileIdx)
                    }
                }

                // Fallback: name-match against disk files if index lookup failed
                if sources.isEmpty {
                    let lower = ptTrack.name.lowercased()
                    let matched = allDiskAudio.filter { url in
                        let stem = url.deletingPathExtension().lastPathComponent.lowercased()
                        return stem.contains(lower) || lower.contains(stem)
                    }.sorted { $0.lastPathComponent < $1.lastPathComponent }
                    // Among matches, prefer processed takes ("-Gain", etc.)
                    let processed = matched.filter {
                        let s = $0.deletingPathExtension().lastPathComponent.lowercased()
                        return s.contains("-gain") || s.contains("-norm") || s.contains("-tmshft")
                    }
                    sources = processed.isEmpty ? matched : processed
                }

                if !sources.isEmpty {
                    stemTracks.append(StemTrack(name: ptTrack.name, sources: sources))
                }
            }
        }

        // If no tracks were resolved, fall back to grouping disk files by stem
        if stemTracks.isEmpty {
            var groups: [(stem: String, urls: [URL])] = []
            var stemIndex = [String: Int]()
            for url in allDiskAudio.sorted(by: { $0.lastPathComponent < $1.lastPathComponent }) {
                var s = url.deletingPathExtension().lastPathComponent
                // Strip trailing take number
                if let m = s.range(of: #"[\s_\-]+\d+$"#, options: .regularExpression) {
                    s = String(s[s.startIndex ..< m.lowerBound])
                }
                s = s.trimmingCharacters(in: .whitespaces)
                if let idx = stemIndex[s] {
                    groups[idx].urls.append(url)
                } else {
                    stemIndex[s] = groups.count
                    groups.append((stem: s, urls: [url]))
                }
            }
            stemTracks = groups.map { StemTrack(name: $0.stem, sources: $0.urls) }
        }

        addLog(.info, "Exporting \(stemTracks.count) stem tracks…")

        // Find the maximum duration across all source files so all stems are the same length
        var maxDurationSamples: Int64 = 0
        let sampleRate = ptSession?.sampleRate ?? 44100.0
        for st in stemTracks {
            for url in st.sources {
                if let af = try? AVAudioFile(forReading: url) {
                    let dur = Int64(Double(af.length) * sampleRate / af.processingFormat.sampleRate)
                    maxDurationSamples = max(maxDurationSamples, dur)
                }
            }
        }

        let stemsFolder = outRoot.appendingPathComponent("Stems")
        try FileManager.default.createDirectory(at: stemsFolder, withIntermediateDirectories: true)

        let step = stemTracks.isEmpty ? 0.0 : 0.70 / Double(stemTracks.count)
        var exportedURLs = [URL]()

        for (i, st) in stemTracks.enumerated() {
            addLog(.info, "[\(i+1)/\(stemTracks.count)] \(st.name)…")
            let safeName = st.name.components(separatedBy: CharacterSet(charactersIn: "/\\:*?\"<>|")).joined(separator: "_")
            let ext = isAAC ? "m4a" : "wav"
            let destURL = stemsFolder.appendingPathComponent("\(safeName).\(ext)")

            do {
                if isAAC {
                    try exportStemAAC(sources: st.sources, dest: destURL,
                                      targetDurationSamples: maxDurationSamples,
                                      sessionSampleRate: sampleRate)
                } else {
                    try exportStemWAV(sources: st.sources, dest: destURL,
                                      targetDurationSamples: maxDurationSamples,
                                      sessionSampleRate: sampleRate)
                }
                exportedURLs.append(destURL)
                addLog(.success, "✓ \(destURL.lastPathComponent)")
            } catch {
                addLog(.warning, "Could not export \(st.name): \(error.localizedDescription)")
            }
            progress = 0.25 + step * Double(i + 1)
        }

        progress = 1.0
        outputFolderURL = stemsFolder
        addLog(.success, "Done! \(exportedURLs.count) stems saved to Desktop.")
    }

    // -----------------------------------------------------------------------
    private func exportStemWAV(sources: [URL], dest: URL,
                               targetDurationSamples: Int64,
                               sessionSampleRate: Double) throws {
        // Use first source's format for output
        guard let firstFile = try? AVAudioFile(forReading: sources[0]) else {
            throw NSError(domain: "PTConverter", code: 10,
                          userInfo: [NSLocalizedDescriptionKey: "Cannot open \(sources[0].lastPathComponent)"])
        }
        let outRate = min(firstFile.processingFormat.sampleRate, 48000)
        let chCount = firstFile.processingFormat.channelCount

        let outSettings: [String: Any] = [
            AVFormatIDKey:             kAudioFormatLinearPCM,
            AVSampleRateKey:           outRate,
            AVNumberOfChannelsKey:     chCount,
            AVLinearPCMBitDepthKey:    24,
            AVLinearPCMIsFloatKey:     false,
            AVLinearPCMIsBigEndianKey: false,
            AVLinearPCMIsNonInterleaved: false
        ]
        let outFile = try AVAudioFile(forWriting: dest, settings: outSettings,
                                      commonFormat: .pcmFormatFloat32, interleaved: true)

        // Write each source back-to-back starting at position 0
        for srcURL in sources {
            guard let inFile = try? AVAudioFile(forReading: srcURL) else { continue }
            let bufSize = AVAudioFrameCount(65536)
            guard let buf = AVAudioPCMBuffer(pcmFormat: outFile.processingFormat,
                                             frameCapacity: bufSize) else { continue }
            let len = inFile.length
            var written: Int64 = 0
            while written < len {
                let toRead = AVAudioFrameCount(min(Int64(bufSize), len - written))
                buf.frameLength = toRead
                try? inFile.read(into: buf, frameCount: toRead)
                try? outFile.write(from: buf)
                written += Int64(toRead)
            }
        }

        // Pad with silence to match the longest track
        let writtenSamples = outFile.length
        let targetInOutRate = Int64(Double(targetDurationSamples) * outRate / sessionSampleRate)
        if writtenSamples < targetInOutRate {
            let silenceCount = AVAudioFrameCount(targetInOutRate - writtenSamples)
            if let silBuf = AVAudioPCMBuffer(pcmFormat: outFile.processingFormat,
                                             frameCapacity: silenceCount) {
                silBuf.frameLength = silenceCount
                // Buffer is already zeroed
                try? outFile.write(from: silBuf)
            }
        }
    }

    // -----------------------------------------------------------------------
    private func exportStemAAC(sources: [URL], dest: URL,
                               targetDurationSamples: Int64,
                               sessionSampleRate: Double) throws {
        // Write a temp WAV first, then encode to AAC via AVAssetExportSession
        let tmpWAV = dest.deletingPathExtension().appendingPathExtension("_tmp.wav")
        try exportStemWAV(sources: sources, dest: tmpWAV,
                          targetDurationSamples: targetDurationSamples,
                          sessionSampleRate: sessionSampleRate)
        defer { try? FileManager.default.removeItem(at: tmpWAV) }

        let asset = AVURLAsset(url: tmpWAV)
        guard let session = AVAssetExportSession(asset: asset,
                                                  presetName: AVAssetExportPresetAppleM4A) else {
            throw NSError(domain: "PTConverter", code: 11,
                          userInfo: [NSLocalizedDescriptionKey: "Cannot create AAC export session"])
        }
        session.outputURL = dest
        session.outputFileType = .m4a

        let sema = DispatchSemaphore(value: 0)
        session.exportAsynchronously { sema.signal() }
        sema.wait()

        if let err = session.error { throw err }
    }

    // -----------------------------------------------------------------------
    private func findPTX(in folder: URL) -> URL? {
        let fm = FileManager.default
        guard let items = try? fm.contentsOfDirectory(at: folder,
                                                       includingPropertiesForKeys: nil,
                                                       options: .skipsHiddenFiles)
        else { return nil }
        return items.first { ["ptx", "pts"].contains($0.pathExtension.lowercased()) }
    }

    private func isAudioFile(_ url: URL) -> Bool {
        let ext = url.pathExtension.lowercased()
        return ["wav", "aif", "aiff", "sd2", "mp3", "m4a", "caf"].contains(ext)
    }

    // Group converted WAVs by base stem; pick the best take per stem.
    // "Audio 1_16-Gain.wav" and "Audio 1_01.wav" → stem "Audio 1",
    // winner = processed take (contains -Gain/-Norm) or highest take number.
    private func stemGroupedWAVs(from wavs: [(name: String, url: URL, lengthSamples: Int64)])
        -> [(name: String, url: URL, lengthSamples: Int64)]
    {
        func baseStem(_ name: String) -> String {
            var s = name
            for sfx in [".L", ".R", "_L", "_R"] {
                if s.lowercased().hasSuffix(sfx.lowercased()) { s = String(s.dropLast(sfx.count)) }
            }
            if let r = s.range(of: "_\\d+$", options: .regularExpression) { s = String(s[..<r.lowerBound]) }
            if let r = s.range(of: "-[^-_]+$", options: .regularExpression) { s = String(s[..<r.lowerBound]) }
            if let r = s.range(of: "_\\d+$", options: .regularExpression) { s = String(s[..<r.lowerBound]) }
            return s
        }
        func takeNum(_ name: String) -> Int {
            if let r = name.range(of: "\\d+$", options: .regularExpression) { return Int(name[r]) ?? 0 }
            return 0
        }

        var stemOrder  = [String]()
        var stemGroups = [String: [(name: String, url: URL, lengthSamples: Int64)]]()
        for wav in wavs {
            let stem = baseStem(wav.name)
            if stemGroups[stem] == nil { stemOrder.append(stem); stemGroups[stem] = [] }
            stemGroups[stem]!.append(wav)
        }

        var result: [(name: String, url: URL, lengthSamples: Int64)] = []
        for stem in stemOrder {
            guard let group = stemGroups[stem], !group.isEmpty else { continue }
            let processed = group.filter {
                let n = $0.name.lowercased()
                return n.contains("-gain") || n.contains("-norm") || n.contains("-tmshft") || n.contains("-rev")
            }
            let pool = processed.isEmpty ? group : processed
            if let best = pool.max(by: { takeNum($0.name) < takeNum($1.name) }) {
                result.append((name: stem, url: best.url, lengthSamples: best.lengthSamples))
            }
        }
        return result
    }

    private func addLog(_ level: ConversionLog.Level, _ msg: String) {
        log.append(ConversionLog(level: level, message: msg))
    }
}
