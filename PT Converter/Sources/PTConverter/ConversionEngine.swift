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

@MainActor
class ConversionEngine: ObservableObject {

    @Published var log: [ConversionLog] = []
    @Published var progress: Double = 0
    @Published var isRunning = false
    @Published var outputSessionURL: URL?
    @Published var outputFolderURL: URL?

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
            // Scan session folder root
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

        // ── 5. Convert audio files ──────────────────────────────────────────
        var audioFileWAVs = [Int: URL]()   // PT file index → converted WAV
        var fallbackWAVs  = [(name: String, url: URL, lengthSamples: Int64)]()

        let filesToConvert: [(index: Int, name: String, src: URL)]
        if let ps = ptSession, !ps.audioFiles.isEmpty {
            filesToConvert = ps.audioFiles.compactMap { af in
                guard let src = converter.findAudioFile(named: af.filename,
                                                        sessionFolder: sessionFolder)
                else {
                    addLog(.warning, "Audio file not found on disk: \(af.filename)")
                    return nil
                }
                return (af.index, af.filename, src)
            }
        } else {
            // No parsed audio files – use everything we found on disk
            filesToConvert = allDiskAudio.enumerated().map { (i, url) in
                (i, url.lastPathComponent, url)
            }
        }

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

        // ── 6. Write .novastudio file ───────────────────────────────────────
        let sessionFile: URL
        if let ps = ptSession, !ps.tracks.isEmpty {
            sessionFile = try exporter.export(
                ptSession: ps,
                regionWAVs: [:],
                audioFileWAVs: audioFileWAVs,
                diskWAVs: fallbackWAVs,
                outputFolder: outRoot
            )
            let trackCount = ps.tracks.count
            let clipCount  = ps.tracks.reduce(0) { $0 + $1.placements.count }
            addLog(.info, "\(trackCount) tracks, \(clipCount) clip placements")
        } else {
            addLog(.warning, "No track layout parsed — creating one track per audio file")
            sessionFile = try exporter.exportFromAudioFiles(
                sessionName: sessName,
                sampleRate: ptSession?.sampleRate ?? 44100,
                wavFiles: fallbackWAVs,
                outputFolder: outRoot
            )
        }

        progress = 1.0
        outputSessionURL = sessionFile
        outputFolderURL  = outRoot
        addLog(.success, "Done! Session saved to Desktop.")
        addLog(.success, sessionFile.lastPathComponent)
    }

    // -----------------------------------------------------------------------
    private func findPTX(in folder: URL) -> URL? {
        let fm = FileManager.default
        guard let items = try? fm.contentsOfDirectory(at: folder,
                                                       includingPropertiesForKeys: nil,
                                                       options: .skipsHiddenFiles)
        else { return nil }
        // .ptx = PT10+, .pts = PT9 (we support both)
        return items.first { ["ptx", "pts"].contains($0.pathExtension.lowercased()) }
    }

    private func isAudioFile(_ url: URL) -> Bool {
        let ext = url.pathExtension.lowercased()
        return ["wav", "aif", "aiff", "sd2", "mp3", "m4a", "caf"].contains(ext)
    }

    private func addLog(_ level: ConversionLog.Level, _ msg: String) {
        log.append(ConversionLog(level: level, message: msg))
    }
}
