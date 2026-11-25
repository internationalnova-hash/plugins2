import Foundation
import AVFoundation

// ---------------------------------------------------------------------------
// Reads a .ptx directly with ptformat (Ardour's open-source Pro Tools session
// reader, run as the bundled `ptx2json` helper) and turns it into the same
// SessionPlan the text-export path produces: every clip on every track at its
// real position with its exact offset into the audio file.
//
// ptformat lists a stereo track as two channel entries with the same name
// (…L.wav / …R.wav); those are put back together as one stereo track.
// ---------------------------------------------------------------------------

struct PTXJSON: Decodable {
    struct File: Decodable { let name: String; let length: Int64 }
    struct Clip: Decodable { let file: String; let start: Int64; let offset: Int64; let length: Int64; let region: String }
    struct Track: Decodable { let index: Int; let name: String; let clips: [Clip] }
    struct MidiClip: Decodable { let start: Int64; let length: Int64; let notes: [[Int64]] }
    struct MidiTrack: Decodable { let index: Int; let name: String; let clips: [MidiClip] }
    let ok: Bool
    let error: String?
    let version: Int?
    let sampleRate: Double?
    let files: [File]?
    let tracks: [Track]?
    let midiTracks: [MidiTrack]?
}

enum PTXReaderError: LocalizedError {
    case helperMissing, failed (String)
    var errorDescription: String? {
        switch self {
        case .helperMissing: return "The session reader (ptx2json) is missing from the app."
        case .failed (let s): return s
        }
    }
}

struct PTXReader {
    /// The helper sits next to the app's executable (or set NOVA_PTX2JSON for tests)
    static func helperURL() -> URL? {
        if let p = ProcessInfo.processInfo.environment["NOVA_PTX2JSON"] { return URL (fileURLWithPath: p) }
        if let u = Bundle.main.url (forAuxiliaryExecutable: "ptx2json") { return u }
        let beside = URL (fileURLWithPath: CommandLine.arguments[0]).deletingLastPathComponent().appendingPathComponent ("ptx2json")
        return FileManager.default.isExecutableFile (atPath: beside.path) ? beside : nil
    }

    static func read (_ ptx: URL) throws -> PTXJSON {
        guard let helper = helperURL() else { throw PTXReaderError.helperMissing }
        let p = Process()
        p.executableURL = helper
        p.arguments = [ptx.path]
        let out = Pipe()
        p.standardOutput = out
        p.standardError = Pipe()
        try p.run()
        let data = out.fileHandleForReading.readDataToEndOfFile()
        p.waitUntilExit()
        let j = try JSONDecoder().decode (PTXJSON.self, from: data)
        if !j.ok { throw PTXReaderError.failed (j.error ?? "Couldn't read the session") }
        return j
    }

    /// Finds a session's audio file the way Pro Tools would:
    ///  1. anywhere inside the session folder
    ///  2. the "Audio Files" folders of nearby sessions (a "Save As" into a new folder keeps
    ///     using the original session's audio)
    ///  3. Spotlight, anywhere on this Mac (preferring files in an "Audio Files" folder)
    static func find (_ name: String, in folder: URL, cache: inout [String: URL]) -> URL? {
        let fm = FileManager.default
        if cache.isEmpty {
            func index (_ dir: URL, depth: Int) {
                guard let en = fm.enumerator (at: dir, includingPropertiesForKeys: nil, options: [.skipsHiddenFiles]) else { return }
                for case let u as URL in en {
                    if en.level > depth { en.skipDescendants(); continue }
                    if cache[u.lastPathComponent] == nil { cache[u.lastPathComponent] = u }
                }
            }
            index (folder, depth: 6)
            // nearby sessions: parent and grandparent "Audio Files" folders, and their siblings' ones
            var up = folder
            for _ in 0..<2 {
                up = up.deletingLastPathComponent()
                let direct = up.appendingPathComponent ("Audio Files")
                if fm.fileExists (atPath: direct.path) { index (direct, depth: 2) }
                if let kids = try? fm.contentsOfDirectory (at: up, includingPropertiesForKeys: nil, options: [.skipsHiddenFiles]) {
                    for k in kids.prefix (200) {
                        let af = k.appendingPathComponent ("Audio Files")
                        if fm.fileExists (atPath: af.path) { index (af, depth: 2) }
                    }
                }
            }
            cache["\u{0}indexed"] = folder
        }
        if let u = cache[name] { return u }
        // Spotlight, last resort
        let p = Process()
        p.executableURL = URL (fileURLWithPath: "/usr/bin/mdfind")
        p.arguments = ["-name", name]
        let pipe = Pipe(); p.standardOutput = pipe; p.standardError = Pipe()
        guard (try? p.run()) != nil else { return nil }
        let out = String (decoding: pipe.fileHandleForReading.readDataToEndOfFile(), as: UTF8.self)
        p.waitUntilExit()
        let hits = out.split (separator: "\n").map { URL (fileURLWithPath: String ($0)) }.filter { $0.lastPathComponent == name }
        let best = hits.first { $0.path.contains ("/Audio Files/") } ?? hits.first
        if let b = best { cache[name] = b }
        return best
    }

