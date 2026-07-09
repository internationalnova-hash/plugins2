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
        .defaultSize(width: 720, height: 700)
        .commands {
            CommandGroup(replacing: .newItem) {}
        }
    }
}
