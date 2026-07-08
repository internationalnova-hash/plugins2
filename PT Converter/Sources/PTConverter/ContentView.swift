import SwiftUI
import UniformTypeIdentifiers
import AppKit

// ---------------------------------------------------------------------------
// Main UI – dark, Nova Studio-adjacent aesthetic
// ---------------------------------------------------------------------------

struct ContentView: View {

    @StateObject private var engine = ConversionEngine()
    @State private var isDragOver = false
    @State private var showOpenPanel = false

    // Colours
    let bgColor    = Color(red: 0.08, green: 0.08, blue: 0.12)
    let panelColor = Color(red: 0.12, green: 0.12, blue: 0.18)
    let accent     = Color(red: 0.29, green: 0.42, blue: 0.96) // Nova blue
    let accentGrn  = Color(red: 0.17, green: 0.80, blue: 0.44)

    var body: some View {
        ZStack {
            bgColor.ignoresSafeArea()

            VStack(spacing: 0) {
                header
                dropZone
                    .padding(.horizontal, 24)
                    .padding(.top, 20)
                progressSection
                    .padding(.horizontal, 24)
                    .padding(.top, 16)
                logSection
                    .padding(.horizontal, 24)
                    .padding(.top, 12)
                    .padding(.bottom, 16)
                footer
            }
        }
        .frame(minWidth: 640, minHeight: 540)
    }

    // ── Header ───────────────────────────────────────────────────────────────
    var header: some View {
        HStack(spacing: 12) {
            Image(systemName: "waveform.badge.magnifyingglass")
                .font(.system(size: 28))
                .foregroundColor(accent)

            VStack(alignment: .leading, spacing: 2) {
                Text("PT Converter")
                    .font(.system(size: 22, weight: .bold, design: .rounded))
                    .foregroundColor(.white)
                Text("Pro Tools → Nova Studio")
                    .font(.system(size: 12))
                    .foregroundColor(.gray)
            }
            Spacer()
            Text("by Nova Audio")
                .font(.system(size: 11, weight: .medium))
                .foregroundColor(.gray)
        }
        .padding(.horizontal, 24)
        .padding(.vertical, 18)
        .background(panelColor)
    }

    // ── Drop Zone ─────────────────────────────────────────────────────────────
    var dropZone: some View {
        ZStack {
            RoundedRectangle(cornerRadius: 16)
                .fill(panelColor)
                .overlay(
                    RoundedRectangle(cornerRadius: 16)
                        .strokeBorder(
                            isDragOver ? accent : Color.white.opacity(0.12),
                            style: StrokeStyle(lineWidth: isDragOver ? 2 : 1.5,
                                               dash: [8, 5])
                        )
                )
                .shadow(color: isDragOver ? accent.opacity(0.3) : .clear, radius: 20)
                .animation(.easeInOut(duration: 0.15), value: isDragOver)

            VStack(spacing: 16) {
                Image(systemName: engine.isRunning ? "gearshape.2.fill" : "square.and.arrow.down.on.square")
                    .font(.system(size: 48))
                    .foregroundColor(isDragOver ? accent : Color.white.opacity(0.35))
                    .rotationEffect(.degrees(engine.isRunning ? 360 : 0))
                    .animation(engine.isRunning ? .linear(duration: 1).repeatForever(autoreverses: false) : .default, value: engine.isRunning)

                if engine.isRunning {
                    Text("Converting…")
                        .font(.system(size: 18, weight: .semibold))
                        .foregroundColor(.white)
                } else if engine.outputSessionURL != nil {
                    Text("Conversion complete!")
                        .font(.system(size: 18, weight: .semibold))
                        .foregroundColor(accentGrn)
                    Text("Drop another session to convert again")
                        .font(.system(size: 13))
                        .foregroundColor(.gray)
                } else {
                    Text("Drop your Pro Tools session folder here")
                        .font(.system(size: 18, weight: .semibold))
                        .foregroundColor(.white)
                    Text("or click to browse — supports .ptx & .pts sessions")
                        .font(.system(size: 13))
                        .foregroundColor(.gray)
                }

                if let outURL = engine.outputFolderURL {
                    Button(action: { NSWorkspace.shared.open(outURL) }) {
                        Label("Open output folder", systemImage: "folder.fill.badge.plus")
                            .font(.system(size: 13, weight: .medium))
                            .foregroundColor(accentGrn)
                            .padding(.horizontal, 16)
                            .padding(.vertical, 8)
                            .background(accentGrn.opacity(0.15))
                            .cornerRadius(8)
                    }
                    .buttonStyle(.plain)
                }
            }
            .padding(40)
        }
        .frame(height: 200)
        .onDrop(of: [.folder, .fileURL], isTargeted: $isDragOver) { providers in
            handleDrop(providers: providers)
        }
        .onTapGesture {
            guard !engine.isRunning else { return }
            openFolderPanel()
        }
    }

