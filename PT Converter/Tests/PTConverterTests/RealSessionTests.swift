import XCTest
import AVFoundation
@testable import PTConverter

// Real Pro Tools sessions on this Mac, read with the bundled ptformat helper.
// Skipped automatically where a session isn't present.
final class RealSessionTests: XCTestCase {
    let home = FileManager.default.homeDirectoryForCurrentUser.path
    lazy var helper = URL (fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        .deletingLastPathComponent().appendingPathComponent ("_shared/third_party/ptformat/ptx2json")

    override func setUp() { setenv ("NOVA_PTX2JSON", helper.path, 1) }

    func session (_ rel: String) throws -> (URL, URL) {
        let ptx = URL (fileURLWithPath: home + "/" + rel)
        try XCTSkipUnless (FileManager.default.fileExists (atPath: ptx.path), "not on this Mac: \(rel)")
        return (ptx, ptx.deletingLastPathComponent())
    }

    func check (_ rel: String, rate: Double, stereoTrack: String? = nil, monoTrack: String? = nil) throws -> SessionPlan {
        let (ptx, folder) = try session (rel)
        let j = try PTXReader.read (ptx)
        XCTAssertEqual (j.sampleRate, rate)
        let plan = PTXReader.plan (j, sessionName: "t", sessionFolder: folder)
        XCTAssertGreaterThan (plan.clipCount, 0)
        // anything still missing must really be gone from this Mac (Pro Tools would show it offline too)
        for m in plan.missingFiles {
            var cache: [String: URL] = [:]
            XCTAssertNil (PTXReader.find (m, in: FileManager.default.temporaryDirectory, cache: &cache), "\(m) exists but wasn't found")
        }
        if let s = stereoTrack { XCTAssertEqual (plan.tracks.first { $0.name == s }?.stereo, true, "\(s) should be stereo") }
        if let m = monoTrack { XCTAssertEqual (plan.tracks.first { $0.name == m }?.stereo, false, "\(m) should be mono") }
        // every clip fits inside its audio file, clips on a track don't overlap
        for t in plan.tracks {
            var lastEnd: Int64 = -1
            for c in t.clips {
                guard case .file (let u) = c.source, let f = try? AVAudioFile (forReading: u) else { continue }
                let fileLen = Int64 (Double (f.length) * rate / f.fileFormat.sampleRate)
                XCTAssertLessThanOrEqual (c.offset + c.length, fileLen + 1, "\(t.name): \(c.name) runs past its file")
                XCTAssertGreaterThanOrEqual (c.start, lastEnd - 1, "\(t.name): \(c.name) overlaps the previous clip")
                lastEnd = c.start + c.length
            }
        }
        print ("\(rel): \(plan.tracks.count) tracks, \(plan.clipCount) clips, \(plan.tracks.filter (\.stereo).count) stereo")
        return plan
    }

    func testBringItBack_44k() throws {
        _ = try check ("Downloads/bring it back/bring it back scalez verse 1.ptx", rate: 44100,
                       stereoTrack: "Bring it back ruff for scalez", monoTrack: "Audio 1")
    }

    func testSuenosGuitars_96k() throws {
        let plan = try check ("Desktop/zip folders/SUENOS/SUENOS GUITARS/SUENOS GUITARS.ptx", rate: 96000)
        // 96 kHz stays 96 kHz through conversion
        let out = FileManager.default.temporaryDirectory.appendingPathComponent ("ptc96-" + UUID().uuidString)
        let audio = out.appendingPathComponent ("Audio")
        try FileManager.default.createDirectory (at: audio, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem (at: out) }
        var small = plan
        small.tracks = [plan.tracks.first { !$0.clips.isEmpty }!]
        small.tracks[0].clips = Array (small.tracks[0].clips.prefix (2))
        let ex = PlanExporter()
        let src = ex.prepareSources (small, outAudio: audio) { fputs ("LOG " + $0 + "\n", stderr) }
        XCTAssertFalse (src.isEmpty)
        for p in src.values { XCTAssertEqual (p.sampleRate, 96000, "converted audio keeps 96 kHz") }
        let file = try ex.writeNovaStudio (small, sources: src, outRoot: out)
        let json = try JSONSerialization.jsonObject (with: Data (contentsOf: file)) as! [String: Any]
        XCTAssertEqual (json["sampleRate"] as? Double, 96000)
        let clip = ((json["tracks"] as! [[String: Any]])[0]["clips"] as! [[String: Any]])[0]
        XCTAssertEqual (clip["startSample"] as? Double, Double (small.tracks[0].clips[0].start), "positions unchanged at 96 kHz")
    }

    func testPeopleNew_96k() throws {
        _ = try check ("Desktop/bad bunny/people/people/people new/people new.ptx", rate: 96000)
    }

    func testBadBunny_96k() throws {
        _ = try check ("Desktop/bad bunny/bad bunny/bad bunny.ptx", rate: 96000)
    }

    /// Whole session: every clip's audio prepared and written into the .novastudio file
    func testFullConversion_BringItBack() throws {
        let plan = try check ("Downloads/bring it back/bring it back scalez verse 1.ptx", rate: 44100)
        let out = FileManager.default.temporaryDirectory.appendingPathComponent ("ptcfull-" + UUID().uuidString)
        let audio = out.appendingPathComponent ("Audio")
        try FileManager.default.createDirectory (at: audio, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem (at: out) }
        let ex = PlanExporter()
        var problems: [String] = []
        let src = ex.prepareSources (plan, outAudio: audio) { if $0.hasPrefix ("⚠︎") { problems.append ($0) } }
        XCTAssertTrue (problems.isEmpty, "\(problems)")
        let file = try ex.writeNovaStudio (plan, sources: src, outRoot: out)
        let json = try JSONSerialization.jsonObject (with: Data (contentsOf: file)) as! [String: Any]
        let tracks = json["tracks"] as! [[String: Any]]
        let written = tracks.reduce (0) { $0 + ($1["clips"] as! [Any]).count }
        XCTAssertEqual (written, plan.clipCount, "every clip made it into the session")
        // stereo track's file really is stereo, mono tracks' files mono
        for (i, t) in plan.tracks.enumerated() {
            guard let c = (tracks[i]["clips"] as! [[String: Any]]).first, let path = c["file"] as? String,
                  let f = try? AVAudioFile (forReading: URL (fileURLWithPath: path)) else { continue }
            XCTAssertEqual (Int (f.fileFormat.channelCount), t.stereo ? 2 : 1, "\(t.name) channel count")
        }
    }

    /// Writes real sessions as Nova Studio sessions for other tools to test with
    /// (only when NOVA_EXPORT_DIR is set).
    func testExportSessionsForMixAssistant() throws {
        guard let outDir = ProcessInfo.processInfo.environment["NOVA_EXPORT_DIR"] else { throw XCTSkip ("NOVA_EXPORT_DIR not set") }
        let list = ["Downloads/bring it back/bring it back scalez verse 1.ptx",
                    "Desktop/bad bunny/bad bunny/bad bunny.ptx",
                    "Desktop/bad bunny/people/people/people new/people new.ptx",
                    "Downloads/SUAVE/brooklyn.ptx",
                    "Desktop/zip folders/SUENOS/SUENOS GUITARS/SUENOS GUITARS.ptx"]
        for rel in list {
            let ptx = URL (fileURLWithPath: home + "/" + rel)
            guard FileManager.default.fileExists (atPath: ptx.path) else { continue }
            let name = ptx.deletingPathExtension().lastPathComponent
            let j = try PTXReader.read (ptx)
            let plan = PTXReader.plan (j, sessionName: name, sessionFolder: ptx.deletingLastPathComponent())
            let out = URL (fileURLWithPath: outDir).appendingPathComponent (name)
            let audio = out.appendingPathComponent ("Audio")
            try FileManager.default.createDirectory (at: audio, withIntermediateDirectories: true)
            let ex = PlanExporter()
            let src = ex.prepareSources (plan, outAudio: audio) { _ in }
            let file = try ex.writeNovaStudio (plan, sources: src, outRoot: out)
            fputs ("EXPORTED \(file.path)\n", stderr)
        }
    }

    func testBrooklyn_44k() throws {
        _ = try check ("Downloads/SUAVE/brooklyn.ptx", rate: 44100)
    }
}
