import SwiftUI

@main
struct PTConverterApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
                .preferredColorScheme(.dark)
        }
        .windowStyle(.hiddenTitleBar)
        .windowResizability(.contentMinSize)
        .defaultSize(width: 640, height: 560)
        .commands {
            CommandGroup(replacing: .newItem) {}
        }
    }
}
