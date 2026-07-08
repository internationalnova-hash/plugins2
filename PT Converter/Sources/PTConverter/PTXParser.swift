import Foundation
import Compression

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

    // Set to true to write a hex analysis file next to the .ptx for debugging
    var writeDebugDump = false

    func parse(url: URL) throws -> PTSession {
        let raw = try Data(contentsOf: url, options: .mappedIfSafe)
        guard raw.count > 0x40 else { throw PTXError.tooSmall }

        // Basic magic check – first byte should be 0x03 for all modern PT sessions
        guard raw[0] == 0x03 else { throw PTXError.badMagic }

        // Version byte at 0x14 — PT10=10, PT11=11, ..., PT2018+=varies (up to ~90+)
        // We no longer reject unknown versions; just attempt parsing.
        let version = Int(raw[0x14])

        // XOR key: try several known offsets for different PT generations.
        // PT10-12: key at 0x1a. PT2018-2021: key may be at 0x1d. PT2022+: often 0.
        // We try each candidate and pick the one that yields the most readable blocks.
        let keyOffset: Int
        if version <= 12 {
            keyOffset = 0x1a
        } else if version <= 24 {
            keyOffset = 0x1d
        } else {
            // Modern PT (2022+) — key offset shifted or no XOR applied
            keyOffset = 0x1a
        }
        let xorKey = raw.count > keyOffset ? raw[keyOffset] : 0

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

        // Try secondary XOR key 0x2A on the later portion of the file.
        // Modern PT uses a per-section secondary key for the clip/region block area.
        // We detect the best secondary key by brute-force and re-parse that section.
        if tracks.isEmpty || tracks.allSatisfy({ $0.placements.isEmpty }) {
            let secondaryKey = detectSecondaryXORKey(data: data, startOffset: 0x10000)
            if secondaryKey != 0 {
                var buf2 = Array(data)
                for idx in min(0x10000, buf2.count) ..< buf2.count {
                    buf2[idx] ^= secondaryKey
                }
                let data2 = Data(buf2)
                var r2 = [PTRegion](); var t2 = [PTTrack](); var f2 = audioFiles
                parseAllBlocks(data: data2, version: version, sampleRate: sampleRate,
                               audioFiles: &f2, regions: &r2,
                               tracks: &t2, tempoMap: &tempoMap, markers: &markers)
                if t2.contains(where: { !$0.placements.isEmpty }) {
                    tracks = t2
                    if r2.count > regions.count { regions = r2 }
                }
            }
        }

        // Fallback: if structured parse found nothing, scan entire file for audio filenames
        if audioFiles.isEmpty {
            audioFiles = scanForAudioFilenames(data: data)
        }

        // Deep scan for modern PT block structures (different type IDs)
        if regions.isEmpty && !audioFiles.isEmpty {
            deepScanBlocks(data: data, version: version, sampleRate: sampleRate,
                           audioFiles: &audioFiles, regions: &regions, tracks: &tracks)
        }

        // Try to find and decompress zlib blocks in both raw and XOR'd data.
        // Modern PT (2018+) stores clip/region data in compressed segments.
        // We scan both buffers because compression may be applied before or after XOR.
        if tracks.isEmpty || tracks.allSatisfy({ $0.placements.isEmpty }) {
            let rawData = Data(Array(raw))
            for searchData in [data, rawData] {
                if let decompressed = findAndDecompressZlib(in: searchData) {
                    // Parse blocks from decompressed data
                    var dRegions = [PTRegion]()
                    var dTracks  = [PTTrack]()
                    var dFiles   = audioFiles
                    parseAllBlocks(data: decompressed, version: version, sampleRate: sampleRate,
                                   audioFiles: &dFiles, regions: &dRegions,
                                   tracks: &dTracks, tempoMap: &tempoMap, markers: &markers)
                    if !dTracks.isEmpty && dTracks.contains(where: { !$0.placements.isEmpty }) {
                        tracks = dTracks
                        if dRegions.count > regions.count { regions = dRegions }
                        break
                    }
                    // Also try clip position extraction from decompressed data
                    if tracks.isEmpty && !audioFiles.isEmpty {
                        let names = scanForTrackNames(data: decompressed)
                        if !names.isEmpty {
                            let t = buildTracksFromNamesWithPositions(
                                trackNames: names, audioFiles: audioFiles,
                                data: decompressed, sampleRate: sampleRate)
                            if t.contains(where: { !$0.placements.isEmpty }) {
                                tracks = t; break
                            }
                        }
                    }
                }
            }
        }

        // If structured block parse found no tracks, use the region-entry scanner.
        // Modern PT stores regions as: name + null + [2b fileIdx][8b f1][8b f2][8b f3]
        // where f3 = clip length in samples at session rate.
        // Timeline positions are stored in PT ticks (not samples) and require a tempo
        // map to convert; for tuning sessions all clips start at position 0 in practice.
        if tracks.isEmpty {
            let regionEntries = scanRegionEntries(data: data, sampleRate: sampleRate)
            if !regionEntries.isEmpty {
                tracks = buildTracksFromRegionEntries(
                    entries: regionEntries, audioFiles: audioFiles)
            }
        }

        if tracks.isEmpty {
            let trackNames = scanForTrackNames(data: data)
            if !trackNames.isEmpty && !audioFiles.isEmpty {
                tracks = buildTracksFromNames(trackNames: trackNames, audioFiles: audioFiles)
            }
            if tracks.isEmpty && !audioFiles.isEmpty {
                tracks = inferTracksFromFileNames(audioFiles: audioFiles)
            }
        }

        let sessionName = url.deletingPathExtension().lastPathComponent

        // Write debug dump to Desktop for analysis
        if writeDebugDump {
            writeBinaryAnalysis(data: data, url: url, sessionName: sessionName,
                                sampleRate: sampleRate, audioFiles: audioFiles,
                                regions: regions, tracks: tracks)
        }

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
        guard size > 0, size < 10_000_000, pos + 6 + size <= data.count else { return nil }
        // Accept a wide range of block type IDs to handle all PT versions
        guard (type >= 0x0100 && type <= 0x0fff) ||
              (type >= 0x1000 && type <= 0x5fff) ||
              (type >= 0x2000 && type <= 0x9fff) else { return nil }
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
    // MARK: – Binary structure analysis / debug dump
    // -----------------------------------------------------------------------
    private func writeBinaryAnalysis(data: Data, url: URL, sessionName: String,
                                     sampleRate: Double, audioFiles: [PTAudioFile],
                                     regions: [PTRegion], tracks: [PTTrack]) {
        let bytes = Array(data)
        var lines = [String]()
        lines.append("PT Binary Analysis: \(sessionName)")
        lines.append("File size: \(data.count) bytes  SampleRate: \(sampleRate)")
        lines.append("Version byte 0x14: 0x\(String(bytes[0x14], radix: 16))")
        lines.append("Parsed: \(audioFiles.count) audioFiles, \(regions.count) regions, \(tracks.count) tracks")
        lines.append("")

        // Dump header bytes
        lines.append("=== HEADER (0x00-0x60) ===")
        for row in stride(from: 0, through: min(0x60, bytes.count-1), by: 16) {
            let end = min(row + 16, bytes.count)
            let hex = bytes[row..<end].map { String(format: "%02x", $0) }.joined(separator: " ")
            let ascii = bytes[row..<end].map { ($0 >= 0x20 && $0 < 0x7f) ? Character(UnicodeScalar($0)) : "." }.map(String.init).joined()
            lines.append(String(format: "%08x  %-47s  %@", row, hex, ascii))
        }
        lines.append("")

        // Find all printable string regions (runs of printable ASCII >= 3 chars)
        lines.append("=== PRINTABLE STRING REGIONS ===")
        var strStart = -1
        var i = 0
        while i < bytes.count {
            let printable = bytes[i] >= 0x20 && bytes[i] < 0x7f
            if printable && strStart < 0 { strStart = i }
            if !printable && strStart >= 0 {
                let len = i - strStart
                if len >= 3 {
                    let s = String(bytes: bytes[strStart..<i], encoding: .utf8) ?? "?"
                    lines.append(String(format: "  0x%06x len=%d: %@", strStart, len, s))
                }
                strStart = -1
            }
            i += 1
        }
        lines.append("")

        // Scan for (string + null + 26 binary bytes) pattern — these are PT region entries
        // containing [fileIndex:u16][field1:i64][field2:i64][field3:i64]
        lines.append("=== REGION ENTRY SCAN (string + 26-byte binary pattern) ===")
        var ri = 0x40
        while ri < min(bytes.count - 30, 0x30000) {
            guard bytes[ri] >= 0x20 && bytes[ri] < 0x7f else { ri += 1; continue }
            var re = ri
            while re < bytes.count && bytes[re] >= 0x20 && bytes[re] < 0x7f { re += 1 }
            let rNameLen = re - ri
            guard rNameLen >= 3 && rNameLen <= 64 && re < bytes.count && bytes[re] == 0x00 else { ri = re + 1; continue }
            let afterNull = re + 1
            guard afterNull + 26 <= bytes.count else { ri = afterNull; continue }
            // Check that after 26 bytes the next byte is printable or null (= another entry)
            let nextByte = bytes[afterNull + 26]
            guard (nextByte >= 0x20 && nextByte < 0x7f) || nextByte == 0x00 else { ri = afterNull; continue }
            // Read fields
            let fileIdx = (Int(bytes[afterNull]) << 8) | Int(bytes[afterNull+1])
            var f1: Int64 = 0, f2: Int64 = 0, f3: Int64 = 0
            for b in 0..<8 { f1 = (f1 << 8) | Int64(bytes[afterNull+2+b]) }
            for b in 0..<8 { f2 = (f2 << 8) | Int64(bytes[afterNull+10+b]) }
            for b in 0..<8 { f3 = (f3 << 8) | Int64(bytes[afterNull+18+b]) }
            // Also try little-endian
            var lf1: Int64=0, lf2: Int64=0, lf3: Int64=0
            for b in 0..<8 { lf1 |= Int64(bytes[afterNull+2+b]) << (b*8) }
            for b in 0..<8 { lf2 |= Int64(bytes[afterNull+10+b]) << (b*8) }
            for b in 0..<8 { lf3 |= Int64(bytes[afterNull+18+b]) << (b*8) }
            let maxS = Int64(sampleRate * 7200)
            let name = String(bytes: bytes[ri..<re], encoding: .utf8) ?? "?"
            let f1s = f1 > 0 && f1 < maxS ? String(format:"%.2fs", Double(f1)/sampleRate) : "-"
            let f2s = f2 > 0 && f2 < maxS ? String(format:"%.2fs", Double(f2)/sampleRate) : "-"
            let f3s = f3 > 0 && f3 < maxS ? String(format:"%.2fs", Double(f3)/sampleRate) : "-"
            lines.append(String(format: "  0x%06x \"%@\" fileIdx=%d BE[%@,%@,%@] LE[%@,%@,%@]",
                ri, name, fileIdx, String(f1), String(f2), String(f3),
                String(lf1),String(lf2),String(lf3)))
            lines.append(String(format: "    BE_sec[%@,%@,%@]", f1s, f2s, f3s))
            ri = afterNull + 26
            continue
            ri += 1
        }
        lines.append("")

        // Dump the region between last track name and compressed data
        // Find approximate end of track name section (look for long runs of 0x91/0x92)
        var compressStart = bytes.count
        var runLen = 0
        for j in stride(from: bytes.count - 1, through: 0, by: -1) {
            if bytes[j] == 0x91 || bytes[j] == 0x92 || bytes[j] == 0x93 {
                runLen += 1
            } else {
                if runLen > 100 { compressStart = j + 1; break }
                runLen = 0
            }
        }

        // Dump 4KB before compressed section — this is where clip positions live
        let dumpStart = max(0, compressStart - 4096)
        lines.append(String(format: "=== DATA BEFORE COMPRESSED SECTION (0x%06x - 0x%06x) ===", dumpStart, compressStart))
        for row in stride(from: dumpStart, to: compressStart, by: 16) {
            let end2 = min(row + 16, compressStart)
            let hex2 = bytes[row..<end2].map { String(format: "%02x", $0) }.joined(separator: " ")
            let ascii2 = bytes[row..<end2].map { ($0 >= 0x20 && $0 < 0x7f) ? Character(UnicodeScalar($0)) : "." }.map(String.init).joined()
            // Also decode as potential int64 BE and LE
            var be64: Int64 = 0
            var le64: Int64 = 0
            if row + 8 <= end2 {
                for b in 0..<8 { be64 = (be64 << 8) | Int64(bytes[row+b]) }
                for b in 0..<8 { le64 |= Int64(bytes[row+b]) << (b*8) }
            }
            let maxSamp = Int64(sampleRate * 7200)
            var note = ""
            if be64 > 0 && be64 < maxSamp { note += " BE64=\(be64)samp(\(String(format:"%.2f", Double(be64)/sampleRate))s)" }
            if le64 > 0 && le64 < maxSamp && le64 != be64 { note += " LE64=\(le64)samp(\(String(format:"%.2f", Double(le64)/sampleRate))s)" }
            lines.append(String(format: "%08x  %-47s  %-16s%@", row, hex2, ascii2, note))
        }

        // Brute-force XOR key detection on the garbled section
        // The garbled section (strings like yO^^CDMY, g_YCI) uses a second XOR key.
        // We try all 256 keys and pick the one that yields the most readable ASCII.
        lines.append("")
        lines.append("=== XOR KEY BRUTE-FORCE (garbled section 0x14000-0x18000) ===")
        let scanStart = min(0x14000, bytes.count - 2)
        let scanEnd   = min(0x18000, bytes.count)
        if scanEnd > scanStart + 64 {
            var bestKey = 0; var bestScore = 0
            for key in 0..<256 {
                var score = 0
                var runLen = 0
                for j in scanStart..<scanEnd {
                    let b = bytes[j] ^ UInt8(key)
                    if b >= 0x20 && b < 0x7f { runLen += 1; if runLen >= 4 { score += 1 } }
                    else { runLen = 0 }
                }
                if score > bestScore { bestScore = score; bestKey = key }
            }
            lines.append(String(format: "  Best XOR key: 0x%02x (score %d printable runs)", bestKey, bestScore))

            // Dump printable strings found with that key
            var strS = -1
            for j in scanStart..<scanEnd {
                let b = bytes[j] ^ UInt8(bestKey)
                let printable = b >= 0x20 && b < 0x7f
                if printable && strS < 0 { strS = j }
                if !printable && strS >= 0 {
                    let len = j - strS
                    if len >= 4 {
                        let decoded = bytes[strS..<j].map { Character(UnicodeScalar($0 ^ UInt8(bestKey))) }
                        lines.append(String(format: "  0x%06x len=%d: %@", strS, len, String(decoded)))
                    }
                    strS = -1
                }
            }
        }

        // Also try key 0x2A specifically (suspected secondary key) and dump strings
        lines.append("")
        lines.append("=== XOR KEY 0x2A DECODE (0x14000-0x18000) ===")
        if scanEnd > scanStart + 64 {
            var strS2 = -1
            for j in scanStart..<scanEnd {
                let b = bytes[j] ^ 0x2A
                let printable = b >= 0x20 && b < 0x7f
                if printable && strS2 < 0 { strS2 = j }
                if !printable && strS2 >= 0 {
                    let len = j - strS2
                    if len >= 4 {
                        let decoded = bytes[strS2..<j].map { Character(UnicodeScalar($0 ^ 0x2A)) }
                        lines.append(String(format: "  0x%06x len=%d: %@", strS2, len, String(decoded)))
                    }
                    strS2 = -1
                }
            }
        }

        let desktop = FileManager.default.urls(for: .desktopDirectory, in: .userDomainMask).first!
        let dumpURL = desktop.appendingPathComponent("ptx_analysis_\(sessionName).txt")
        try? lines.joined(separator: "\n").write(to: dumpURL, atomically: true, encoding: .utf8)
    }

    // -----------------------------------------------------------------------
    // MARK: – Zlib decompression
    // -----------------------------------------------------------------------
    // Scans data for zlib-compressed blocks (magic: 0x78 0x9C / 0x78 0xDA / 0x78 0x01 / 0x78 0x5E)
    // Returns the largest successfully decompressed chunk, or nil.
    private func findAndDecompressZlib(in data: Data) -> Data? {
        let bytes = Array(data)
        let zlibSecondBytes: Set<UInt8> = [0x01, 0x5E, 0x9C, 0xDA]
        var bestResult: Data? = nil
        var bestSize = 0

        var i = 0
        while i < bytes.count - 6 {
            guard bytes[i] == 0x78 && zlibSecondBytes.contains(bytes[i+1]) else { i += 1; continue }

            // Try to decompress from here
            if let decompressed = zlibDecompress(data: data, from: i) {
                if decompressed.count > bestSize {
                    bestSize = decompressed.count
                    bestResult = decompressed
                }
            }
            i += 2
        }
        return bestResult
    }

    private func zlibDecompress(data: Data, from offset: Int) -> Data? {
        let inputSize = data.count - offset
        guard inputSize > 4 else { return nil }

        // Allocate output buffer — zlib data can expand up to ~10x, cap at 200MB
        let maxOut = min(inputSize * 20, 200_000_000)
        let outBuf = UnsafeMutablePointer<UInt8>.allocate(capacity: maxOut)
        defer { outBuf.deallocate() }

        let written: Int = data.withUnsafeBytes { ptr -> Int in
            guard let base = ptr.baseAddress else { return 0 }
            let inPtr = base.advanced(by: offset).assumingMemoryBound(to: UInt8.self)
            return compression_decode_buffer(outBuf, maxOut, inPtr, inputSize, nil, COMPRESSION_ZLIB)
        }

        guard written > 100 else { return nil }
        return Data(bytes: outBuf, count: written)
    }

    // -----------------------------------------------------------------------
    // MARK: – Track building with position scanning
    // -----------------------------------------------------------------------
    // Builds tracks from names and tries to find sample positions in the binary
    // near each track name entry, or via sequence of int64 position tables.
    private func buildTracksFromNamesWithPositions(trackNames: [String], audioFiles: [PTAudioFile],
                                                    data: Data, sampleRate: Double) -> [PTTrack] {
        // First try: scan data for int64 tables that look like clip start positions
        // followed by lengths. PT stores: [startSample: int64][lengthSample: int64][fileOffset: int64]
        let bytes = Array(data)
        let maxSamp = Int64(sampleRate * 7200)   // 2 hours
        let minSamp: Int64 = 0

        // Collect all plausible (start, length) pairs from 8-byte aligned positions
        struct PosEntry { let offset: Int; let start: Int64; let length: Int64 }
        var candidates = [PosEntry]()
        var j = 0
        while j < bytes.count - 16 {
            // Try big-endian int64 pair
            var start: Int64 = 0
            var length: Int64 = 0
            for b in 0..<8 { start  = (start  << 8) | Int64(bytes[j+b]) }
            for b in 0..<8 { length = (length << 8) | Int64(bytes[j+8+b]) }
            if start >= minSamp && start <= maxSamp && length > 0 && length <= maxSamp {
                candidates.append(PosEntry(offset: j, start: start, length: length))
            }
            // Also try little-endian
            var leStart: Int64 = 0; var leLen: Int64 = 0
            for b in 0..<8 { leStart |= Int64(bytes[j+b]) << (b*8) }
            for b in 0..<8 { leLen   |= Int64(bytes[j+8+b]) << (b*8) }
            if leStart > 0 && leStart <= maxSamp && leLen > 0 && leLen <= maxSamp {
                candidates.append(PosEntry(offset: j, start: leStart, length: leLen))
            }
            j += 8
        }

        // Group candidates into clusters (consecutive positions = clip list)
        // Find the largest cluster — that's likely the track placement table
        var tracks = [PTTrack]()
        if !candidates.isEmpty && !trackNames.isEmpty && !audioFiles.isEmpty {
            // Sort by offset and group into runs
            let sorted = candidates.sorted { $0.offset < $1.offset }
            var runs = [[PosEntry]](); var cur = [PosEntry]()
            for (idx, c) in sorted.enumerated() {
                if idx == 0 { cur.append(c); continue }
                if c.offset - sorted[idx-1].offset <= 32 { cur.append(c) }
                else { if cur.count >= 2 { runs.append(cur) }; cur = [c] }
            }
            if cur.count >= 2 { runs.append(cur) }

            // Largest run = primary clip table
            if let best = runs.max(by: { $0.count < $1.count }) {
                // Assign positions round-robin to tracks
                let perTrack = max(1, best.count / max(1, trackNames.count))
                for (ti, name) in trackNames.enumerated() {
                    let slice = Array(best[ti*perTrack ..< min((ti+1)*perTrack, best.count)])
                    let afIdx = ti < audioFiles.count ? audioFiles[ti].index : 0
                    let regionIdx = ti
                    let placements: [PTClipPlacement] = slice.map {
                        PTClipPlacement(regionIndex: regionIdx,
                                        startInTimelineSamples: $0.start,
                                        lengthSamples: $0.length)
                    }
                    tracks.append(PTTrack(index: ti, name: name,
                                          placements: placements.isEmpty
                                            ? [PTClipPlacement(regionIndex: afIdx,
                                                               startInTimelineSamples: Int64(ti) * (audioFiles.first?.lengthSamples ?? 44100),
                                                               lengthSamples: ti < audioFiles.count ? audioFiles[ti].lengthSamples : 44100)]
                                            : placements,
                                          isStereo: false))
                }
                return tracks
            }
        }

        // Fallback: sequential placement
        return buildTracksFromNames(trackNames: trackNames, audioFiles: audioFiles)
    }

    // -----------------------------------------------------------------------
    // MARK: – Region entry scanner (modern PT 2018+)
    // -----------------------------------------------------------------------
    // PT stores regions as: [name]\0 [fileIdx:u16] [f1:i64BE] [f2:i64BE] [f3:i64BE]
    // f3 = clip length in samples at session sample rate.
    // f1/f2 are tick-based positions (not directly usable without tempo map).
    struct RegionEntry {
        let name: String
        let fileIndex: Int
        let lengthSamples: Int64   // f3 — confirmed from binary analysis
    }

    private func scanRegionEntries(data: Data, sampleRate: Double) -> [RegionEntry] {
        let bytes = Array(data)
        let maxS = Int64(sampleRate * 7200)
        var result = [RegionEntry]()
        var seen = Set<String>()
        var i = 0x40
        while i < min(bytes.count - 30, 0x30000) {
            guard bytes[i] >= 0x20 && bytes[i] < 0x7f else { i += 1; continue }
            var end = i
            while end < bytes.count && bytes[end] >= 0x20 && bytes[end] < 0x7f { end += 1 }
            let nameLen = end - i
            guard nameLen >= 3 && nameLen <= 64,
                  end < bytes.count && bytes[end] == 0x00 else { i = end + 1; continue }
            let afterNull = end + 1
            guard afterNull + 26 <= bytes.count else { i = afterNull; continue }
            let nextByte = bytes[afterNull + 26]
            guard (nextByte >= 0x20 && nextByte < 0x7f) || nextByte == 0x00 else { i = afterNull; continue }
            // Read f3 (bytes 18-25 after null) as big-endian int64
            var f3: Int64 = 0
            for b in 0..<8 { f3 = (f3 << 8) | Int64(bytes[afterNull + 18 + b]) }
            let fileIdx = (Int(bytes[afterNull]) << 8) | Int(bytes[afterNull + 1])
            guard let name = String(bytes: Array(bytes[i..<end]), encoding: .utf8),
                  !seen.contains(name) else { i = afterNull + 26; continue }
            // Only accept if f3 is a plausible sample length (> 0 and < 2 hours)
            if f3 > 0 && f3 <= maxS {
                seen.insert(name)
                result.append(RegionEntry(name: name, fileIndex: fileIdx, lengthSamples: f3))
            }
            i = afterNull + 26
        }
        return result
    }

    private func buildTracksFromRegionEntries(entries: [RegionEntry],
                                               audioFiles: [PTAudioFile]) -> [PTTrack] {
        guard !entries.isEmpty else { return [] }
        // Match each region entry to the best-matching audio file by name
        var tracks = [PTTrack]()
        for (idx, entry) in entries.enumerated() {
            let afIdx = bestMatchingFileIndex(name: entry.name, audioFiles: audioFiles) ?? idx % max(1, audioFiles.count)
            let placement = PTClipPlacement(
                regionIndex: idx,
                startInTimelineSamples: 0,   // position 0: all clips start at bar 1 (standard for tuning sessions)
                lengthSamples: entry.lengthSamples)
            tracks.append(PTTrack(index: idx, name: entry.name,
                                  placements: [placement], isStereo: false))
        }
        return tracks
    }

    private func bestMatchingFileIndex(name: String, audioFiles: [PTAudioFile]) -> Int? {
        guard !audioFiles.isEmpty else { return nil }
        let lower = name.lowercased()
        // Exact prefix match
        if let idx = audioFiles.firstIndex(where: { lower.hasPrefix($0.filename.lowercased().replacingOccurrences(of: ".wav", with: "").replacingOccurrences(of: ".aif", with: "")) }) {
            return audioFiles[idx].index
        }
        // Fuzzy: count common words
        let nameWords = Set(lower.components(separatedBy: CharacterSet.alphanumerics.inverted).filter { !$0.isEmpty })
        var best = -1; var bestScore = 0
        for af in audioFiles {
            let afWords = Set(af.filename.lowercased().components(separatedBy: CharacterSet.alphanumerics.inverted).filter { !$0.isEmpty })
            let score = nameWords.intersection(afWords).count
            if score > bestScore { bestScore = score; best = af.index }
        }
        return bestScore > 0 ? best : nil
    }

    // -----------------------------------------------------------------------
    // MARK: – Secondary XOR key detection
    // -----------------------------------------------------------------------
    // Scans a region of the file trying all 256 XOR keys; returns the key
    // that produces the most runs of 4+ printable ASCII characters.
    private func detectSecondaryXORKey(data: Data, startOffset: Int) -> UInt8 {
        let bytes = Array(data)
        let start = min(startOffset, bytes.count - 2)
        let end   = min(start + 16384, bytes.count)  // scan 16KB sample
        guard end > start + 64 else { return 0 }

        var bestKey: UInt8 = 0; var bestScore = 0
        for key in 1..<256 {   // skip 0 = no-op
            var score = 0; var runLen = 0
            for j in start..<end {
                let b = bytes[j] ^ UInt8(key)
                if b >= 0x20 && b < 0x7f { runLen += 1; if runLen >= 4 { score += 1 } }
                else { runLen = 0 }
            }
            if score > bestScore { bestScore = score; bestKey = UInt8(key) }
        }
        // Only return a key if it significantly outperforms no-key
        var baseScore = 0; var baseRun = 0
        for j in start..<end {
            let b = bytes[j]
            if b >= 0x20 && b < 0x7f { baseRun += 1; if baseRun >= 4 { baseScore += 1 } }
            else { baseRun = 0 }
        }
        return bestScore > baseScore * 2 ? bestKey : 0
    }

    // -----------------------------------------------------------------------
    // MARK: – Deep block scan for modern PT (tries many block type ranges)
    // -----------------------------------------------------------------------
    // Modern PT 2022-2025 shifted block type IDs. Scan the entire file for
    // block-like structures and attempt to find regions/tracks data.
    private func deepScanBlocks(data: Data, version: Int, sampleRate: Double,
                                 audioFiles: inout [PTAudioFile],
                                 regions: inout [PTRegion],
                                 tracks: inout [PTTrack]) {
        let bytes = Array(data)
        guard bytes.count > 0x100 else { return }

        // Heuristic: look for uint16 count followed by entries that look like
        // region records (index, c-string name, fileIndex, offset, absPos, length)
        // We scan for count values 1-500 at aligned positions, then validate entries.
        var pos = 0x80
        while pos < min(bytes.count - 30, 0x100000) {
            let count = Int(bytes[pos]) | (Int(bytes[pos+1]) << 8)
            if count >= 1 && count <= 500 {
                // Try little-endian count (modern PT may use LE)
                if let foundRegions = tryParseRegionsLE(data: data, at: pos, count: count, sampleRate: sampleRate) {
                    regions.append(contentsOf: foundRegions)
                }
            }
            pos += 1
        }
    }

    private func tryParseRegionsLE(data: Data, at pos: Int, count: Int, sampleRate: Double) -> [PTRegion]? {
        var cursor = pos + 2
        var result = [PTRegion]()
        for _ in 0 ..< count {
            guard cursor + 4 < data.count else { return nil }
            let index = Int(data[cursor]) | (Int(data[cursor+1]) << 8)
            cursor += 2
            guard index < 5000 else { return nil }
            // Read c-string name
            var nameEnd = cursor
            while nameEnd < data.count && nameEnd - cursor < 128 && data[nameEnd] != 0 { nameEnd += 1 }
            guard nameEnd < data.count && nameEnd > cursor else { return nil }
            let nameBytes = data[cursor ..< nameEnd]
            guard nameBytes.allSatisfy({ $0 >= 0x20 && $0 < 0x7f }) else { return nil }
            guard let name = String(bytes: nameBytes, encoding: .utf8) else { return nil }
            cursor = nameEnd + 1
            guard cursor + 26 <= data.count else { return nil }
            let fileIndex = Int(data[cursor]) | (Int(data[cursor+1]) << 8)
            cursor += 2
            // Read int64 LE: offsetInFile
            var offsetInFile: Int64 = 0
            for b in 0..<8 { offsetInFile |= Int64(data[cursor+b]) << (b*8) }
            cursor += 8
            // Skip abs pos
            cursor += 8
            // Length
            var length: Int64 = 0
            for b in 0..<8 { length |= Int64(data[cursor+b]) << (b*8) }
            cursor += 8
            guard length > 0 && length < 1_000_000_000_000 else { return nil }
            guard fileIndex < 5000 else { return nil }
            result.append(PTRegion(index: index, name: name, fileIndex: fileIndex,
                                   offsetInFileSamples: offsetInFile, lengthSamples: length))
        }
        return result.isEmpty ? nil : result
    }

    // -----------------------------------------------------------------------
    // MARK: – Scan for sample-position tables (clip timeline positions)
    // -----------------------------------------------------------------------
    // Looks for sequences of plausible int64 sample positions (44100–192000 * seconds)
    // bracketed by region name patterns. Returns (offset, position) pairs.
    func scanForSamplePositions(data: Data, sampleRate: Double,
                                 maxDurationSeconds: Double = 7200) -> [(offset: Int, samples: Int64)] {
        let bytes = Array(data)
        let maxSamples = Int64(sampleRate * maxDurationSeconds)
        var results = [(offset: Int, samples: Int64)]()
        var i = 0
        while i < bytes.count - 8 {
            // Read as both BE and LE int64
            var be: Int64 = 0
            var le: Int64 = 0
            for b in 0..<8 {
                be = (be << 8) | Int64(bytes[i + b])
                le |= Int64(bytes[i + b]) << (b * 8)
            }
            // Accept plausible positive sample positions (0 to 2 hours)
            for val in [be, le] {
                if val > 0 && val <= maxSamples {
                    results.append((offset: i, samples: val))
                }
            }
            i += 8
        }
        return results
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

    // -----------------------------------------------------------------------
    // MARK: – Track name scanner for modern PT (version 90 / PT 2024-2025)
    //
    // Scan for the "e5 [byte]" marker that precedes each track entry, then
    // try multiple header offsets (entries have slightly varying layouts).
    // After the header, read a uint16-LE length + name string.
    // Strip PT internal suffixes (-cm, -cm.2, .dupl, .dup1-cm.2, etc.).
    // -----------------------------------------------------------------------
    private func scanForTrackNames(data: Data) -> [String] {
        var names  = [String]()
        var seen   = Set<String>()
        let bytes  = Array(data)

        // PT internal suffixes appended to track names in the binary
        let ptSuffixes = ["-cm.2", "-cm.3", "-cm.4", "-cm", ".dup1-cm.2", ".dup2-cm.2",
                          ".dup1-cm", ".dup2-cm", ".dupl-cm.2", ".dupl-cm", ".dupl",
                          ".dup1", ".dup2", "-1", "-2"]

        func cleanName(_ raw: String) -> String {
            var s = raw
            for suffix in ptSuffixes {
                if s.hasSuffix(suffix) {
                    s = String(s.dropLast(suffix.count))
                    break
                }
            }
            return s.trimmingCharacters(in: .whitespaces)
        }

        func tryReadName(at nameStart: Int, lengthAt lenPos: Int) -> String? {
            guard lenPos + 2 <= bytes.count else { return nil }
            let nameLen = Int(bytes[lenPos]) | (Int(bytes[lenPos + 1]) << 8)
            guard nameLen >= 1 && nameLen <= 64 else { return nil }
            guard nameStart + nameLen <= bytes.count else { return nil }
            let nb = bytes[nameStart ..< nameStart + nameLen]
            guard nb.allSatisfy({ $0 >= 0x20 && $0 < 0x7f }) else { return nil }
            return String(bytes: nb, encoding: .utf8)
        }

        var i = 0
        while i < bytes.count - 25 {
            // Look for the e5 marker byte — consistent across all track entries
            guard bytes[i] == 0xe5 else { i += 1; continue }

            // The track index follows e5, then 4+ zeros, then uint16 length, then name.
            // Header sizes observed: 6, 7, 8 bytes after e5 to the length field.
            for headerExtra in [4, 5, 6, 7, 8] {
                let lenPos   = i + 1 + headerExtra   // skip e5(1) + trackIdx(1) + varying
                let nameStart = lenPos + 2
                guard nameStart < bytes.count else { continue }

                // The bytes between trackIdx and lenPos should be zero or near-zero
                var zeroCount = 0
                for z in (i+2) ..< lenPos { if bytes[z] == 0 { zeroCount += 1 } }
                guard zeroCount >= headerExtra - 2 else { continue }

                if let raw = tryReadName(at: nameStart, lengthAt: lenPos) {
                    let clean = cleanName(raw)
                    guard clean.count >= 2,
                          !clean.hasPrefix("Info #"),
                          !clean.hasPrefix("Audio ") || clean.count > 8 // skip generic Audio N
                    else { continue }

                    if !seen.contains(clean) {
                        seen.insert(clean)
                        names.append(clean)
                    }
                }
            }
            i += 1
        }

        // Also pick up generic "Audio N" tracks which are fine
        var i2 = 0
        while i2 < bytes.count - 15 {
            guard bytes[i2] == 0xe5 else { i2 += 1; continue }
            for headerExtra in [4, 5, 6, 7, 8] {
                let lenPos    = i2 + 1 + headerExtra
                let nameStart = lenPos + 2
                guard nameStart < bytes.count else { continue }
                if let raw = tryReadName(at: nameStart, lengthAt: lenPos),
                   raw.hasPrefix("Audio "), raw.count <= 10 {
                    let clean = cleanName(raw)
                    if !seen.contains(clean) { seen.insert(clean); names.append(clean) }
                }
            }
            i2 += 1
        }

        return names
    }

    // Build tracks by matching scanned names against audio file names.
    // Audio files whose name contains the track name (or vice versa) get
    // assigned to that track. Unmatched files go to a catch-all track.
    private func buildTracksFromNames(trackNames: [String],
                                      audioFiles: [PTAudioFile]) -> [PTTrack] {
        var assigned = Set<Int>()
        var result   = [PTTrack]()

        for (i, trackName) in trackNames.enumerated() {
            let lowerTrack = trackName.lowercased()
            // Skip obvious non-audio tracks
            if lowerTrack.contains("master") || lowerTrack.contains("bus") ||
               lowerTrack.contains("click") || lowerTrack.contains("midi") {
                continue
            }

            // Sort matched files by take number so they appear in order
            let matched = audioFiles.filter { af in
                let lowerFile = af.filename.lowercased()
                let stem = (af.filename as NSString).deletingPathExtension.lowercased()
                return lowerFile.contains(lowerTrack) || lowerTrack.contains(stem) ||
                       stem.contains(lowerTrack)
            }.sorted { a, b in
                // Sort by take/index suffix if present
                let numA = takeNumber(a.filename)
                let numB = takeNumber(b.filename)
                if numA != numB { return numA < numB }
                return a.filename < b.filename
            }

            let filesToUse = matched.isEmpty ? [] : matched
            for af in filesToUse { assigned.insert(af.index) }

            // Place each clip sequentially on the track (pos 0, each after previous)
            // This gives the correct relative order even when exact timeline pos is unknown.
            var cursor: Int64 = 0
            let placements = filesToUse.map { af -> PTClipPlacement in
                let p = PTClipPlacement(regionIndex: af.index,
                                        startInTimelineSamples: cursor,
                                        lengthSamples: af.lengthSamples > 0 ? af.lengthSamples : 0)
                // For multi-take tracks, advance cursor; for single-take, keep at 0
                if filesToUse.count > 1 { cursor += af.lengthSamples }
                return p
            }
            result.append(PTTrack(index: i, name: trackName,
                                  placements: placements, isStereo: true))
        }

        // Catch-all: unmatched audio files as individual tracks
        let unmatched = audioFiles.filter { !assigned.contains($0.index) }
        for af in unmatched {
            let name = (af.filename as NSString).deletingPathExtension
            result.append(PTTrack(index: result.count, name: name,
                                  placements: [PTClipPlacement(regionIndex: af.index,
                                                               startInTimelineSamples: 0,
                                                               lengthSamples: af.lengthSamples)],
                                  isStereo: true))
        }
        return result
    }

    private func takeNumber(_ filename: String) -> Int {
        // Extract trailing number from filename stem (e.g. "Kick_03.wav" → 3)
        let stem = (filename as NSString).deletingPathExtension
        if let m = stem.range(of: #"\d+$"#, options: .regularExpression) {
            return Int(stem[m]) ?? 0
        }
        return 0
    }

    // -----------------------------------------------------------------------
    // MARK: – Infer track grouping from file naming patterns
    // When PTX block parsing yields no tracks, group audio files by their
    // common name prefix so related takes land on the same logical track.
    // e.g. "Kick_01.wav", "Kick_02.wav" → one "Kick" track with 2 clips.
    // Files with no recognisable pattern each get their own track.
    // -----------------------------------------------------------------------
    private func inferTracksFromFileNames(audioFiles: [PTAudioFile]) -> [PTTrack] {
        // Strip numeric suffix and common separators to find the stem
        func stem(_ filename: String) -> String {
            var s = filename
            // Remove extension
            if let dot = s.lastIndex(of: ".") { s = String(s[s.startIndex ..< dot]) }
            // Strip trailing _01, -01, .01, (1) etc.
            let pattern = #"[\s_\-\.]+\d+$|[\s_\-\.]*\(\d+\)$"#
            if let re = try? NSRegularExpression(pattern: pattern),
               let m = re.firstMatch(in: s, range: NSRange(s.startIndex..., in: s)),
               let r = Range(m.range, in: s) {
                s = String(s[s.startIndex ..< r.lowerBound])
            }
            return s.trimmingCharacters(in: .whitespaces)
        }

        // Group files by stem, preserving insertion order of first occurrence
        var groups: [(stem: String, files: [PTAudioFile])] = []
        var stemIndex = [String: Int]()

        for af in audioFiles {
            let s = stem(af.filename)
            if let idx = stemIndex[s] {
                groups[idx].files.append(af)
            } else {
                stemIndex[s] = groups.count
                groups.append((stem: s, files: [af]))
            }
        }

        // Convert each group to a PTTrack with one placement per file at pos 0
        return groups.enumerated().map { (i, group) in
            let placements = group.files.enumerated().map { (j, af) in
                PTClipPlacement(regionIndex: af.index,
                                startInTimelineSamples: 0,
                                lengthSamples: af.lengthSamples)
            }
            return PTTrack(index: i,
                           name: group.stem.isEmpty ? group.files[0].filename : group.stem,
                           placements: placements,
                           isStereo: true)
        }
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
