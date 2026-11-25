import Foundation
import AVFoundation

// ---------------------------------------------------------------------------
// Writes a SessionPlan out: audio files (converted, split-mono pairs merged to
// stereo), the .novastudio session with every clip in place, or rendered stems.
// ---------------------------------------------------------------------------

final class PlanExporter {
    private let converter = AudioConverter()

    struct Prepared { var url: URL; var sampleRate: Double; var frames: Int64 }

    /// Converts / merges every source the plan uses. Returns SourceKey -> prepared WAV.
    func prepareSources (_ plan: SessionPlan, outAudio: URL, log: (String) -> Void) -> [SourceKey: Prepared] {
        var keys: [SourceKey] = []
        for t in plan.tracks { for c in t.clips where !keys.contains (c.source) { keys.append (c.source) } }
        var out: [SourceKey: Prepared] = [:]
        var usedNames = Set<String>()
        func uniqueName (_ base: String) -> String {
            var n = base, i = 2
            while usedNames.contains (n.lowercased()) { n = "\(base) \(i)"; i += 1 }
            usedNames.insert (n.lowercased())
            return n
        }
        for (i, k) in keys.enumerated() {
            do {
                switch k {
                case .file (let u):
                    log ("Converting [\(i + 1)/\(keys.count)] \(u.lastPathComponent)")
                    let r = try converter.convertToWAV (src: u, outDir: outAudio, newName: uniqueName (u.deletingPathExtension().lastPathComponent))
                    if let f = try? AVAudioFile (forReading: r.outputURL) {
                        out[k] = Prepared (url: r.outputURL, sampleRate: f.fileFormat.sampleRate, frames: f.length)
                    }
                case .pair (let l, let r):
                    var base = l.deletingPathExtension().lastPathComponent
                    for suf in [".L", "_L", ".l"] where base.hasSuffix (suf) { base = String (base.dropLast (suf.count)) }
                    log ("Joining [\(i + 1)/\(keys.count)] \(l.lastPathComponent) + \(r.lastPathComponent) → stereo")
                    let dest = outAudio.appendingPathComponent (uniqueName (base) + ".wav")
                    out[k] = try mergeToStereo (left: l, right: r, dest: dest)
                }
            } catch {
                log ("⚠︎ Couldn't prepare \(k): \(error.localizedDescription)")
            }
        }
        return out
    }

    /// Two mono files -> one interleaved stereo 24-bit WAV (max 48 kHz like the rest)
    func mergeToStereo (left: URL, right: URL, dest: URL) throws -> Prepared {
        let lf = try AVAudioFile (forReading: left), rf = try AVAudioFile (forReading: right)
        let rate = min (lf.processingFormat.sampleRate, 48000)
        let frames = max (lf.length, rf.length)
        let fmt = AVAudioFormat (standardFormatWithSampleRate: lf.processingFormat.sampleRate, channels: 2)!
        let settings: [String: Any] = [AVFormatIDKey: kAudioFormatLinearPCM, AVSampleRateKey: lf.processingFormat.sampleRate,
                                       AVNumberOfChannelsKey: 2, AVLinearPCMBitDepthKey: 24, AVLinearPCMIsFloatKey: false,
                                       AVLinearPCMIsBigEndianKey: false, AVLinearPCMIsNonInterleaved: false]
        let tmp = dest.deletingLastPathComponent().appendingPathComponent (".merge-" + UUID().uuidString + ".wav")
        do {
            let out = try AVAudioFile (forWriting: tmp, settings: settings, commonFormat: .pcmFormatFloat32, interleaved: false)
            let chunk: AVAudioFrameCount = 65536
            let lb = AVAudioPCMBuffer (pcmFormat: lf.processingFormat, frameCapacity: chunk)!
            let rb = AVAudioPCMBuffer (pcmFormat: rf.processingFormat, frameCapacity: chunk)!
            let ob = AVAudioPCMBuffer (pcmFormat: fmt, frameCapacity: chunk)!
            var done: Int64 = 0
            while done < frames {
                let n = AVAudioFrameCount (min (Int64 (chunk), frames - done))
                lb.frameLength = 0; rb.frameLength = 0
                if lf.framePosition < lf.length { try lf.read (into: lb, frameCount: n) }
                if rf.framePosition < rf.length { try rf.read (into: rb, frameCount: n) }
                ob.frameLength = n
                let o0 = ob.floatChannelData![0], o1 = ob.floatChannelData![1]
                let l0 = lb.floatChannelData![0], r0 = rb.floatChannelData![0]
                for k in 0..<Int (n) {
                    o0[k] = k < Int (lb.frameLength) ? l0[k] : 0
                    o1[k] = k < Int (rb.frameLength) ? r0[k] : 0
                }
                try out.write (from: ob)
                done += Int64 (n)
            }
        }
        // same 48 kHz ceiling as single files
        let final = try converter.convertToWAV (src: tmp, outDir: dest.deletingLastPathComponent(),
                                                newName: dest.deletingPathExtension().lastPathComponent)
        try? FileManager.default.removeItem (at: tmp)
        let f = try AVAudioFile (forReading: final.outputURL)
        _ = rate
        return Prepared (url: final.outputURL, sampleRate: f.fileFormat.sampleRate, frames: f.length)
    }

