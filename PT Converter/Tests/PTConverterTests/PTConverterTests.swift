import XCTest
import AVFoundation
@testable import PTConverter

final class SessionTextParserTests: XCTestCase {
    private func fixture (_ name: String) throws -> String {
        let url = Bundle.module.url (forResource: name, withExtension: "txt", subdirectory: "Fixtures")!
        return try PTSessionTextParser.load (url)
    }

    func testTimecodeExport() throws {
        let s = try PTSessionTextParser().parse (fixture ("OneOfEverything_Timecode"))
        XCTAssertEqual (s.name, "SessionText_OneOfEverything")
        XCTAssertEqual (s.sampleRate, 48000)
        XCTAssertEqual (s.timeFormat, .timecode)
        XCTAssertEqual (s.tracks.count, 5)
        // Audio 1 starts at 01:00:00:00 in a session starting 00:59:55:00 at 23.976 -> 5 s of TC = 120 frames * 1001/24000 s
        let a1 = s.tracks[0].clips[0]
        XCTAssertEqual (a1.start, Int64 ((120.0 * 1001.0 / 24000.0 * 48000.0).rounded()), accuracy: 2)
        // stereo track: two channels, clip list knows which file channel each side is
        XCTAssertEqual (s.tracks[2].channels, [1, 2])
        XCTAssertEqual (s.clipSource["Audio 3 Clip1.R"]?.file, "Audio 3 Clip1.wav")
        XCTAssertEqual (s.clipSource["Audio 3 Clip1.R"]?.channel, 2)
        // muted clip, fades, inactive track
        XCTAssertTrue (s.tracks[2].clips.contains { $0.name == "Audio 3 Clip2.L" && $0.muted })
        XCTAssertEqual (s.tracks[3].clips.filter { $0.kind == .fadeIn }.count, 2)
        XCTAssertTrue (s.tracks[4].inactive)
        XCTAssertEqual (s.markers.count, 2)
    }

    func testSamplesAndMinSecs() throws {
        let s = try PTSessionTextParser().parse (fixture ("Samples"))
        XCTAssertEqual (s.timeFormat, .samples)
        let b = s.tracks.first { $0.name == "Audio B" }!.clips[0]
        XCTAssertEqual (b.start, 401408)
        XCTAssertEqual (b.end, 1043408)
        let m = try PTSessionTextParser().parse (fixture ("MinSecs"))
        XCTAssertEqual (m.timeFormat, .minSecs)
        XCTAssertEqual (m.markers.first?.position, 2695168)   // uses the exact sample reference when present
    }

    func testBarsBeatsAsksForSamples() throws {
        XCTAssertThrowsError (try PTSessionTextParser().parse (fixture ("BarsBeats")))
    }
}

// A full session built from scratch: mono vocal (edited clip + fade in),
// split-mono stereo guitar, interleaved stereo pad.
final class EndToEndTests: XCTestCase {
    let sr = 48000.0
    var dir: URL!

    override func setUpWithError() throws {
        dir = FileManager.default.temporaryDirectory.appendingPathComponent ("ptc-" + UUID().uuidString)
        try FileManager.default.createDirectory (at: dir.appendingPathComponent ("Audio Files"), withIntermediateDirectories: true)
    }
    override func tearDownWithError() throws { try? FileManager.default.removeItem (at: dir) }

    /// deterministic, position-revealing signal: value encodes the frame index
    func sig (_ i: Int, _ seed: Int) -> Float { Float (sin (Double (i) * 0.001 * Double (seed + 1)) * 0.5 + Double ((i * 7919 + seed * 104729) % 1000) / 4000.0) }

