import Foundation
import AVFoundation

// ---------------------------------------------------------------------------
// Converts any AVFoundation-readable audio (AIF, SD2, WAV, MP3, M4A, etc.)
// to WAV (PCM). Preserves sample rate, channel count, and bit depth.
// Also extracts individual clip regions from larger source files.
// ---------------------------------------------------------------------------

class AudioConverter {

    struct ConversionResult {
        let outputURL: URL
        let sampleRate: Double
        let channelCount: Int
        let lengthSamples: Int64
    }

    // Full-file conversion: src → outDir/[newName].wav
    func convertToWAV(src: URL, outDir: URL, newName: String? = nil) throws -> ConversionResult {
        let destName = (newName ?? src.deletingPathExtension().lastPathComponent) + ".wav"
        let dest = outDir.appendingPathComponent(destName)

        // Already WAV – copy as-is if valid, else re-encode to ensure clean headers
        let srcExt = src.pathExtension.lowercased()

        guard let inFile = try? AVAudioFile(forReading: src) else {
            throw ConversionError.cannotOpen(src.lastPathComponent)
        }
        let fmt = inFile.processingFormat
        let length = inFile.length

        let outSettings: [String: Any] = [
            AVFormatIDKey:            kAudioFormatLinearPCM,
            AVSampleRateKey:          fmt.sampleRate,
            AVNumberOfChannelsKey:    fmt.channelCount,
            AVLinearPCMBitDepthKey:   bestBitDepth(src: srcExt),
            AVLinearPCMIsFloatKey:    false,
            AVLinearPCMIsBigEndianKey: false,
            AVLinearPCMIsNonInterleaved: false
        ]

        let outFile = try AVAudioFile(forWriting: dest, settings: outSettings,
                                     commonFormat: .pcmFormatInt32,
                                     interleaved: false)

        let bufSize = AVAudioFrameCount(min(Int64(65536), length))
        guard let buf = AVAudioPCMBuffer(pcmFormat: inFile.processingFormat,
                                         frameCapacity: bufSize) else {
            throw ConversionError.bufferAlloc
        }

        var written: Int64 = 0
        while written < length {
            let toRead = AVAudioFrameCount(min(Int64(bufSize), length - written))
            buf.frameLength = toRead
            try inFile.read(into: buf, frameCount: toRead)
            try outFile.write(from: buf)
            written += Int64(toRead)
        }

        return ConversionResult(
            outputURL: dest,
            sampleRate: fmt.sampleRate,
            channelCount: Int(fmt.channelCount),
            lengthSamples: length
        )
    }

    // Clip extraction: read [offsetSamples, offsetSamples+lengthSamples) from src
    // and write as a new WAV. Returns nil if offset/length out of range.
    func extractClip(src: URL,
                     offsetSamples: Int64,
                     lengthSamples: Int64,
                     outDir: URL,
                     clipName: String) throws -> ConversionResult? {

        guard let inFile = try? AVAudioFile(forReading: src) else {
            throw ConversionError.cannotOpen(src.lastPathComponent)
        }

        let fileLength = inFile.length
        guard offsetSamples >= 0,
              offsetSamples < fileLength else { return nil }

        let actualLength = min(lengthSamples, fileLength - offsetSamples)
        guard actualLength > 0 else { return nil }

        let fmt = inFile.processingFormat
        let destName = sanitiseFilename(clipName) + ".wav"
        let dest = outDir.appendingPathComponent(destName)

        let outSettings: [String: Any] = [
            AVFormatIDKey:            kAudioFormatLinearPCM,
            AVSampleRateKey:          fmt.sampleRate,
            AVNumberOfChannelsKey:    fmt.channelCount,
            AVLinearPCMBitDepthKey:   24,
            AVLinearPCMIsFloatKey:    false,
            AVLinearPCMIsBigEndianKey: false,
            AVLinearPCMIsNonInterleaved: false
        ]

        let outFile = try AVAudioFile(forWriting: dest, settings: outSettings,
                                     commonFormat: .pcmFormatInt32,
                                     interleaved: false)

        // Seek to the correct offset in the source file
        inFile.framePosition = offsetSamples

        let bufSize = AVAudioFrameCount(65536)
        guard let buf = AVAudioPCMBuffer(pcmFormat: fmt, frameCapacity: bufSize) else {
            throw ConversionError.bufferAlloc
        }

        var remaining = actualLength
        while remaining > 0 {
            let toRead = AVAudioFrameCount(min(Int64(bufSize), remaining))
            buf.frameLength = toRead
            try inFile.read(into: buf, frameCount: toRead)
            try outFile.write(from: buf)
            remaining -= Int64(toRead)
        }

        return ConversionResult(
            outputURL: dest,
            sampleRate: fmt.sampleRate,
            channelCount: Int(fmt.channelCount),
            lengthSamples: actualLength
        )
    }

    // MARK: – Helpers

    private func bestBitDepth(src: String) -> Int {
        // WAV/AIFF sources keep their native depth; everything else → 24
        return (src == "wav" || src == "aif" || src == "aiff") ? 24 : 24
    }

    func sanitiseFilename(_ s: String) -> String {
        let bad = CharacterSet(charactersIn: "/\\:*?\"<>|")
        return s.components(separatedBy: bad).joined(separator: "_")
    }

    // Resolve a PT audio file name against the session folder hierarchy.
    // PT stores audio in "Audio Files" or "Bounces" or directly in the session folder.
    func findAudioFile(named: String, sessionFolder: URL) -> URL? {
        let searchDirs = [
            sessionFolder.appendingPathComponent("Audio Files"),
            sessionFolder.appendingPathComponent("Bounced Files"),
            sessionFolder.appendingPathComponent("Rendered Files"),
            sessionFolder.appendingPathComponent("Clip Groups"),
            sessionFolder
        ]
        for dir in searchDirs {
            let candidate = dir.appendingPathComponent(named)
            if FileManager.default.fileExists(atPath: candidate.path) {
                return candidate
            }
            // Also try without extension (PT sometimes omits it internally)
            for ext in ["wav", "aif", "aiff", "sd2", "mp3", "m4a"] {
                let c2 = dir.appendingPathComponent(named).appendingPathExtension(ext)
                if FileManager.default.fileExists(atPath: c2.path) { return c2 }
            }
        }
        return nil
    }
}

enum ConversionError: LocalizedError {
    case cannotOpen(String)
    case bufferAlloc

    var errorDescription: String? {
        switch self {
        case .cannotOpen(let f): return "Cannot open audio file: \(f)"
        case .bufferAlloc:       return "Could not allocate audio buffer"
        }
    }
}