    // ── Progress ──────────────────────────────────────────────────────────────
    var progressSection: some View {
        VStack(spacing: 6) {
            HStack {
                Text("Progress")
                    .font(.system(size: 12, weight: .medium))
                    .foregroundColor(.gray)
                Spacer()
                Text("\(Int(engine.progress * 100))%")
                    .font(.system(size: 12, weight: .medium, design: .monospaced))
                    .foregroundColor(engine.progress >= 1.0 ? accentGrn : accent)
            }
            GeometryReader { geo in
                ZStack(alignment: .leading) {
                    RoundedRectangle(cornerRadius: 4)
                        .fill(Color.white.opacity(0.08))
                        .frame(height: 6)
                    RoundedRectangle(cornerRadius: 4)
                        .fill(engine.progress >= 1.0 ? accentGrn : accent)
                        .frame(width: geo.size.width * engine.progress, height: 6)
                        .animation(.easeInOut(duration: 0.3), value: engine.progress)
                }
            }
            .frame(height: 6)
        }
    }

    // ── Log ───────────────────────────────────────────────────────────────────
    var logSection: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack {
                Text("Log")
                    .font(.system(size: 12, weight: .medium))
                    .foregroundColor(.gray)
                Spacer()
                if !engine.log.isEmpty {
                    Button("Clear") { engine.log = [] }
                        .font(.system(size: 11))
                        .foregroundColor(.gray)
                        .buttonStyle(.plain)
                }
            }
            .padding(.bottom, 8)

            ScrollView {
                LazyVStack(alignment: .leading, spacing: 3) {
                    if engine.log.isEmpty {
                        Text("Waiting for session…")
                            .font(.system(size: 12, design: .monospaced))
                            .foregroundColor(Color.white.opacity(0.2))
                            .padding(.vertical, 6)
                    } else {
                        ForEach(engine.log) { entry in
                            LogRow(entry: entry, accent: accent, accentGrn: accentGrn)
                        }
                    }
                }
                .padding(10)
            }
            .background(
                RoundedRectangle(cornerRadius: 10)
                    .fill(Color.black.opacity(0.35))
            )
            .frame(minHeight: 120, maxHeight: 180)
        }
    }

    // ── Footer ────────────────────────────────────────────────────────────────
    var footer: some View {
        HStack {
            Text("Supports Pro Tools 9–2024 (.ptx / .pts) · Outputs .novastudio + WAV")
                .font(.system(size: 10))
                .foregroundColor(Color.white.opacity(0.2))
            Spacer()
        }
        .padding(.horizontal, 24)
        .padding(.bottom, 12)
    }

    // ── Drop handler ──────────────────────────────────────────────────────────
    private func handleDrop(providers: [NSItemProvider]) -> Bool {
        guard !engine.isRunning else { return false }
        for provider in providers {
            if provider.hasItemConformingToTypeIdentifier(UTType.folder.identifier) {
                provider.loadItem(forTypeIdentifier: UTType.folder.identifier) { item, _ in
                    guard let url = item as? URL else { return }
                    Task { await engine.convert(sessionFolder: url) }
                }
                return true
            }
            if provider.hasItemConformingToTypeIdentifier(UTType.fileURL.identifier) {
                provider.loadItem(forTypeIdentifier: UTType.fileURL.identifier) { item, _ in
                    var url: URL?
                    if let u = item as? URL { url = u }
                    else if let data = item as? Data { url = URL(dataRepresentation: data, relativeTo: nil) }
                    guard let u = url else { return }
                    // If user dropped the .ptx file, use its parent folder
                    let folder = ["ptx","pts"].contains(u.pathExtension.lowercased())
                        ? u.deletingLastPathComponent()
                        : u
                    Task { await engine.convert(sessionFolder: folder) }
                }
                return true
            }
        }
        return false
    }

    private func openFolderPanel() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        panel.message = "Select your Pro Tools session folder"
        panel.prompt = "Convert"
        if panel.runModal() == .OK, let url = panel.url {
            Task { await engine.convert(sessionFolder: url) }
        }
    }
}

// ── Log row ───────────────────────────────────────────────────────────────────

struct LogRow: View {
    let entry: ConversionLog
    let accent: Color
    let accentGrn: Color

    var icon: String {
        switch entry.level {
        case .info:    return "●"
        case .warning: return "▲"
        case .success: return "✓"
        case .error:   return "✗"
        }
    }
    var color: Color {
        switch entry.level {
        case .info:    return Color.white.opacity(0.55)
        case .warning: return Color.yellow.opacity(0.85)
        case .success: return accentGrn
        case .error:   return Color.red.opacity(0.9)
        }
    }

    var body: some View {
        HStack(alignment: .top, spacing: 6) {
            Text(icon)
                .font(.system(size: 10, design: .monospaced))
                .foregroundColor(color)
                .frame(width: 12)
            Text(entry.message)
                .font(.system(size: 11, design: .monospaced))
                .foregroundColor(color)
                .lineLimit(3)
                .fixedSize(horizontal: false, vertical: true)
        }
    }
}
