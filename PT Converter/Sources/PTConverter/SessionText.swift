import Foundation

// ---------------------------------------------------------------------------
// Pro Tools "Export Session Info as Text" parser.
//
// This is Avid's own, documented export: it lists every track, every clip on
// it with its exact start/end, muted clips, fades, the clip → audio file map
// and markers. Unlike the binary .ptx format it doesn't change (and isn't
// scrambled) between Pro Tools versions, so it's the reliable way in.
//
// Best export settings (File > Export > Session Info as Text):
//   ✓ Include File List  ✓ Include Clip List  ✓ Include Track EDLs
//   ✓ Show Sub-Frames    ✓ Include User Timestamps   ✓ Include Markers
//   Time Format: Samples
// ---------------------------------------------------------------------------

enum PTTimeFormat: String {
    case samples = "Samples", minSecs = "Min:Secs", timecode = "Timecode",
         barsBeats = "Bars|Beats", feetFrames = "Feet+Frames", unknown = "Unknown"
}

struct PTTextClip {
    enum Kind { case clip, fadeIn, fadeOut, crossfade }
    var channel: Int
    var event: Int
    var name: String
    var start: Int64          // session-relative samples
    var end: Int64
    var timestamp: Int64?     // "user timestamp" column, raw samples as exported (see SessionBuilder)
    var muted: Bool
    var kind: Kind
}

struct PTTextTrack {
    var name: String
    var inactive = false
    var muted = false
    var hidden = false
    var clips: [PTTextClip] = []       // all channels
    var channels: [Int] { Array(Set(clips.map(\.channel))).sorted() }
}

struct PTTextMarker { var name: String; var position: Int64 }

struct PTTextSession {
    var name = ""
    var sampleRate: Double = 48000
    var bitDepth = 24
    var startTimecode = "00:00:00:00"
    var fpsLabel = "30 Frame"
    var timeFormat: PTTimeFormat = .unknown
    var hasTimestamps = false
    var files: [String: String] = [:]            // file name -> PT location ("Macintosh HD:Users:...:")
    var offlineFiles: Set<String> = []
    var clipSource: [String: (file: String, channel: Int?)] = [:]
    var tracks: [PTTextTrack] = []
    var markers: [PTTextMarker] = []
    var warnings: [String] = []

    /// Session start as samples since midnight (for absolute timestamps)
    var startSamplesSinceMidnight: Int64 { PTTimecode.samples(startTimecode, fpsLabel: fpsLabel, sampleRate: sampleRate) ?? 0 }
}

enum PTTextError: LocalizedError {
    case notASessionExport
    case unsupportedTimeFormat(PTTimeFormat)
    var errorDescription: String? {
        switch self {
        case .notASessionExport:
            return "This text file isn't a Pro Tools “Session Info as Text” export."
        case .unsupportedTimeFormat(let f):
            return "This export uses \(f.rawValue) times. Re-export with Time Format set to Samples (Bars|Beats and Feet+Frames can't be placed exactly)."
        }
    }
}

// MARK: - Timecode helpers
enum PTTimecode {
    /// Real frames per second and the nominal (counting) rate for a PT "TIMECODE FORMAT" label
    static func rates (_ label: String) -> (real: Double, nominal: Int, drop: Bool) {
        let l = label.lowercased()
        let drop = l.contains("drop") && !l.contains("non")
        if l.hasPrefix("23.976") { return (24000.0 / 1001.0, 24, false) }
        if l.hasPrefix("24") { return (24, 24, false) }
        if l.hasPrefix("25") { return (25, 25, false) }
        if l.hasPrefix("29.97") { return (30000.0 / 1001.0, 30, drop) }
        if l.hasPrefix("30") { return (30, 30, drop) }
        if l.hasPrefix("47.95") { return (48000.0 / 1001.0, 48, false) }
        if l.hasPrefix("48") { return (48, 48, false) }
        if l.hasPrefix("50") { return (50, 50, false) }
        if l.hasPrefix("59.94") { return (60000.0 / 1001.0, 60, drop) }
        if l.hasPrefix("60") { return (60, 60, false) }
        return (30, 30, false)
    }

    /// "hh:mm:ss:ff(.sub)" or with ';' for drop frame -> samples since 00:00:00:00
    static func samples (_ tc: String, fpsLabel: String, sampleRate: Double) -> Int64? {
        let t = tc.trimmingCharacters(in: .whitespaces)
        var main = t, sub = 0.0
        if let dot = t.firstIndex(of: ".") {
            main = String(t[..<dot])
            sub = Double("0." + t[t.index(after: dot)...]) ?? 0   // sub-frames are hundredths of a frame
        }
        let parts = main.split (whereSeparator: { $0 == ":" || $0 == ";" }).compactMap { Int($0) }
        guard parts.count == 4 else { return nil }
        let r = rates (fpsLabel)
        var frames = ((parts[0] * 60 + parts[1]) * 60 + parts[2]) * r.nominal + parts[3]
        if r.drop {   // drop-frame: 2 (or 4) frame numbers skipped each minute except every 10th
            let dropPerMin = r.nominal / 15
            let totalMinutes = parts[0] * 60 + parts[1]
            frames -= dropPerMin * (totalMinutes - totalMinutes / 10)
        }
        return Int64 ((Double (frames) + sub) / r.real * sampleRate + 0.5)
    }
}