    func writeWav (_ name: String, channels: Int, frames: Int, seed: Int, timeReference: UInt64?) throws -> URL {
        let url = dir.appendingPathComponent ("Audio Files").appendingPathComponent (name)
        // hand-written WAV: fmt (32-bit float) + optional bext (so we control the BWF timestamp)
        var d = Data()
        func u32 (_ v: UInt32) { var x = v.littleEndian; d.append (Data (bytes: &x, count: 4)) }
        func u16 (_ v: UInt16) { var x = v.littleEndian; d.append (Data (bytes: &x, count: 2)) }
        var body = Data()
        func chunk (_ id: String, _ payload: Data) { body.append (id.data (using: .ascii)!); var n = UInt32 (payload.count).littleEndian; body.append (Data (bytes: &n, count: 4)); body.append (payload); if payload.count % 2 == 1 { body.append (0) } }
        var fmt = Data(); func f16 (_ v: UInt16) { var x = v.littleEndian; fmt.append (Data (bytes: &x, count: 2)) }; func f32 (_ v: UInt32) { var x = v.littleEndian; fmt.append (Data (bytes: &x, count: 4)) }
        f16 (3); f16 (UInt16 (channels)); f32 (UInt32 (sr)); f32 (UInt32 (sr) * UInt32 (channels) * 4); f16 (UInt16 (channels * 4)); f16 (32)
        chunk ("fmt ", fmt)
        if let tr = timeReference {
            var bext = Data (count: 602)
            var t = tr.littleEndian
            bext.replaceSubrange (338..<346, with: Data (bytes: &t, count: 8))
            chunk ("bext", bext)
        }
        var pcm = Data (capacity: frames * channels * 4)
        for i in 0..<frames { for c in 0..<channels { var v = sig (i, seed + c * 10).bitPattern.littleEndian; pcm.append (Data (bytes: &v, count: 4)) } }
        chunk ("data", pcm)
        d.append ("RIFF".data (using: .ascii)!); u32 (UInt32 (4 + body.count)); d.append ("WAVE".data (using: .ascii)!); d.append (body)
        _ = u16
        try d.write (to: url)
        return url
    }

