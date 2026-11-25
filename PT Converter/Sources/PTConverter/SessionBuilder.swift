import Foundation
import AVFoundation

// ---------------------------------------------------------------------------
// Turns a parsed Session Info export into a placement plan: every clip on
// every track with its audio file, timeline position, the point inside the
// file where it starts, fades and mute state. No guessing by file names.
//
// Where a clip starts inside its file:
//   A clip's (original/user) timestamp = its file's BWF timestamp + the offset
//   into the file, so offset = clip timestamp - file timestamp. This needs
//   "Include User Timestamps" in the export. Without it, whole-file clips are
//   still exact; edited clips start at the file's beginning (flagged).
// ---------------------------------------------------------------------------

enum SourceKey: Hashable {
    case file (URL)
    case pair (URL, URL)          // split-mono stereo (.L / .R files) -> merged on export
}

struct PlannedClip {
    var name: String
    var source: SourceKey
    var start: Int64              // session samples
    var length: Int64
    var offset: Int64             // session samples into the source
    var fadeIn: Int64 = 0
    var fadeOut: Int64 = 0
    var muted: Bool
    var exactOffset: Bool
}

struct PlannedTrack {
    var name: String
    var stereo: Bool
    var muted: Bool
    var clips: [PlannedClip]
}

struct SessionPlan {
    var name: String
    var sampleRate: Double
    var tracks: [PlannedTrack]
    var markers: [PTTextMarker]
    var warnings: [String]
    var missingFiles: [String]
    var clipCount = 0, exactOffsets = 0, guessedOffsets = 0
    var sessionEnd: Int64 { tracks.flatMap (\.clips).map { $0.start + $0.length }.max() ?? 0 }
}

struct AudioFileInfo { var url: URL; var frames: Int64; var sampleRate: Double; var channels: Int; var timeReference: Int64? }

final class SessionBuilder {
    private let text: PTTextSession
    private let folder: URL
    private var infoCache: [URL: AudioFileInfo] = [:]
    private let converter = AudioConverter()

    init (text: PTTextSession, sessionFolder: URL) { self.text = text; self.folder = sessionFolder }

    // MARK: files
    /// PT location ("Macintosh HD:Users:me:Song:Audio Files:") + name -> file on this Mac
    func resolve (_ fileName: String) -> URL? {
        if let loc = text.files[fileName], !loc.isEmpty {
            var comps = loc.split (separator: ":").map (String.init)
            if !comps.isEmpty {
                let volume = comps.removeFirst()
                let rel = comps.joined (separator: "/") + "/" + fileName
                for base in ["/", "/Volumes/\(volume)/"] {
                    let u = URL (fileURLWithPath: base + rel)
                    if FileManager.default.fileExists (atPath: u.path) { return u }
                }
            }
        }
        if let u = converter.findAudioFile (named: fileName, sessionFolder: folder) { return u }
        // anywhere below the session folder (moved / renamed sub-folders)
        if let en = FileManager.default.enumerator (at: folder, includingPropertiesForKeys: nil, options: [.skipsHiddenFiles]) {
            for case let u as URL in en where u.lastPathComponent == fileName { return u }
        }
        return nil
    }

    func info (_ url: URL) -> AudioFileInfo? {
        if let i = infoCache[url] { return i }
        guard let f = try? AVAudioFile (forReading: url) else { return nil }
        let i = AudioFileInfo (url: url, frames: f.length, sampleRate: f.fileFormat.sampleRate,
                               channels: Int (f.fileFormat.channelCount), timeReference: Self.bwfTimeReference (url))
        infoCache[url] = i
        return i
    }

    /// BWF "bext" TimeReference (samples since midnight), if the file has one
    static func bwfTimeReference (_ url: URL) -> Int64? {
        guard let h = try? FileHandle (forReadingFrom: url) else { return nil }
        defer { try? h.close() }
        guard let hdr = try? h.read (upToCount: 12), hdr.count == 12,
              String (decoding: hdr[0..<4], as: UTF8.self) == "RIFF" || String (decoding: hdr[0..<4], as: UTF8.self) == "RF64",
              String (decoding: hdr[8..<12], as: UTF8.self) == "WAVE" else { return nil }
        var pos: UInt64 = 12
        for _ in 0..<64 {
            try? h.seek (toOffset: pos)
            guard let ch = try? h.read (upToCount: 8), ch.count == 8 else { return nil }
            let id = String (decoding: ch[0..<4], as: UTF8.self)
            let size = UInt64 (ch[4]) | UInt64 (ch[5]) << 8 | UInt64 (ch[6]) << 16 | UInt64 (ch[7]) << 24
            if id == "bext" {
                try? h.seek (toOffset: pos + 8 + 338)
                guard let tr = try? h.read (upToCount: 8), tr.count == 8 else { return nil }
                var v: UInt64 = 0
                for i in 0..<8 { v |= UInt64 (tr[i]) << (8 * UInt64 (i)) }
                return Int64 (v)
            }
            if id == "data" && size == 0xFFFFFFFF { return nil }
            pos += 8 + size + (size & 1)
        }
        return nil
    }