    /// The Nova Studio session. Positions are rescaled to the converted audio's rate.
    func writeNovaStudio (_ plan: SessionPlan, sources: [SourceKey: Prepared], outRoot: URL) throws -> URL {
        let outRate = min (plan.sampleRate, 48000)
        let k = outRate / plan.sampleRate
        func s (_ v: Int64) -> Double { (Double (v) * k).rounded() }

        var tracksJSON: [[String: Any]] = []
        for t in plan.tracks {
            var clips: [[String: Any]] = []
            for c in t.clips {
                guard let p = sources[c.source] else { continue }
                clips.append ([
                    "file": p.url.path, "originalFile": p.url.path,
                    "startSample": s (c.start), "lengthSamples": s (c.length), "fileOffsetSamples": s (c.offset),
                    "fadeInSamples": s (c.fadeIn), "fadeOutSamples": s (c.fadeOut),
                    "gainDb": 0.0, "isMidi": false, "muted": c.muted, "locked": false, "aligned": false, "isPreview": false,
                    "alignmentOffsetSamples": 0.0, "guideTrackIndex": -1, "guideClipIndex": -1, "sourceTrackIndex": -1,
                    "sourceClipIndex": -1, "alignVersion": 0, "alignmentTimestamp": "", "alignmentPhraseSensitivity": 0.75,
                    "alignmentConsonantPriority": 0.5, "alignmentSourceGuidePath": "", "alignmentWarpPoints": [] as [Any],
                    "clipColor": "ff4c6af5"
                ])
            }
            tracksJSON.append ([
                "name": t.name, "type": "Audio", "isStereo": t.stereo, "volumeDb": 0.0, "pan": 0.0,
                "muted": t.muted, "solo": false, "armed": false, "inputBus": "", "outputBus": "Main Out",
                "auxInputBusIndex": -1, "locked": false, "groupName": "", "colour": "ff4c6af5",
                "sendLevels": [-100.0, -100.0, -100.0, -100.0, -100.0, -100.0], "sendBusIndex": [-1, -1, -1, -1, -1, -1],
                "sendPreFader": [false, false, false, false, false, false], "automationLanes": [] as [Any], "clips": clips
            ])
        }
        let markers: [[String: Any]] = plan.markers.map { ["name": $0.name, "timeSeconds": Double ($0.position) / plan.sampleRate] }
        let root: [String: Any] = [
            "projectName": plan.name, "tempoBpm": 120.0, "sampleRate": outRate, "tracks": tracksJSON,
            "macros": [] as [Any], "markers": markers, "tempoMap": [["timeSeconds": 0.0, "bpm": 120.0]],
            "novaAlignSettings": [:] as [String: Any], "novaAlignPreviewEnabled": true
        ]
        let data = try JSONSerialization.data (withJSONObject: root, options: [.prettyPrinted, .sortedKeys])
        let file = outRoot.appendingPathComponent (plan.name + ".novastudio")
        try data.write (to: file)
        return file
    }

