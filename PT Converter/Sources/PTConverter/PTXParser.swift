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
    var writeDebugDump = true

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
                    // Skip for v90+ — track name scanner finds iCloud path garbage.
                    if tracks.isEmpty && !audioFiles.isEmpty && version < 90 {
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

        // For v90 (PT 2022+), use the ZMARK block scanner.
        // v90 block format: 5a [type2 LE] [size4 LE] [content]
        // This replaces ALL old block parsing for v90 — Ardour's type IDs don't apply.
        if version >= 90 {
            var v90Files  = [PTAudioFile]()
            var v90Regions = [PTRegion]()
            var v90Tracks  = [PTTrack]()
            parseV90ZmarkBlocks(data: data, sampleRate: sampleRate,
                                audioFiles: &v90Files, regions: &v90Regions,
                                tracks: &v90Tracks)
            if !v90Files.isEmpty  { audioFiles = v90Files  }
            if !v90Regions.isEmpty { regions   = v90Regions }
            if !v90Tracks.isEmpty  { tracks    = v90Tracks  }

            // If ZMARK parse found audio files but no tracks, use track name scanner
            // to build named tracks matched against the discovered audio files.
            if audioFiles.isEmpty {
                audioFiles = scanForAudioFilenames(data: data)
            }
            if tracks.isEmpty && !audioFiles.isEmpty {
                let v90Names = scanV90TrackNames(data: data)
                if !v90Names.isEmpty {
                    tracks = buildTracksFromNames(trackNames: v90Names, audioFiles: audioFiles)
                }
            }
        }

        // For modern PT (v90+), the region entry scanner and track name scanner
        // find iCloud path fragments and other garbage, not real track data.
        // Skip them and go straight to filename-based grouping which is reliable.
        if tracks.isEmpty && version < 90 {
            let regionEntries = scanRegionEntries(data: data, sampleRate: sampleRate)
            if !regionEntries.isEmpty {
                var syntheticRegions = [PTRegion]()
                tracks = buildTracksFromRegionEntries(
                    entries: regionEntries, audioFiles: audioFiles,
                    regions: &syntheticRegions)
                if regions.isEmpty { regions = syntheticRegions }
            }
        }

        if tracks.isEmpty && version < 90 {
            let trackNames = scanForTrackNames(data: data)
            if !trackNames.isEmpty && !audioFiles.isEmpty {
                tracks = buildTracksFromNames(trackNames: trackNames, audioFiles: audioFiles)
            }
        }

        // Filename-based grouping: works for any PT version when binary parsing fails.
        // Groups audio files by stem (strips take numbers, processing suffixes, .L/.R).
        // This matches exactly what PT track names produce for standard sessions.
        if tracks.isEmpty && !audioFiles.isEmpty {
            tracks = inferTracksFromFileNames(audioFiles: audioFiles)
        }

        // Validate tracks against audio files on disk. If none of the found track
        // names correspond to any audio file stem, the scanner found garbage
        // (iCloud path fragments, system strings, etc.). Replace with filename grouping.
        if !audioFiles.isEmpty {
            let audioStems = audioFiles.map {
                ($0.filename as NSString).deletingPathExtension.lowercased()
            }
            let anyMatch = tracks.contains { t in
                let lower = t.name.lowercased()
                return audioStems.contains { stem in
                    stem.contains(lower) || lower.contains(stem)
                }
            }
            // Only override if no track names matched any audio file at all.
            // Zero-length placements are expected for v90 sessions (exporter reads real WAV lengths).
            if !anyMatch {
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
        let ver = bytes.count > 0x14 ? bytes[0x14] : 0
        lines.append("PT Hex Dump: \(sessionName)  SR=\(sampleRate)  ver=\(ver) (0x\(String(ver, radix:16)))")
        lines.append("File size: \(bytes.count) bytes (0x\(String(bytes.count, radix:16)))")
        lines.append("")

        // Summary of parsed results
        lines.append("=== PARSED RESULTS ===")
        lines.append("Audio files: \(audioFiles.count)")
        for af in audioFiles { lines.append("  [\(af.index)] \(af.filename)  len=\(af.lengthSamples)") }
        lines.append("Regions: \(regions.count)")
        for r in regions { lines.append("  [\(r.index)] \(r.name)  file=\(r.fileIndex)  off=\(r.offsetInFileSamples)  len=\(r.lengthSamples)") }
        lines.append("Tracks: \(tracks.count)")
        for t in tracks { lines.append("  [\(t.index)] \(t.name)  clips=\(t.placements.count)") }
        lines.append("")

        // ZMARK block map — scan for all 5a blocks and list them
        lines.append("=== ZMARK BLOCK MAP (5a [type2 LE] [size4 LE]) ===")
        var bpos = 0
        while bpos < bytes.count - 7 {
            if bytes[bpos] == 0x5a {
                let btype = UInt16(bytes[bpos+1]) | (UInt16(bytes[bpos+2]) << 8)
                let bsize = Int(bytes[bpos+3]) | (Int(bytes[bpos+4]) << 8) |
                            (Int(bytes[bpos+5]) << 16) | (Int(bytes[bpos+6]) << 24)
                if bsize > 0 && bsize < 50_000_000 && bpos + 7 + bsize <= bytes.count {
                    // Sample first 32 content bytes as ASCII
                    let cstart = bpos + 7
                    let cend   = min(cstart + 32, bpos + 7 + bsize)
                    let preview = (cstart..<cend).map { bytes[$0] >= 0x20 && bytes[$0] < 0x7f ? Character(UnicodeScalar(bytes[$0])) : Character(".") }
                    lines.append(String(format: "0x%06x  type=0x%04x  size=%6d  |%@|", bpos, btype, bsize, String(preview)))
                    bpos += 7 + bsize
                    continue
                }
            }
            bpos += 1
        }
        lines.append("")

        // Full hex dump — dump entire file (up to 4MB to keep file manageable)
        let dumpEnd = min(4_000_000, bytes.count)
        lines.append("=== FULL HEX DUMP 0x000000 – 0x\(String(dumpEnd, radix:16)) ===")
        for row in stride(from: 0, to: dumpEnd, by: 16) {
            let end = min(row + 16, dumpEnd)
            let hexParts = (row..<end).map { String(format: "%02x", bytes[$0]) }
            let hex = hexParts.joined(separator: " ")
            let ascii = (row..<end).map { bytes[$0] >= 0x20 && bytes[$0] < 0x7f ? Character(UnicodeScalar(bytes[$0])) : Character(".") }
            lines.append(String(format: "%06x  %-47s  %@", row, hex, String(ascii)))
        }

        let desktop = FileManager.default.urls(for: .desktopDirectory, in: .userDomainMask).first!
        let dumpURL = desktop.appendingPathComponent("ptx_hex_\(sessionName).txt")
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
                    let afIdx = ti < audioFiles.count ? audioFiles[ti].index : ti
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
    // Actual binary format (confirmed from hex dump):
    //   [2b type LE] [4b nameLen LE] [name bytes] [10b prefix] [8b VALUE] [2-3b trailer]
    // VALUE bytes 4-7 (LE float32) = timeline position in seconds for clip entries.
    // Clip entries have sequential index >= 32 in the trailer; track entries are < 32.
    struct RegionEntry {
        let name: String
        let fileIndex: Int
        let lengthSamples: Int64
        let positionSamples: Int64   // timeline start position in samples
    }

    private func scanRegionEntries(data: Data, sampleRate: Double) -> [RegionEntry] {
        let bytes = Array(data)
        var result = [RegionEntry]()
        var seen = Set<String>()
        var i = 0x40

        while i < bytes.count - 28 {
            // Read 4-byte LE name length
            guard i + 6 <= bytes.count else { break }
            let nameLen = Int(bytes[i+2]) | (Int(bytes[i+3]) << 8) |
                          (Int(bytes[i+4]) << 16) | (Int(bytes[i+5]) << 24)
            guard nameLen >= 1 && nameLen <= 128 else { i += 1; continue }

            let nameStart = i + 6
            let nameEnd   = nameStart + nameLen
            guard nameEnd + 18 <= bytes.count else { i += 1; continue }

            // Validate name bytes are printable ASCII
            let nameBytes = bytes[nameStart ..< nameEnd]
            guard nameBytes.allSatisfy({ $0 >= 0x20 && $0 < 0x7f }) else { i += 1; continue }
            guard let name = String(bytes: nameBytes, encoding: .utf8) else { i += 1; continue }

            // After name: 10-byte prefix, then 8-byte VALUE
            let valueOffset = nameEnd + 10
            guard valueOffset + 8 <= bytes.count else { i += 1; continue }

            // VALUE bytes 4-7 as LE float32 = position in seconds
            let b4 = bytes[valueOffset + 4]
            let b5 = bytes[valueOffset + 5]
            let b6 = bytes[valueOffset + 6]
            let b7 = bytes[valueOffset + 7]
            let bits = UInt32(b4) | (UInt32(b5) << 8) | (UInt32(b6) << 16) | (UInt32(b7) << 24)
            let posSeconds = Float(bitPattern: bits)

            // Sanity check: position must be a finite non-negative float < 24 hours
            guard posSeconds.isFinite && posSeconds >= 0 && posSeconds < 86400 else { i += 1; continue }

            // Trailer index byte (2 bytes after VALUE)
            let trailerIdx = Int(bytes[valueOffset + 8]) | (Int(bytes[valueOffset + 9]) << 8)

            // Only clip entries (index >= 32) have valid timeline positions
            // Track entries (index < 32) have non-position VALUE bytes
            guard trailerIdx >= 32 else { i += 1; continue }

            if !seen.contains(name) {
                seen.insert(name)
                let posSamples = Int64(Double(posSeconds) * sampleRate)
                // fileIndex unknown from deep scan — use -1 so exporter skips audio-file-index path
                result.append(RegionEntry(name: name, fileIndex: -1,
                                          lengthSamples: 0,
                                          positionSamples: posSamples))
            }
            // Advance to next entry: skip past this full entry
            i = valueOffset + 10
        }
        return result
    }

    private func buildTracksFromRegionEntries(entries: [RegionEntry],
                                               audioFiles: [PTAudioFile],
                                               regions: inout [PTRegion]) -> [PTTrack] {
        guard !entries.isEmpty else { return [] }
        var tracks = [PTTrack]()
        for (idx, entry) in entries.enumerated() {
            let afIdx = bestMatchingFileIndex(name: entry.name, audioFiles: audioFiles)
                        ?? (idx < audioFiles.count ? audioFiles[idx].index : idx)
            let af = audioFiles.first(where: { $0.index == afIdx })
            let clipLength = af?.lengthSamples ?? entry.lengthSamples
            // Synthesize a PTRegion so the exporter can resolve the audio file
            regions.append(PTRegion(index: idx, name: entry.name,
                                    fileIndex: afIdx,
                                    offsetInFileSamples: 0,
                                    lengthSamples: clipLength))
            let placement = PTClipPlacement(
                regionIndex: idx,
                startInTimelineSamples: 0,
                lengthSamples: clipLength)
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
        // PT audio file naming convention:
        //   [TrackName][-ProcessingSuffix][_TakeNumber].ext
        //   [TrackName].L.wav  /  [TrackName].R.wav  (stereo pairs)
        // We strip all suffixes to recover the original track name.
        let ptProcessing = ["-Gain", "-Norm", "-TmShft", "-Fade", "-Rev", "-Comp",
                            "-cm.4", "-cm.3", "-cm.2", "-cm",
                            ".dup2-cm.2", ".dup1-cm.2", ".dupl-cm.2",
                            ".dup2-cm", ".dup1-cm", ".dupl-cm",
                            ".dup2", ".dup1", ".dupl"]

        func ptStem(_ filename: String) -> String {
            var s = filename
            // Remove extension
            if let dot = s.lastIndex(of: ".") { s = String(s[s.startIndex ..< dot]) }
            // Strip stereo channel suffix (.L / .R)
            if s.hasSuffix(".L") || s.hasSuffix(".R") { s = String(s.dropLast(2)) }
            // Strip trailing take number: _01, _002, -03, etc.
            let numPat = #"[\s_\-]+\d+$"#
            func stripNum(_ str: String) -> String {
                guard let re = try? NSRegularExpression(pattern: numPat),
                      let m = re.firstMatch(in: str, range: NSRange(str.startIndex..., in: str)),
                      let r = Range(m.range, in: str) else { return str }
                return String(str[str.startIndex ..< r.lowerBound])
            }
            s = stripNum(s)
            // Strip PT processing suffix
            for sfx in ptProcessing {
                if s.hasSuffix(sfx) { s = String(s.dropLast(sfx.count)); break }
            }
            // Strip number again after removing processing suffix
            s = stripNum(s)
            return s.trimmingCharacters(in: .whitespaces)
        }

        // Identify stereo pairs: stems that have both .L and .R variants
        var stereoStems = Set<String>()
        let byStem = Dictionary(grouping: audioFiles, by: { ptStem($0.filename) })
        for (stem, files) in byStem {
            let hasL = files.contains { $0.filename.hasSuffix(".L.wav") || $0.filename.hasSuffix(".L.aif") || $0.filename.hasSuffix(".L.aiff") }
            let hasR = files.contains { $0.filename.hasSuffix(".R.wav") || $0.filename.hasSuffix(".R.aif") || $0.filename.hasSuffix(".R.aiff") }
            if hasL && hasR { stereoStems.insert(stem) }
        }

        // Group files by stem, preserving insertion order of first occurrence
        var groups: [(stem: String, files: [PTAudioFile])] = []
        var stemIndex = [String: Int]()

        for af in audioFiles {
            let s = ptStem(af.filename)
            // For stereo tracks, skip .R files — only the .L file represents the clip
            let isRChannel = stereoStems.contains(s) &&
                (af.filename.hasSuffix(".R.wav") || af.filename.hasSuffix(".R.aif") || af.filename.hasSuffix(".R.aiff"))
            if isRChannel { continue }

            if let idx = stemIndex[s] {
                groups[idx].files.append(af)
            } else {
                stemIndex[s] = groups.count
                groups.append((stem: s, files: [af]))
            }
        }

        return groups.enumerated().map { (i, group) in
            let isStereo = stereoStems.contains(group.stem)
            let placements = group.files.enumerated().map { (_, af) in
                PTClipPlacement(regionIndex: af.index,
                                startInTimelineSamples: 0,
                                lengthSamples: af.lengthSamples)
            }
            return PTTrack(index: i,
                           name: group.stem.isEmpty ? group.files[0].filename : group.stem,
                           placements: placements,
                           isStereo: isStereo)
        }
    }

    // -----------------------------------------------------------------------
    // MARK: – PTX v90 ZMARK block parser
    // -----------------------------------------------------------------------
    // v90 block format (confirmed from binary analysis of PT 2022+ sessions):
    //   [markerByte ^ blockKey]   — when decrypted with blockKey gives 0x5a
    //   [type 2b LE ^ blockKey]
    //   [size 4b LE ^ blockKey]
    //   [content bytes ^ blockKey]
    //
    // The blockKey is adaptive: at each block boundary, blockKey = rawByte ^ 0x5a.
    // This works for both the outer file and the globally-XOR'd buffer because the
    // global XOR key is absorbed into the per-block adaptive key detection.
    //
    // Confirmed structures (from hex analysis of "bring it back scalez" session):
    //   Audio file list: [count 4b LE][nameLen 4b LE][name][EVAW 4b][pad 5b] per entry
    //   Region block:    partial decode — region entries with name→fileIndex mapping
    //   Playlist block:  top-level track containers → type=0x0002 (48b) → type=0x000a (37b)
    //   Clip entry (37b): [4f 10 00 00][regionIdx 2b LE][00 00 00][timelinePos 7b LE][00][flag][pad 19b]
    //   flag=0x03 → active/rendered clip; flag=0x01 → superseded original
    // -----------------------------------------------------------------------

    // Internal clip entry decoded from the playlist block
    private struct V90Clip {
        let regionIdx: Int
        let timelinePos: Int64
        let flag: UInt8
    }

    private func parseV90ZmarkBlocks(data: Data, sampleRate: Double,
                                     audioFiles: inout [PTAudioFile],
                                     regions: inout [PTRegion],
                                     tracks: inout [PTTrack]) {
        let bytes = Array(data)

        // ── Step 1: Adaptive ZMARK block scanner ──────────────────────────────
        // blockKey = bytes[pos] ^ 0x5a; apply to type and size fields.
        // Accept when size is valid and block fits within file.
        struct ZBlock { let type: UInt16; let start: Int; let size: Int; let key: UInt8 }
        var blocks = [ZBlock]()
        var pos = 0
        while pos < bytes.count - 7 {
            let k   = bytes[pos] ^ 0x5a
            let t0  = bytes[pos+1] ^ k;  let t1 = bytes[pos+2] ^ k
            let s0  = bytes[pos+3] ^ k;  let s1 = bytes[pos+4] ^ k
            let s2  = bytes[pos+5] ^ k;  let s3 = bytes[pos+6] ^ k
            let btype = UInt16(t0) | (UInt16(t1) << 8)
            let bsize = Int(s0) | (Int(s1) << 8) | (Int(s2) << 16) | (Int(s3) << 24)
            if bsize > 0, bsize < 50_000_000, pos + 7 + bsize <= bytes.count {
                blocks.append(ZBlock(type: btype, start: pos + 7, size: bsize, key: k))
                pos += 7 + bsize
            } else {
                pos += 1
            }
        }

        // Helper: return a block's content decrypted with its adaptive key
        func decryptBlock(_ blk: ZBlock) -> [UInt8] {
            let k = blk.key
            if k == 0 { return Array(bytes[blk.start ..< blk.start + blk.size]) }
            return (blk.start ..< blk.start + blk.size).map { bytes[$0] ^ k }
        }

        // ── Step 2: Audio file list block (EVAW format) ───────────────────────
        var foundFiles = [PTAudioFile]()
        for blk in blocks {
            let content = decryptBlock(blk)
            if let files = tryParseV90AudioFileBlockEVAW(content) {
                if files.count > foundFiles.count { foundFiles = files }
            }
        }
        if !foundFiles.isEmpty { audioFiles = foundFiles }

        // ── Step 3: Region block — partial decode for name→fileIndex map ──────
        // Build a map: regionIdx → (regionName, fileIndex)
        // Used to resolve active clip → audio file below.
        var regionIdxToInfo = [Int: (name: String, fileIdx: Int)]()

        for blk in blocks {
            let content = decryptBlock(blk)
            let map = tryParseV90RegionContent(content,
                                               audioFiles: audioFiles.isEmpty ? foundFiles : audioFiles)
            if map.count > regionIdxToInfo.count { regionIdxToInfo = map }
        }

        // Materialise into PTRegion array (used by exporter for region→file lookups)
        if !regionIdxToInfo.isEmpty {
            var synth = [PTRegion]()
            for (idx, info) in regionIdxToInfo {
                let af = audioFiles.first(where: { $0.index == info.fileIdx })
                synth.append(PTRegion(index: idx, name: info.name,
                                      fileIndex: info.fileIdx,
                                      offsetInFileSamples: 0,
                                      lengthSamples: af?.lengthSamples ?? 0))
            }
            regions = synth.sorted { $0.index < $1.index }
        }

        // ── Step 4: Playlist block — per-track clip extraction ────────────────
        // The playlist block's decrypted content contains top-level track containers.
        // Each track container holds type=0x0002 (48b) clip blocks.
        // Each type=0x0002 block holds a type=0x000a (37b) clip entry.
        // We identify the playlist block as the one yielding the most track groups.

        let trackNames = scanV90TrackNames(data: data)
        var bestTrackGroups = [[V90Clip]]()

        for blk in blocks {
            let content = decryptBlock(blk)
            if let groups = tryParseV90PlaylistGroups(content) {
                if groups.count > bestTrackGroups.count { bestTrackGroups = groups }
            }
        }

        // ── Step 5: Build PTTrack objects ─────────────────────────────────────
        if !bestTrackGroups.isEmpty {
            let af = audioFiles.isEmpty ? foundFiles : audioFiles
            var builtTracks = [PTTrack]()
            let nameCount = trackNames.count

            for (ti, rawClips) in bestTrackGroups.enumerated() {
                // Filter to only the active clip at each timeline position
                let active = filterActiveV90Clips(rawClips)
                guard !active.isEmpty else { continue }

                let name = ti < nameCount ? trackNames[ti] : "Track \(ti + 1)"

                var placements = [PTClipPlacement]()
                for clip in active {
                    // Resolve region → file index
                    let fileIdx = resolveV90ClipFile(
                        clip: clip, trackName: name,
                        regionMap: regionIdxToInfo, audioFiles: af)

                    placements.append(PTClipPlacement(
                        regionIndex: fileIdx,
                        startInTimelineSamples: clip.timelinePos,
                        lengthSamples: 0))  // length 0 → exporter reads actual WAV duration
                }

                builtTracks.append(PTTrack(index: ti, name: name,
                                           placements: placements, isStereo: false))
            }

            if !builtTracks.isEmpty { tracks = builtTracks }
        }
    }

    // ── EVAW audio file list parser ──────────────────────────────────────────
    // Format per entry (confirmed): [nameLen 4b LE][name bytes][EVAW 4b][pad 5b]
    private func tryParseV90AudioFileBlockEVAW(_ content: [UInt8]) -> [PTAudioFile]? {
        guard content.count >= 9 else { return nil }

        // Try 4-byte LE count then 2-byte LE
        for countBytes in [4, 2] {
            var count = 0
            if countBytes == 4 {
                count = Int(content[0]) | (Int(content[1]) << 8) |
                        (Int(content[2]) << 16) | (Int(content[3]) << 24)
            } else {
                count = Int(content[0]) | (Int(content[1]) << 8)
            }
            guard count >= 1 && count <= 2000 else { continue }

            var cursor = countBytes
            var result = [PTAudioFile]()
            var ok = true

            for idx in 0 ..< count {
                guard cursor + 4 <= content.count else { ok = false; break }
                let nameLen = Int(content[cursor]) | (Int(content[cursor+1]) << 8) |
                              (Int(content[cursor+2]) << 16) | (Int(content[cursor+3]) << 24)
                cursor += 4
                guard nameLen >= 1 && nameLen <= 512 && cursor + nameLen <= content.count else {
                    ok = false; break
                }
                let nameBytes = Array(content[cursor ..< cursor + nameLen])
                guard nameBytes.allSatisfy({ $0 >= 0x20 && $0 < 0x7f }) else { ok = false; break }
                guard let name = String(bytes: nameBytes, encoding: .utf8) else { ok = false; break }
                let lower = name.lowercased()
                guard lower.hasSuffix(".wav") || lower.hasSuffix(".aif") ||
                      lower.hasSuffix(".aiff") || lower.hasSuffix(".sd2") ||
                      lower.hasSuffix(".mp3")  || lower.hasSuffix(".m4a") else { ok = false; break }
                cursor += nameLen

                // Skip EVAW marker (4 bytes) + 5 padding bytes = 9 bytes total
                // If EVAW is absent, try to find it within the next 16 bytes
                if cursor + 4 <= content.count &&
                   content[cursor]   == 0x45 && content[cursor+1] == 0x56 &&
                   content[cursor+2] == 0x41 && content[cursor+3] == 0x57 {
                    cursor += 4 + 5   // EVAW + 5 padding bytes
                }
                // (if EVAW not found, entries are tightly packed — just continue)

                result.append(PTAudioFile(index: idx, filename: name))
            }

            // Accept partial results: if we got at least 3 files or half the declared count.
            // The audio file list block may be partially encrypted with a different key
            // near the end; keep what we could decode.
            let threshold = max(3, count / 2)
            if result.count >= threshold { return result }
        }
        return nil
    }

    // ── Region content decoder ───────────────────────────────────────────────
    // Scans decrypted region block content for entries that look like:
    //   [regionIdx 2b LE][nameLen 4b LE][name bytes][fileIdx 2b LE][...]
    // Returns regionIdx → (name, fileIdx) map.
    private func tryParseV90RegionContent(_ content: [UInt8],
                                          audioFiles: [PTAudioFile]) -> [Int: (name: String, fileIdx: Int)] {
        var result = [Int: (name: String, fileIdx: Int)]()
        guard content.count >= 12 else { return result }

        var cursor = 0
        let end = content.count
        let afCount = audioFiles.count

        while cursor + 8 < end {
            // Look for a 2-byte LE index followed by a plausible 4-byte LE nameLen
            let idx = Int(content[cursor]) | (Int(content[cursor+1]) << 8)
            guard idx < 10000 else { cursor += 1; continue }

            let nameLen = Int(content[cursor+2]) | (Int(content[cursor+3]) << 8) |
                          (Int(content[cursor+4]) << 16) | (Int(content[cursor+5]) << 24)
            guard nameLen >= 1 && nameLen <= 128 && cursor + 6 + nameLen + 2 <= end else {
                cursor += 1; continue
            }

            let nameSlice = content[(cursor+6) ..< (cursor+6+nameLen)]
            guard nameSlice.allSatisfy({ $0 >= 0x20 && $0 < 0x7f }),
                  let name = String(bytes: nameSlice, encoding: .utf8) else {
                cursor += 1; continue
            }

            let fiCursor = cursor + 6 + nameLen
            guard fiCursor + 2 <= end else { cursor += 1; continue }
            let fileIdx = Int(content[fiCursor]) | (Int(content[fiCursor+1]) << 8)
            guard afCount == 0 || fileIdx < afCount + 200 else { cursor += 1; continue }

            // Validate by matching region name to an audio file stem
            let stem = name.lowercased()
            let nameMatches = afCount == 0 || audioFiles.contains {
                let afStem = ($0.filename as NSString).deletingPathExtension.lowercased()
                return afStem == stem || stem.hasPrefix(afStem.prefix(6))
            }
            guard nameMatches || afCount == 0 else { cursor += 1; continue }

            result[idx] = (name: name, fileIdx: fileIdx)
            cursor += 6 + nameLen + 2  // advance past the validated entry
        }

        return result
    }

    // ── Playlist block parser — returns per-track clip arrays ────────────────
    // Scans decrypted playlist content for top-level ZMARK containers.
    // Uses adaptive per-block key detection at every nesting level because nested
    // ZMARK blocks may carry their own keys within the already-decrypted outer content.
    private func tryParseV90PlaylistGroups(_ content: [UInt8]) -> [[V90Clip]]? {
        guard content.count > 51 else { return nil }

        // Quick check: scan a window for the 37-byte clip entry ZMARK signature.
        // The signature [5a 0a 00 25 00 00 00 4f 10 00 00] may appear literally (key=0)
        // or XOR'd with the inner block key. Check literal first, then try common keys.
        func windowHasClips(_ buf: [UInt8], xorKey: UInt8) -> Bool {
            let limit = min(buf.count - 10, 500_000)
            for i in 0 ..< limit {
                if (buf[i] ^ xorKey) == 0x4f && (buf[i+1] ^ xorKey) == 0x10 &&
                   (buf[i+2] ^ xorKey) == 0x00 && (buf[i+3] ^ xorKey) == 0x00 {
                    return true
                }
            }
            return false
        }
        let commonKeys: [UInt8] = [0x00, 0x5a, 0x64, 0x32, 0x2a, 0x3c, 0x14, 0x28]
        guard commonKeys.contains(where: { windowHasClips(content, xorKey: $0) }) else { return nil }

        var groups = [[V90Clip]]()
        var pos = 0

        while pos < content.count - 6 {
            // Adaptive key: k = content[pos] ^ 0x5a
            let k   = content[pos] ^ 0x5a
            let t0  = content[pos+1] ^ k; let t1 = content[pos+2] ^ k
            let s0  = content[pos+3] ^ k; let s1 = content[pos+4] ^ k
            let s2  = content[pos+5] ^ k; let s3 = content[pos+6] ^ k
            let btype = UInt16(t0) | (UInt16(t1) << 8)
            let bsize = Int(s0) | (Int(s1) << 8) | (Int(s2) << 16) | (Int(s3) << 24)
            guard bsize > 0, bsize < 10_000_000, pos + 7 + bsize <= content.count else {
                pos += 1; continue
            }

            // Decrypt this inner block's content with its adaptive key
            let sub: [UInt8]
            if k == 0 { sub = Array(content[(pos+7) ..< (pos+7+bsize)]) }
            else       { sub = (pos+7 ..< pos+7+bsize).map { content[$0] ^ k } }

            let clips = v90ExtractClips(sub)
            if !clips.isEmpty { groups.append(clips) }
            pos += 7 + bsize
        }

        guard groups.contains(where: { !$0.isEmpty }) else { return nil }
        return groups
    }

    // Extracts V90Clip entries from decrypted block content (up to maxDepth levels deep).
    // Uses adaptive key detection at every level.
    // Handles: outer container → type=0x0002 (48b) → type=0x000a (37b clip).
    private func v90ExtractClips(_ content: [UInt8], depth: Int = 0) -> [V90Clip] {
        guard depth < 4 else { return [] }   // cap recursion depth
        var clips = [V90Clip]()
        var pos = 0
        while pos < content.count - 6 {
            let k   = content[pos] ^ 0x5a
            let t0  = content[pos+1] ^ k; let t1 = content[pos+2] ^ k
            let s0  = content[pos+3] ^ k; let s1 = content[pos+4] ^ k
            let s2  = content[pos+5] ^ k; let s3 = content[pos+6] ^ k
            let btype = UInt16(t0) | (UInt16(t1) << 8)
            let bsize = Int(s0) | (Int(s1) << 8) | (Int(s2) << 16) | (Int(s3) << 24)
            guard bsize > 0, pos + 7 + bsize <= content.count else { pos += 1; continue }

            let sub: [UInt8]
            if k == 0 { sub = Array(content[(pos+7) ..< (pos+7+bsize)]) }
            else       { sub = (pos+7 ..< pos+7+bsize).map { content[$0] ^ k } }

            if btype == 0x000a && bsize == 37 {
                if let c = v90ParseClipEntry(sub) { clips.append(c) }
                pos += 7 + 37
            } else if btype == 0x0002 && bsize == 48 {
                clips.append(contentsOf: v90ExtractClips(sub, depth: depth + 1))
                pos += 7 + 48
            } else if bsize <= 65536 {
                // Recurse into small unknown blocks that might contain clips
                let inner = v90ExtractClips(sub, depth: depth + 1)
                if !inner.isEmpty { clips.append(contentsOf: inner) }
                pos += 7 + bsize
            } else {
                pos += 7 + bsize
            }
        }
        return clips
    }

    // Parse the confirmed 37-byte clip entry structure:
    //   [4f 10 00 00]         constant magic (4b)
    //   [regionIdx 2b LE]     index into region table (2b)
    //   [00 00 00]            zeros (3b)
    //   [timelinePos 7b LE]   sample position at session SR (7b)
    //   [00]                  zero (1b)
    //   [flag]                0x03 = active, 0x01 = superseded (1b)
    //   [padding 19b]
    private func v90ParseClipEntry(_ d: [UInt8]) -> V90Clip? {
        guard d.count >= 17 else { return nil }
        guard d[0] == 0x4f && d[1] == 0x10 && d[2] == 0x00 && d[3] == 0x00 else { return nil }
        let regionIdx = Int(d[4]) | (Int(d[5]) << 8)
        var timelinePos: Int64 = 0
        for b in 0 ..< 7 { timelinePos |= Int64(d[9 + b]) << (b * 8) }
        let flag = d[16]
        guard timelinePos >= 0 && timelinePos < 6_912_000_000_000 else { return nil }
        return V90Clip(regionIdx: regionIdx, timelinePos: timelinePos, flag: flag)
    }

    // Filter to one active clip per timeline position.
    // When flag=0x03 and flag=0x01 both exist at the same position, prefer 0x03.
    // Isolated flag=0x01 clips (no 0x03 partner) are also kept.
    private func filterActiveV90Clips(_ clips: [V90Clip]) -> [V90Clip] {
        var byPos = [Int64: [V90Clip]]()
        for c in clips { byPos[c.timelinePos, default: []].append(c) }
        var active = [V90Clip]()
        for (_, group) in byPos.sorted(by: { $0.key < $1.key }) {
            if group.count == 1 {
                active.append(group[0])
            } else if let rendered = group.first(where: { $0.flag == 0x03 }) {
                active.append(rendered)
            } else {
                active.append(group[0])
            }
        }
        return active
    }

    // Resolve an active clip's region index → audio file index.
    // Priority:
    //   1. Exact match in region map (region name → audio file by stem comparison)
    //   2. Name-based fuzzy match against track name prefix in audio files
    //   3. Raw region index as file index (last resort)
    private func resolveV90ClipFile(clip: V90Clip, trackName: String,
                                    regionMap: [Int: (name: String, fileIdx: Int)],
                                    audioFiles: [PTAudioFile]) -> Int {
        // 1. Region map lookup
        if let info = regionMap[clip.regionIdx] {
            // Verify the mapped file index exists
            if audioFiles.isEmpty || audioFiles.contains(where: { $0.index == info.fileIdx }) {
                return info.fileIdx
            }
            // Map gave us a name — try to match it to a file by stem
            let stemLower = info.name.lowercased()
            if let matched = audioFiles.first(where: {
                ($0.filename as NSString).deletingPathExtension.lowercased() == stemLower
            }) { return matched.index }
        }

        // 2. Track-name prefix match: find the "best" audio file for this track.
        //    Prefer processed takes ("-Gain", "-Norm", etc.) over raw ones.
        //    Among processed, prefer the file whose stem most closely matches.
        let trackLower = trackName.lowercased()
        let candidates = audioFiles.filter {
            let fl = $0.filename.lowercased()
            let st = ($0.filename as NSString).deletingPathExtension.lowercased()
            return fl.contains(trackLower) || trackLower.contains(st.prefix(min(6, st.count)))
        }

        // Prefer processed (contains a dash+suffix like "-Gain", "-Norm", etc.)
        let processed = candidates.filter {
            let st = ($0.filename as NSString).deletingPathExtension.lowercased()
            return st.contains("-gain") || st.contains("-norm") || st.contains("-tmshft") ||
                   st.contains("-rev") || st.contains("-comp")
        }

        // Among candidates, pick the one with highest take number
        func highestTake(_ af: PTAudioFile) -> Int {
            let s = (af.filename as NSString).deletingPathExtension
            if let m = s.range(of: #"\d+$"#, options: .regularExpression) {
                return Int(s[m]) ?? 0
            }
            return 0
        }

        if let best = (processed.isEmpty ? candidates : processed).max(by: {
            highestTake($0) < highestTake($1)
        }) { return best.index }

        // 3. Fallback: use region index directly as file index
        return clip.regionIdx
    }

    // ── Legacy helpers kept for non-v90 code paths ──────────────────────────
    private func tryParseV90AudioFileBlock(bytes: [UInt8], start: Int, size: Int) -> [PTAudioFile]? {
        let content = Array(bytes[start ..< min(start + size, bytes.count)])
        return tryParseV90AudioFileBlockEVAW(content)
    }

    private func tryParseV90RegionBlock(bytes: [UInt8], start: Int, size: Int,
                                         audioFileCount: Int) -> [PTRegion]? {
        return nil  // replaced by tryParseV90RegionContent in the adaptive path
    }

    private func tryParseV90PlaylistBlock(bytes: [UInt8], start: Int, size: Int,
                                           trackNames: [String], regions: [PTRegion],
                                           sampleRate: Double) -> [PTTrack]? {
        return nil  // replaced by tryParseV90PlaylistGroups in the adaptive path
    }

    // -----------------------------------------------------------------------
    // MARK: – PTX v90 exact track name scanner
    // -----------------------------------------------------------------------
    // Confirmed from hex dump: each track block contains the 2-byte marker
    // 1A 25 followed by a 2-byte type flag and a 4-byte LE name length.
    //   00 00 = audio track  (include)
    //   01 00 = audio track  (include)
    //   02 00 = aux/bus      (exclude)
    //   05 00 = master fader (exclude)
    private func scanV90TrackNames(data: Data) -> [String] {
        let bytes = Array(data)
        var names = [String]()
        var seen  = Set<String>()
        var i = 0x40

        while i < bytes.count - 8 {
            guard bytes[i] == 0x1a, bytes[i+1] == 0x25 else { i += 1; continue }

            let typeFlag = UInt16(bytes[i+2]) | (UInt16(bytes[i+3]) << 8)
            // Only audio tracks (type 00 or 01); skip aux (02), master (05), etc.
            guard typeFlag == 0x00 || typeFlag == 0x01 else { i += 1; continue }

            let nameLen = Int(bytes[i+4]) | (Int(bytes[i+5]) << 8) |
                          (Int(bytes[i+6]) << 16) | (Int(bytes[i+7]) << 24)
            guard nameLen >= 1 && nameLen <= 128 else { i += 1; continue }

            let nameStart = i + 8
            guard nameStart + nameLen <= bytes.count else { i += 1; continue }

            let nameBytes = bytes[nameStart ..< nameStart + nameLen]
            guard nameBytes.allSatisfy({ $0 >= 0x20 && $0 < 0x7f }),
                  let name = String(bytes: nameBytes, encoding: .utf8),
                  name.count >= 2
            else { i += 1; continue }

            if !seen.contains(name) {
                seen.insert(name)
                names.append(name)
            }
            i += 8 + nameLen
        }
        return names
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