    static func plan (_ j: PTXJSON, sessionName: String, sessionFolder: URL) -> SessionPlan {
        let rate = j.sampleRate ?? 48000
        var plan = SessionPlan (name: sessionName, sampleRate: rate, tracks: [], markers: [], warnings: [], missingFiles: [])
        var cache: [String: URL] = [:]
        var missing = Set<String>()
        let tracks = j.tracks ?? []

        func baseName (_ f: String) -> String {
            var s = (f as NSString).deletingPathExtension
            for suf in [".L", ".R", "_L", "_R", ".l", ".r"] where s.hasSuffix (suf) { s = String (s.dropLast (suf.count)); break }
            return s
        }

        var i = 0
        while i < tracks.count {
            let t = tracks[i]
            // a stereo track = this entry + the next one with the same name, same clip layout
            var right: PTXJSON.Track? = nil
            if i + 1 < tracks.count, tracks[i + 1].name == t.name, tracks[i + 1].index == t.index + 1,
               tracks[i + 1].clips.count == t.clips.count {
                right = tracks[i + 1]
            }
            var clips: [PlannedClip] = []
            for (ci, c) in t.clips.enumerated() {
                guard let lu = find (c.file, in: sessionFolder, cache: &cache) else { missing.insert (c.file); continue }
                var key = SourceKey.file (lu)
                if let r = right {
                    let rc = r.clips[ci]
                    if rc.file != c.file {
                        if let ru = find (rc.file, in: sessionFolder, cache: &cache) { key = .pair (lu, ru) } else { missing.insert (rc.file) }
                    }
                }
                // never read past the file's end
                var length = c.length
                if let f = try? AVAudioFile (forReading: lu) {
                    let fileLenSession = Int64 (Double (f.length) * rate / f.fileFormat.sampleRate)
                    length = max (0, min (length, fileLenSession - c.offset))
                }
                guard length > 0 else { continue }
                clips.append (PlannedClip (name: c.region, source: key, start: c.start, length: length, offset: c.offset,
                                           muted: false, exactOffset: true))
                plan.clipCount += 1
                plan.exactOffsets += 1
            }
            clips.sort { $0.start < $1.start }
            // Pro Tools never plays two clips on one track at once: a clip laid over another
            // hides the part underneath, so the earlier clip ends where the next one begins.
            for k in 1..<max (1, clips.count) where clips[k - 1].start + clips[k - 1].length > clips[k].start {
                clips[k - 1].length = max (0, clips[k].start - clips[k - 1].start)
                if clips[k - 1].fadeOut > clips[k - 1].length { clips[k - 1].fadeOut = clips[k - 1].length }
            }
            clips.removeAll { $0.length <= 0 }
            plan.tracks.append (PlannedTrack (name: t.name, stereo: right != nil, muted: false, clips: clips))
            i += right != nil ? 2 : 1
        }
        if let midi = j.midiTracks, !midi.isEmpty {
            plan.warnings.append ("\(midi.count) MIDI track(s) found — MIDI isn't converted yet, only audio.")
        }
        plan.missingFiles = missing.sorted()
        return plan
    }
}