    func testExactPlacementMonoStereo() throws {
        let start = "01:00:00:00"                       // session start, 30 fps
        let startAbs: UInt64 = 3600 * 48000
        // vocal recorded at session 2 s; the clip used starts 1 s into the file
        _ = try writeWav ("Vox_01.wav", channels: 1, frames: 480000, seed: 1, timeReference: startAbs + 96000)
        _ = try writeWav ("Gtr.L.wav", channels: 1, frames: 240000, seed: 2, timeReference: startAbs)
        _ = try writeWav ("Gtr.R.wav", channels: 1, frames: 240000, seed: 3, timeReference: startAbs)
        _ = try writeWav ("Pad.wav", channels: 2, frames: 300000, seed: 4, timeReference: nil)
        let loc = "Macintosh HD:nowhere:"                // not on disk -> found in the session folder instead
        let tsVox = Int64 (startAbs) + 96000 + 48000      // file timestamp + 1 s
        let text = """
        SESSION NAME:\tTestSong
        SAMPLE RATE:\t48000.000000
        BIT DEPTH:\t24-bit
        SESSION START TIMECODE:\t\(start)
        TIMECODE FORMAT:\t30 Frame
        # OF AUDIO TRACKS:\t3


        O N L I N E  F I L E S  I N  S E S S I O N
        Filename\tLocation
        Vox_01.wav\t\(loc)
        Gtr.L.wav\t\(loc)
        Gtr.R.wav\t\(loc)
        Pad.wav\t\(loc)


        O N L I N E  C L I P S  I N  S E S S I O N
        CLIP NAME\tSource File
        Vox_01-01\tVox_01.wav
        Gtr.L\tGtr.L.wav
        Gtr.R\tGtr.R.wav
        Pad.L\tPad.wav\t[1]
        Pad.R\tPad.wav\t[2]


        T R A C K  L I S T I N G
        TRACK NAME:\tLead Vox
        COMMENTS:\t
        USER DELAY:\t0 Samples
        STATE:\t
        CHANNEL \tEVENT   \tCLIP NAME \tSTART TIME \tEND TIME \tDURATION \tTIMESTAMP \tSTATE
        1\t1\t(fade in)\t139200\t144000\t4800\t\t Unmuted
        1\t2\tVox_01-01\t144000\t240000\t96000\t\(tsVox)\tUnmuted


        TRACK NAME:\tGuitar
        COMMENTS:\t
        USER DELAY:\t0 Samples
        STATE:\t
        CHANNEL \tEVENT   \tCLIP NAME \tSTART TIME \tEND TIME \tDURATION \tTIMESTAMP \tSTATE
        1\t1\tGtr.L\t48000\t288000\t240000\t\(startAbs)\tUnmuted


        2\t1\tGtr.R\t48000\t288000\t240000\t\(startAbs)\tUnmuted


        TRACK NAME:\tPad
        COMMENTS:\t
        USER DELAY:\t0 Samples
        STATE:\t
        CHANNEL \tEVENT   \tCLIP NAME \tSTART TIME \tEND TIME \tDURATION \tTIMESTAMP \tSTATE
        1\t1\tPad.L\t0\t300000\t300000\t0\tMuted


        2\t1\tPad.R\t0\t300000\t300000\t0\tMuted
        """
        let parsed = try PTSessionTextParser().parse (text)
        XCTAssertTrue (parsed.hasTimestamps)
        let plan = SessionBuilder (text: parsed, sessionFolder: dir).build()
        XCTAssertTrue (plan.missingFiles.isEmpty, "\(plan.missingFiles)")

        let vox = plan.tracks[0], gtr = plan.tracks[1], pad = plan.tracks[2]
        XCTAssertFalse (vox.stereo, "vocal stays mono")
        XCTAssertTrue (gtr.stereo && pad.stereo, "stereo tracks stay stereo")
        XCTAssertEqual (vox.clips.count, 1)
        let vc = vox.clips[0]
        XCTAssertEqual (vc.offset, 48000 - 4800, "1 s into the file, pulled back by the 0.1 s fade-in")
        XCTAssertEqual (vc.start, 139200)
        XCTAssertEqual (vc.fadeIn, 4800)
        XCTAssertTrue (vc.exactOffset)
        if case .pair = gtr.clips[0].source {} else { XCTFail ("split L/R guitar should be joined into stereo") }
        if case .file = pad.clips[0].source {} else { XCTFail ("interleaved pad uses its own file") }
        XCTAssertTrue (pad.clips[0].muted)

        // export + render, then check the audio lands on the exact samples
        let out = dir.appendingPathComponent ("out"); let audio = out.appendingPathComponent ("Audio")
        try FileManager.default.createDirectory (at: audio, withIntermediateDirectories: true)
        let ex = PlanExporter()
        let src = ex.prepareSources (plan, outAudio: audio) { _ in }
        let session = try ex.writeNovaStudio (plan, sources: src, outRoot: out)
        let json = try JSONSerialization.jsonObject (with: Data (contentsOf: session)) as! [String: Any]
        let tracks = json["tracks"] as! [[String: Any]]
        XCTAssertEqual (tracks[0]["isStereo"] as? Bool, false)
        XCTAssertEqual (tracks[1]["isStereo"] as? Bool, true)
        let voxClip = (tracks[0]["clips"] as! [[String: Any]])[0]
        XCTAssertEqual (voxClip["fileOffsetSamples"] as? Double, 43200)
        XCTAssertEqual (voxClip["startSample"] as? Double, 139200)

        let stems = try ex.renderStems (plan, sources: src, outDir: out, aac: false) { _ in }
        let voxStem = try AVAudioFile (forReading: stems.first { $0.lastPathComponent == "Lead Vox.wav" }!)
        XCTAssertEqual (voxStem.fileFormat.channelCount, 1, "mono stem stays mono")
        let buf = AVAudioPCMBuffer (pcmFormat: voxStem.processingFormat, frameCapacity: AVAudioFrameCount (voxStem.length))!
        try voxStem.read (into: buf)
        let p = buf.floatChannelData![0]
        // after the fade-in, stem sample at 150000 = vocal file sample 48000 + (150000 - 144000)
        var worst: Float = 0
        for i in 150000..<150500 { worst = max (worst, abs (p[i] - sig (i - 144000 + 48000, 1))) }
        XCTAssertLessThan (worst, 1.0e-4, "vocal lands on the exact samples")
        XCTAssertEqual (p[100000], 0, "silence before the clip")
        let gtrStem = try AVAudioFile (forReading: stems.first { $0.lastPathComponent == "Guitar.wav" }!)
        XCTAssertEqual (gtrStem.fileFormat.channelCount, 2)
        XCTAssertNil (stems.first { $0.lastPathComponent == "Pad.wav" }.flatMap { u -> URL? in
            let f = try? AVAudioFile (forReading: u); let b = AVAudioPCMBuffer (pcmFormat: f!.processingFormat, frameCapacity: 1000)!
            try? f!.read (into: b, frameCount: 1000); return b.floatChannelData![0][500] != 0 ? u : nil
        }, "muted clip stays silent in stems")
    }
}
