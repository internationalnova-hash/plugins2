import Foundation

// ---------------------------------------------------------------------------
// Pro Tools .ptx / .pts binary parser
// Based on the Ardour ptformat reverse-engineering (LGPL community work).
// Handles PT 10-2024 sessions. Gracefully degrades on unknown blocks.
// ---------------------------------------------------------------------------

enum PTXError: LocalizedError {
    case fileNotFound
    case tooSmall
    case badMagic
    case unsupportedVersion(Int)
    case decodeFailed(String)

    var errorDescription: String? {
        switch self {
        case .fileNotFound:          return "Session file not found"
        case .tooSmall:              return "File too small to be a PT session"
        case .badMagic:              return "Not a Pro Tools session file"
        case .unsupportedVersion(let v): return "Unsupported PT version \(v)"
        case .decodeFailed(let m):   return "Parse error: \(m)"
        }
    }
}

struct PTAudioFile {
    let index: Int
    let filename: String
    var lengthSamples: Int64 = 0
}

struct PTRegion {
    let index: Int
    let name: String
    let fileIndex: Int
    let offsetInFileSamples: Int64   // where in the audio file this region starts
    let lengthSamples: Int64
}

struct PTClipPlacement {
    let regionIndex: Int
    let startInTimelineSamples: Int64
    let lengthSamples: Int64
}

struct PTTrack {
    let index: Int
    let name: String
    let placements: [PTClipPlacement]
    let isStereo: Bool
}

struct PTTempoPoint {
    let positionSamples: Int64
    let bpm: Double
}

struct PTMarker {
    let name: String
    let positionSamples: Int64
}

struct PTSession {
    let name: String
    let version: Int
    let sampleRate: Double
    let bitDepth: Int
    var audioFiles: [PTAudioFile]
    var regions: [PTRegion]
    var tracks: [PTTrack]
    var tempoMap: [PTTempoPoint]
    var markers: [PTMarker]
}

// ---------------------------------------------------------------------------
// MARK: – Parser
// ---------------------------------------------------------------------------

class PTXParser {

    // PT magic bytes at file start
    private static let magic: [UInt8] = [0x03, 0x00]

    func parse(url: URL) throws -> PTSession {
        let raw = try Data(contentsOf: url, options: .mappedIfSafe)
        guard raw.count > 0x40 else { throw PTXError.tooSmall }

        // Basic magic check – first byte should be 0x03 for all modern PT sessions
        guard raw[0] == 0x03 else { throw PTXError.badMagic }

        let version = Int(raw[0x14])
        guard version >= 7 && version <= 30 else {
            throw PTXError.unsupportedVersion(version)
        }

        // XOR key byte is at 0x1a for PT10+, 0x11 for older
        let keyOffset: Int = version >= 10 ? 0x1a : 0x11
        let xorKey = raw[keyOffset]

        // Decrypt the session data (everything after the key byte)
        let decryptStart = keyOffset + 1
        var buf = Array(raw)
        if xorKey != 0 {
            for i in decryptStart ..< buf.count {
                buf[i] ^= xorKey
            }
        }

        let data = Data(buf)

        // Parse sample rate from header (big-endian uint32 at 0x20 in PT10+)
        let sampleRate = parseSampleRate(data: data, version: version)
        let bitDepth   = parseBitDepth(data: data, version: version)

        var audioFiles = [PTAudioFile]()
        var regions    = [PTRegion]()
        var tracks     = [PTTrack]()
        var tempoMap   = [PTTempoPoint]()
        var markers    = [PTMarker]()

        // Walk all blocks in the decrypted buffer
        parseAllBlocks(data: data, version: version, sampleRate: sampleRate,
                       audioFiles: &audioFiles, regions: &regions,
                       tracks: &tracks, tempoMap: &tempoMap, markers: &markers)

        // Fallback: if we found no audio files at all, scan for strings with known extensions
        if audioFiles.isEmpty {
            audioFiles = scanForAudioFilenames(data: data)
        }

        let sessionName = url.deletingPathExtension().lastPathComponent

        return PTSession(
            name: sessionName,
            version: version,
            sampleRate: sampleRate,
            bitDepth: bitDepth,
            audioFiles: audioFiles,
            regions: regions,
            tracks: tracks,
            tempoMap: tempoMap,
            markers: markers
        )
    }

    // -----------------------------------------------------------------------
    // MARK: – Header fields
    // -----------------------------------------------------------------------