    // MARK: plan
    func build() -> SessionPlan {
        var plan = SessionPlan (name: text.name, sampleRate: text.sampleRate, tracks: [], markers: text.markers,
                                warnings: text.warnings, missingFiles: [])
        var missing = Set<String>()
        let startAbs = text.startSamplesSinceMidnight

        for t in text.tracks {
            let chans = t.channels
            let stereo = chans.count >= 2
            let ch1 = t.clips.filter { $0.channel == (chans.first ?? 1) }.sorted { $0.start < $1.start }
            let ch2 = stereo ? t.clips.filter { $0.channel == chans[1] }.sorted { $0.start < $1.start } : []
            var planned: [PlannedClip] = []
            var pendingFadeIn: Int64 = 0, pendingFadeStart: Int64 = 0

            for (i, c) in ch1.enumerated() {
                switch c.kind {
                case .fadeIn:
                    pendingFadeIn = c.end - c.start; pendingFadeStart = c.start
                    continue
                case .fadeOut:
                    if var last = planned.popLast() {
                        let len = c.end - c.start
                        last.length += len; last.fadeOut = len
                        planned.append (last)
                    }
                    continue
                case .crossfade:
                    // both neighbours play through the crossfade: extend the previous one out,
                    // and the next one in (handled by pendingFadeIn)
                    let len = c.end - c.start
                    if var last = planned.popLast() { last.length += len; last.fadeOut = len; planned.append (last) }
                    pendingFadeIn = len; pendingFadeStart = c.start
                    continue
                case .clip: break
                }

                // source file(s)
                guard let src = text.clipSource[c.name] ?? fallbackSource (c.name) else {
                    plan.warnings.append ("“\(c.name)” on \(t.name): not in the clip list, skipped")
                    continue
                }
                guard let url = resolve (src.file) else { missing.insert (src.file); continue }
                var key = SourceKey.file (url)
                if stereo {
                    // the matching right-channel event (same position)
                    if let r = ch2.first (where: { $0.kind == .clip && $0.start == c.start }) ?? (i < ch2.count ? ch2[i] : nil),
                       let rs = text.clipSource[r.name] ?? fallbackSource (r.name), rs.file != src.file {
                        if let ru = resolve (rs.file) { key = .pair (url, ru) } else { missing.insert (rs.file) }
                    }
                }
                guard let fi = info (url) else { missing.insert (src.file); continue }

                let toFile = fi.sampleRate / text.sampleRate          // session samples -> file frames
                var length = c.end - c.start
                let lenFile = Int64 (Double (length) * toFile)

                // where the clip starts inside the file
                var offsetFile: Int64 = 0
                var exact = false
                if let ts = c.timestamp {
                    let tr = fi.timeReference ?? 0
                    let candidates = [Int64 (Double (ts - tr) * toFile), Int64 (Double (ts + startAbs - tr) * toFile), Int64 (Double (ts) * toFile)]
                    if let ok = candidates.first (where: { $0 >= -2 && $0 + lenFile <= fi.frames + 2 }) {
                        offsetFile = max (0, ok); exact = true
                    }
                }
                if !exact {
                    let stem = url.deletingPathExtension().lastPathComponent
                    let clipBase = c.name.replacingOccurrences (of: ".L", with: "").replacingOccurrences (of: ".R", with: "")
                    if clipBase == stem || clipBase + ".L" == stem || lenFile >= fi.frames - 2 { exact = true }   // whole-file clip
                }
                var start = c.start
                var offset = Int64 (Double (offsetFile) / toFile)
                var fadeIn: Int64 = 0
                if pendingFadeIn > 0 && pendingFadeStart + pendingFadeIn >= c.start - 2 {
                    let pull = min (pendingFadeIn, offset)        // can only reach back as far as the file allows
                    start -= pull; offset -= pull; length += pull; fadeIn = pull
                    pendingFadeIn = 0
                }
                // never read past the end of the file
                let fileLenSession = Int64 (Double (fi.frames) / toFile)
                length = max (0, min (length, fileLenSession - offset))
                guard length > 0 else { continue }

                planned.append (PlannedClip (name: c.name, source: key, start: start, length: length, offset: offset,
                                             fadeIn: fadeIn, fadeOut: 0, muted: c.muted, exactOffset: exact))
                plan.clipCount += 1
                if exact { plan.exactOffsets += 1 } else { plan.guessedOffsets += 1 }
            }
            plan.tracks.append (PlannedTrack (name: t.name, stereo: stereo, muted: t.muted || t.inactive, clips: planned))
        }
        plan.missingFiles = missing.sorted()
        if plan.guessedOffsets > 0 && !text.hasTimestamps {
            plan.warnings.append ("\(plan.guessedOffsets) edited clip(s) may start at the wrong point in their audio file. Re-export with “Include User Timestamps” ticked for exact edits.")
        }
        return plan
    }

    /// Clip name -> file when the clip list wasn't exported: match the file name
    private func fallbackSource (_ clip: String) -> (file: String, channel: Int?)? {
        var base = clip
        for suf in [".L", ".R"] where base.hasSuffix (suf) { base = String (base.dropLast (suf.count)) }
        for f in text.files.keys {
            let stem = (f as NSString).deletingPathExtension
            if stem == clip || stem == base { return (f, nil) }
        }
        // edited clips are named "<file>-01", "<file>_02"... : longest file name that prefixes the clip
        let hits = text.files.keys.filter { base.hasPrefix (($0 as NSString).deletingPathExtension) }
        if let best = hits.max (by: { $0.count < $1.count }) { return (best, nil) }
        return nil
    }
}