// MARK: - Parser
struct PTSessionTextParser {

    static func load (_ url: URL) throws -> String {
        let data = try Data (contentsOf: url)
        if let s = String (data: data, encoding: .utf8) { return s }
        if let s = String (data: data, encoding: .macOSRoman) { return s }
        return String (decoding: data, as: UTF8.self)
    }

    /// Quick check used when scanning a folder for the export
    static func looksLikeExport (_ url: URL) -> Bool {
        guard let h = try? FileHandle (forReadingFrom: url) else { return false }
        defer { try? h.close() }
        let head = String (decoding: h.readData (ofLength: 64), as: UTF8.self)
        return head.hasPrefix ("SESSION NAME:")
    }

    func parse (_ text: String) throws -> PTTextSession {
        let lines = text.replacingOccurrences (of: "\r\n", with: "\n").replacingOccurrences (of: "\r", with: "\n")
                        .components (separatedBy: "\n")
        guard lines.first?.hasPrefix ("SESSION NAME:") == true else { throw PTTextError.notASessionExport }

        var s = PTTextSession()
        enum Section { case header, onlineFiles, offlineFiles, onlineClips, offlineClips, plugins, tracks, markers, other }
        var section = Section.header

        // raw clip rows: times are parsed after the whole header is known (sample rate, fps)
        struct RawRow { var track: Int; var channel: Int; var event: Int; var name: String; var start: String; var end: String; var ts: String?; var state: String }
        var rows: [RawRow] = []
        var trackCols: [String] = []
        var rawMarkers: [(name: String, location: String, ref: String, units: String)] = []
        var markerCols: [String] = []

        func cols (_ line: String) -> [String] { line.components (separatedBy: "\t").map { $0.trimmingCharacters (in: .whitespaces) } }
        func value (_ line: String) -> String { line.components (separatedBy: "\t").dropFirst().joined (separator: "\t").trimmingCharacters (in: .whitespaces) }

        for line in lines {
            let squashed = line.replacingOccurrences (of: " ", with: "")
            switch squashed {
                case "ONLINEFILESINSESSION": section = .onlineFiles; continue
                case "OFFLINEFILESINSESSION": section = .offlineFiles; continue
                case "ONLINECLIPSINSESSION": section = .onlineClips; continue
                case "OFFLINECLIPSINSESSION": section = .offlineClips; continue
                case "PLUG-INSLISTING": section = .plugins; continue
                case "TRACKLISTING": section = .tracks; continue
                case "MARKERSLISTING": section = .markers; continue
                default: break
            }
            if line.trimmingCharacters (in: .whitespaces).isEmpty { continue }

            switch section {
            case .header:
                if line.hasPrefix ("SESSION NAME:") { s.name = value (line) }
                else if line.hasPrefix ("SAMPLE RATE:") { s.sampleRate = Double (value (line)) ?? 48000 }
                else if line.hasPrefix ("BIT DEPTH:") { s.bitDepth = Int (value (line).filter (\.isNumber)) ?? 24 }
                else if line.hasPrefix ("SESSION START TIMECODE:") { s.startTimecode = value (line) }
                else if line.hasPrefix ("TIMECODE FORMAT:") { s.fpsLabel = value (line) }

            case .onlineFiles, .offlineFiles:
                let c = cols (line)
                if c.first == "Filename" || c.count < 1 { continue }
                if section == .onlineFiles { s.files[c[0]] = c.count > 1 ? c[1] : "" }
                else { s.offlineFiles.insert (c[0]) }

            case .onlineClips, .offlineClips:
                let c = cols (line)
                if c.first == "CLIP NAME" || c.count < 2 { continue }
                var ch: Int? = nil
                if c.count > 2, c[2].hasPrefix ("["), let n = Int (c[2].trimmingCharacters (in: CharacterSet (charactersIn: "[]"))) { ch = n }
                s.clipSource[c[0]] = (c[1], ch)

            case .tracks:
                if line.hasPrefix ("TRACK NAME:") { s.tracks.append (PTTextTrack (name: value (line))); continue }
                guard !s.tracks.isEmpty else { continue }
                if line.hasPrefix ("STATE:") {
                    let st = line.dropFirst (6).split (whereSeparator: { $0 == " " || $0 == "\t" }).map (String.init)
                    s.tracks[s.tracks.count - 1].inactive = st.contains ("Inactive")
                    s.tracks[s.tracks.count - 1].muted = st.contains ("Muted")
                    s.tracks[s.tracks.count - 1].hidden = st.contains ("Hidden")
                    continue
                }
                if line.hasPrefix ("CHANNEL") { trackCols = cols (line).map { $0.uppercased() }; continue }
                if line.hasPrefix ("COMMENTS:") || line.hasPrefix ("USER DELAY:") || line.hasPrefix ("PLUG-INS:") { continue }
                let c = cols (line)
                guard c.count >= 6, let ch = Int (c[0]), let ev = Int (c[1]) else { continue }
                func col (_ name: String) -> String? {
                    guard let i = trackCols.firstIndex (where: { $0.hasPrefix (name) }), i < c.count else { return nil }
                    return c[i]
                }
                rows.append (RawRow (track: s.tracks.count - 1, channel: ch, event: ev,
                                     name: col ("CLIP NAME") ?? c[2], start: col ("START TIME") ?? c[3], end: col ("END TIME") ?? c[4],
                                     ts: col ("TIMESTAMP"), state: col ("STATE") ?? c.last ?? ""))

            case .markers:
                let c = cols (line)
                if c.first == "#" { markerCols = c.map { $0.uppercased() }; continue }
                guard c.count >= 5, Int (c[0]) != nil else { continue }
                func col (_ name: String, _ fallback: Int) -> String {
                    if let i = markerCols.firstIndex (of: name), i < c.count { return c[i] }
                    return fallback < c.count ? c[fallback] : ""
                }
                rawMarkers.append ((col ("NAME", 4), col ("LOCATION", 1), col ("TIME REFERENCE", 2), col ("UNITS", 3)))

            case .plugins, .other: break
            }
        }

        // Work out the time format from the clip times
        let probe = rows.first?.start ?? rawMarkers.first?.location ?? ""
        s.timeFormat = Self.detectFormat (probe)
        if s.timeFormat == .barsBeats || s.timeFormat == .feetFrames { throw PTTextError.unsupportedTimeFormat (s.timeFormat) }
        s.hasTimestamps = rows.contains { $0.ts != nil && !($0.ts ?? "").isEmpty }

        let startAbs = s.startSamplesSinceMidnight
        func toSamples (_ v: String, relativeToSession: Bool = true) -> Int64? {
            let t = v.trimmingCharacters (in: .whitespaces)
            switch s.timeFormat {
            case .samples: return Int64 (t)
            case .minSecs: return Self.minSecs (t).map { Int64 ($0 * s.sampleRate + 0.5) }
            case .timecode:
                guard let abs = PTTimecode.samples (t, fpsLabel: s.fpsLabel, sampleRate: s.sampleRate) else { return nil }
                return relativeToSession ? abs - startAbs : abs
            default: return nil
            }
        }

        for r in rows {
            guard let st = toSamples (r.start), let en = toSamples (r.end) else {
                s.warnings.append ("Couldn't read the time of “\(r.name)” on \(s.tracks[r.track].name)")
                continue
            }
            let lower = r.name.lowercased()
            let kind: PTTextClip.Kind = lower == "(fade in)" ? .fadeIn : lower == "(fade out)" ? .fadeOut
                                      : lower.hasPrefix ("(cross fade") || lower.hasPrefix ("(crossfade") ? .crossfade : .clip
            var ts: Int64? = nil
            if let raw = r.ts, !raw.isEmpty { ts = toSamples (raw, relativeToSession: false) }
            s.tracks[r.track].clips.append (PTTextClip (channel: r.channel, event: r.event, name: r.name, start: st, end: max (st, en),
                                                        timestamp: ts, muted: r.state.lowercased() == "muted", kind: kind))
        }

        for m in rawMarkers {
            // Prefer the absolute sample reference when the marker carries one
            if m.units.lowercased() == "samples", let v = Int64 (m.ref) { s.markers.append (PTTextMarker (name: m.name, position: v)) }
            else if let v = toSamples (m.location) { s.markers.append (PTTextMarker (name: m.name, position: v)) }
        }
        if s.timeFormat == .timecode { s.warnings.append ("Times are in Timecode, so clips snap to whole frames. Export with Time Format: Samples for sample-exact placement.") }
        if s.timeFormat == .minSecs { s.warnings.append ("Times are in Min:Secs (millisecond accuracy). Export with Time Format: Samples for sample-exact placement.") }
        return s
    }

    static func detectFormat (_ v: String) -> PTTimeFormat {
        let t = v.trimmingCharacters (in: .whitespaces)
        if t.isEmpty { return .samples }
        if t.allSatisfy (\.isNumber) { return .samples }
        if t.contains ("|") { return .barsBeats }
        if t.contains ("+") { return .feetFrames }
        let colons = t.filter { $0 == ":" || $0 == ";" }.count
        if colons == 3 { return .timecode }
        if colons >= 1 { return .minSecs }
        return .unknown
    }

    /// "m:ss.mmm" / "h:mm:ss.mmm" -> seconds
    static func minSecs (_ t: String) -> Double? {
        let parts = t.split (separator: ":").map (String.init)
        guard !parts.isEmpty else { return nil }
        var secs = 0.0
        for p in parts { guard let v = Double (p) else { return nil }; secs = secs * 60 + v }
        return secs
    }
}