    private func parseSampleRate(data: Data, version: Int) -> Double {
        // PT10+ stores sample rate as big-endian uint32 at offset 0x20
        let offset = version >= 10 ? 0x20 : 0x18
        guard data.count > offset + 3 else { return 44100 }
        let hi = UInt32(data[offset])
        let m1 = UInt32(data[offset + 1])
        let m2 = UInt32(data[offset + 2])
        let lo = UInt32(data[offset + 3])
        let raw = (hi << 24) | (m1 << 16) | (m2 << 8) | lo
        let sr = Double(raw)
        // Sanity check – standard rates
        let valid: [Double] = [44100, 48000, 88200, 96000, 176400, 192000]
        return valid.min(by: { abs($0 - sr) < abs($1 - sr) }) ?? 44100
    }

    private func parseBitDepth(data: Data, version: Int) -> Int {
        // Bit depth hint often at 0x24 or 0x28
        let offset = version >= 10 ? 0x24 : 0x1c
        guard data.count > offset else { return 24 }
        let raw = Int(data[offset])
        if raw == 16 || raw == 24 || raw == 32 { return raw }
        return 24
    }

    // -----------------------------------------------------------------------
    // MARK: – Block walker
    // -----------------------------------------------------------------------
    // PT sessions are organized as typed blocks. We scan for known signatures.

    private struct Block {
        let type: UInt16
        let contentOffset: Int
        let contentLength: Int
    }

    private func parseAllBlocks(data: Data,
                                version: Int,
                                sampleRate: Double,
                                audioFiles: inout [PTAudioFile],
                                regions: inout [PTRegion],
                                tracks: inout [PTTrack],
                                tempoMap: inout [PTTempoPoint],
                                markers: inout [PTMarker]) {

        // Block scan start: skip header (0x40 bytes is safe for all versions)
        var pos = 0x40
        let end = data.count - 6

        while pos < end {
            guard let block = readBlock(data: data, at: pos) else {
                pos += 1
                continue
            }

            switch block.type {
            case 0x1001: // Audio files table
                parseAudioFilesBlock(data: data, block: block,
                                     sampleRate: sampleRate,
                                     audioFiles: &audioFiles)
            case 0x1003: // Regions
                parseRegionsBlock(data: data, block: block,
                                  sampleRate: sampleRate,
                                  regions: &regions)
            case 0x100b, 0x1007: // Tracks
                parseTracksBlock(data: data, block: block,
                                 sampleRate: sampleRate,
                                 tracks: &tracks)
            case 0x2000, 0x2001: // Compound block – recurse
                parseAllBlocks(data: data.subdata(in: block.contentOffset ..< (block.contentOffset + block.contentLength)),
                               version: version,
                               sampleRate: sampleRate,
                               audioFiles: &audioFiles,
                               regions: &regions,
                               tracks: &tracks,
                               tempoMap: &tempoMap,
                               markers: &markers)
            case 0x1028, 0x102b: // Tempo events
                parseTempoBlock(data: data, block: block,
                                sampleRate: sampleRate,
                                tempoMap: &tempoMap)
            case 0x1030, 0x1033: // Markers / memory locations
                parseMarkersBlock(data: data, block: block,
                                  sampleRate: sampleRate,
                                  markers: &markers)
            default:
                break
            }

            // Advance past this block
            pos = block.contentOffset + block.contentLength
        }
    }

    private func readBlock(data: Data, at pos: Int) -> Block? {
        guard pos + 6 <= data.count else { return nil }
        let type = readUInt16BE(data, at: pos)
        let size = Int(readUInt32BE(data, at: pos + 2))
        guard size > 0, pos + 6 + size <= data.count else { return nil }
        // Sanity: block type must be in a known range
        guard (type >= 0x1000 && type <= 0x10ff) ||
              (type >= 0x2000 && type <= 0x20ff) ||
              (type >= 0x3000 && type <= 0x30ff) else { return nil }
        return Block(type: type, contentOffset: pos + 6, contentLength: size)
    }

