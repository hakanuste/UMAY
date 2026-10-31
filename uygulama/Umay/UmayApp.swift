import SwiftUI
#if os(macOS)
import AppKit
#endif

@main
struct UmayApp: App {
    init() {
        #if os(macOS)
        // Dock simgesi: paketteki Umay.icns (Info.plist de aynısını gösterir; bu, paket dışı çalıştırmada da simgeyi koyar).
        if let u = Bundle.main.url(forResource: "Umay", withExtension: "icns"), let g = NSImage(contentsOf: u) {
            NSApplication.shared.applicationIconImage = g
        }
        #endif
    }

    var body: some Scene {
        WindowGroup {
            AnaGorunum()
        }
    }
}