    /// One stem per track, every stem the full session length from 0, clips/fades/mutes applied.
    func renderStems (_ plan: SessionPlan, sources: [SourceKey: Prepared], outDir: URL, aac: Bool, log: (String) -> Void) throws -> [URL] {
        let outRate = min (plan.sampleRate, 48000)
        let k = outRate / plan.sampleRate
        let total = Int (Double (plan.sessionEnd) * k) + 1
        var written: [URL] = []
        for (ti, t) in plan.tracks.enumerated() where !t.clips.isEmpty {
            log ("Rendering stem [\(ti + 1)/\(plan.tracks.count)] \(t.name)")
            var L = [Float] (repeating: 0, count: total), R = [Float] (repeating: 0, count: total)
            for c in t.clips where !c.muted {
                guard let p = sources[c.source], let f = try? AVAudioFile (forReading: p.url) else { continue }
                let start = Int (Double (c.start) * k), len = Int (Double (c.length) * k)
                let off = Int64 (Double (c.offset) * k), fin = Int (Double (c.fadeIn) * k), fout = Int (Double (c.fadeOut) * k)
                guard len > 0, off < f.length else { continue }
                f.framePosition = off
                let n = AVAudioFrameCount (min (Int64 (len), f.length - off))
                guard let b = AVAudioPCMBuffer (pcmFormat: f.processingFormat, frameCapacity: n) else { continue }
                try f.read (into: b, frameCount: n)
                let ch0 = b.floatChannelData![0], ch1 = b.format.channelCount > 1 ? b.floatChannelData![1] : ch0
                for i in 0..<Int (b.frameLength) {
                    let at = start + i
                    if at < 0 || at >= total { continue }
                    var g: Float = 1
                    if fin > 0 && i < fin { g *= Float (i) / Float (fin) }
                    if fout > 0 && i >= len - fout { g *= Float (len - i) / Float (fout) }
                    L[at] += ch0[i] * g; R[at] += ch1[i] * g
                }
            }
            let safe = t.name.replacingOccurrences (of: "/", with: "-").replacingOccurrences (of: ":", with: "-")
            let url = outDir.appendingPathComponent (safe + (aac ? ".m4a" : ".wav"))
            // mono tracks stay mono, stereo tracks stay stereo - exactly as in Pro Tools
            let nch = t.stereo ? 2 : 1
            let settings: [String: Any] = aac
                ? [AVFormatIDKey: kAudioFormatMPEG4AAC, AVSampleRateKey: outRate, AVNumberOfChannelsKey: nch, AVEncoderBitRateKey: nch == 2 ? 256000 : 160000]
                : [AVFormatIDKey: kAudioFormatLinearPCM, AVSampleRateKey: outRate, AVNumberOfChannelsKey: nch, AVLinearPCMBitDepthKey: 24,
                   AVLinearPCMIsFloatKey: false, AVLinearPCMIsBigEndianKey: false, AVLinearPCMIsNonInterleaved: false]
            let out = try AVAudioFile (forWriting: url, settings: settings, commonFormat: .pcmFormatFloat32, interleaved: false)
            let fmt = AVAudioFormat (standardFormatWithSampleRate: outRate, channels: AVAudioChannelCount (nch))!
            let chunk = 65536
            var pos = 0
            while pos < total {
                let n = min (chunk, total - pos)
                let ob = AVAudioPCMBuffer (pcmFormat: fmt, frameCapacity: AVAudioFrameCount (n))!
                ob.frameLength = AVAudioFrameCount (n)
                for i in 0..<n { ob.floatChannelData![0][i] = L[pos + i]; if nch == 2 { ob.floatChannelData![1][i] = R[pos + i] } }
                try out.write (from: ob)
                pos += n
            }
            written.append (url)
        }
        return written
    }
}