    // -----------------------------------------------------------------------
    // MARK: – Audio files block  (type 0x1001)
    // -----------------------------------------------------------------------
    private func parseAudioFilesBlock(data: Data, block: Block,
                                      sampleRate: Double,
                                      audioFiles: inout [PTAudioFile]) {
        var pos = block.contentOffset
        let end = block.contentOffset + block.contentLength
        guard pos + 2 <= end else { return }

        let count = Int(readUInt16BE(data, at: pos))
        pos += 2
        guard count > 0 && count < 10000 else { return }

        for _ in 0 ..< count {
            guard pos + 2 <= end else { break }
            let index = Int(readUInt16BE(data, at: pos)); pos += 2

            guard let name = readCString(data: data, at: &pos, end: end) else { break }
            guard !name.isEmpty else { continue }

            // Length in samples follows (8 bytes BE int64)
            var length: Int64 = 0
            if pos + 8 <= end {
                length = readInt64BE(data, at: pos); pos += 8
            }
            // Skip any extra padding to align to next entry
            while pos < end && data[pos] == 0 { pos += 1 }

            audioFiles.append(PTAudioFile(index: index,
                                          filename: name,
                                          lengthSamples: length))
        }
    }

    // -----------------------------------------------------------------------
    // MARK: – Regions block  (type 0x1003)
    // -----------------------------------------------------------------------
    private func parseRegionsBlock(data: Data, block: Block,
                                   sampleRate: Double,
                                   regions: inout [PTRegion]) {
        var pos = block.contentOffset
        let end = block.contentOffset + block.contentLength
        guard pos + 2 <= end else { return }

        let count = Int(readUInt16BE(data, at: pos)); pos += 2
        guard count > 0 && count < 100000 else { return }

        for _ in 0 ..< count {
            guard pos + 2 <= end else { break }
            let index = Int(readUInt16BE(data, at: pos)); pos += 2
            guard let name = readCString(data: data, at: &pos, end: end) else { break }
            guard pos + 2 <= end else { break }
            let fileIndex = Int(readUInt16BE(data, at: pos)); pos += 2
            guard pos + 24 <= end else { break }
            let offsetInFile = readInt64BE(data, at: pos); pos += 8
            _ = readInt64BE(data, at: pos); pos += 8           // absolute position (unused here)
            let length = readInt64BE(data, at: pos); pos += 8

            guard length > 0 else { continue }
            regions.append(PTRegion(index: index,
                                    name: name,
                                    fileIndex: fileIndex,
                                    offsetInFileSamples: offsetInFile,
                                    lengthSamples: length))
        }
    }

    // -----------------------------------------------------------------------
    // MARK: – Tracks block  (type 0x100b / 0x1007)
    // -----------------------------------------------------------------------
    private func parseTracksBlock(data: Data, block: Block,
                                  sampleRate: Double,
                                  tracks: inout [PTTrack]) {
        var pos = block.contentOffset
        let end = block.contentOffset + block.contentLength
        guard pos + 2 <= end else { return }

        let count = Int(readUInt16BE(data, at: pos)); pos += 2
        guard count > 0 && count < 10000 else { return }

        for _ in 0 ..< count {
            guard pos + 2 <= end else { break }
            let index = Int(readUInt16BE(data, at: pos)); pos += 2
            guard let name = readCString(data: data, at: &pos, end: end) else { break }
            guard pos + 1 <= end else { break }
            let stereoFlag = data[pos]; pos += 1
            guard pos + 2 <= end else { break }
            let numPlacements = Int(readUInt16BE(data, at: pos)); pos += 2
            guard numPlacements < 100000 else { continue }

            var placements = [PTClipPlacement]()
            for _ in 0 ..< numPlacements {
                guard pos + 18 <= end else { break }
                let regionIndex = Int(readUInt16BE(data, at: pos)); pos += 2
                let timelinePos = readInt64BE(data, at: pos); pos += 8
                let clipLen     = readInt64BE(data, at: pos); pos += 8
                placements.append(PTClipPlacement(
                    regionIndex: regionIndex,
                    startInTimelineSamples: timelinePos,
                    lengthSamples: clipLen))
            }

            tracks.append(PTTrack(index: index,
                                  name: name.isEmpty ? "Track \(index + 1)" : name,
                                  placements: placements,
                                  isStereo: stereoFlag == 1))
        }
    }

    // -----------------------------------------------------------------------
    // MARK: – Tempo block  (type 0x1028 / 0x102b)
    // -----------------------------------------------------------------------
    private func parseTempoBlock(data: Data, block: Block,
                                 sampleRate: Double,
                                 tempoMap: inout [PTTempoPoint]) {
        var pos = block.contentOffset
        let end = block.contentOffset + block.contentLength
        guard pos + 2 <= end else { return }
        let count = Int(readUInt16BE(data, at: pos)); pos += 2
        guard count < 100000 else { return }
        for _ in 0 ..< count {
            guard pos + 16 <= end else { break }
            let posSamples = readInt64BE(data, at: pos); pos += 8
            let bpmRaw     = readInt64BE(data, at: pos); pos += 8
            // BPM is stored as fixed-point (×1000 typically)
            let bpm = Double(bpmRaw) / 1000.0
            guard bpm > 0 && bpm < 1000 else { continue }
            tempoMap.append(PTTempoPoint(positionSamples: posSamples, bpm: bpm))
        }
    }

    // -----------------------------------------------------------------------
    // MARK: – Markers block  (type 0x1030 / 0x1033)
    // -----------------------------------------------------------------------
    private func parseMarkersBlock(data: Data, block: Block,
                                   sampleRate: Double,
                                   markers: inout [PTMarker]) {
        var pos = block.contentOffset
        let end = block.contentOffset + block.contentLength
        guard pos + 2 <= end else { return }
        let count = Int(readUInt16BE(data, at: pos)); pos += 2
        guard count < 10000 else { return }
        for _ in 0 ..< count {
            guard pos + 10 <= end else { break }
            _ = readUInt16BE(data, at: pos); pos += 2          // marker index
            let posSamples = readInt64BE(data, at: pos); pos += 8
            guard let name = readCString(data: data, at: &pos, end: end) else { break }
            markers.append(PTMarker(name: name.isEmpty ? "Marker" : name,
                                    positionSamples: posSamples))
        }
    }

    // -----------------------------------------------------------------------
    // MARK: – Fallback: scan for audio file names in raw bytes
    // -----------------------------------------------------------------------
    private func scanForAudioFilenames(data: Data) -> [PTAudioFile] {
        var result = [PTAudioFile]()
        var seen   = Set<String>()
        let extensions = [".wav", ".aif", ".aiff", ".sd2", ".mp3", ".m4a"]
        let bytes = Array(data)
        var i = 0
        while i < bytes.count - 5 {
            // Look for a dot followed by a known extension
            if bytes[i] == 0x2e {
                let extStart = i
                var extEnd   = i + 1
                while extEnd < bytes.count && extEnd - extStart < 6 {
                    let c = bytes[extEnd]
                    if c >= 0x61 && c <= 0x7a { extEnd += 1 } else { break }
                }
                let extBytes = bytes[extStart ..< extEnd]
                let ext = String(bytes: extBytes, encoding: .utf8) ?? ""
                if extensions.contains(ext) {
                    // Walk back to find start of filename
                    var nameStart = extStart - 1
                    while nameStart > 0 && isPrintableASCII(bytes[nameStart]) {
                        nameStart -= 1
                    }
                    nameStart += 1
                    let nameBytes = bytes[nameStart ..< extEnd]
                    if let name = String(bytes: nameBytes, encoding: .utf8),
                       name.count > 1 && !seen.contains(name) {
                        seen.insert(name)
                        result.append(PTAudioFile(index: result.count, filename: name))
                    }
                }
            }
            i += 1
        }
        return result
    }

    private func isPrintableASCII(_ b: UInt8) -> Bool {
        return b >= 0x20 && b < 0x7f && b != 0x2f && b != 0x00
    }

    // -----------------------------------------------------------------------
    // MARK: – Primitive readers
    // -----------------------------------------------------------------------

    private func readUInt16BE(_ data: Data, at offset: Int) -> UInt16 {
        guard offset + 1 < data.count else { return 0 }
        return (UInt16(data[offset]) << 8) | UInt16(data[offset + 1])
    }

    private func readUInt32BE(_ data: Data, at offset: Int) -> UInt32 {
        guard offset + 3 < data.count else { return 0 }
        return (UInt32(data[offset])     << 24) |
               (UInt32(data[offset + 1]) << 16) |
               (UInt32(data[offset + 2]) <<  8) |
                UInt32(data[offset + 3])
    }

    private func readInt64BE(_ data: Data, at offset: Int) -> Int64 {
        guard offset + 7 < data.count else { return 0 }
        var v: Int64 = 0
        for i in 0 ..< 8 {
            v = (v << 8) | Int64(data[offset + i])
        }
        return v
    }

    private func readCString(data: Data, at pos: inout Int, end: Int) -> String? {
        var end2 = pos
        while end2 < end && data[end2] != 0 { end2 += 1 }
        guard end2 <= end else { return nil }
        let str = String(bytes: data[pos ..< end2], encoding: .utf8)
                  ?? String(bytes: data[pos ..< end2], encoding: .isoLatin1)
                  ?? ""
        pos = end2 + 1
        return str
    }
}
