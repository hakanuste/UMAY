#if os(macOS)
import SwiftUI
import AppKit

/// AJANLAR PANOSUNU KENDİ PENCERESİNE AYIRIR (yalnız macOS).
///
/// NEDEN AppKit, SwiftUI'nin `openWindow`'u değil: `openWindow` ve
/// `WindowGroup(id:)` **macOS 13** ister. Bu uygulamanın hedefi
/// **macOS 12.0** (`arac/mac_uygulama.sh` ve `uygulama/project.yml`
/// ikisi de böyle diyor) ve kullanılan makine macOS 12.7. `NSWindow`
/// ile `NSHostingView` 10.15'ten beri vardır, yani hedefi
/// yükseltmeden çalışır. Hedefi yükseltmek, çalışan bir makineyi
/// dışarıda bırakmak olurdu.
///
/// NEDEN KENDİLİĞİNDEN AÇILMIYOR: pencereyi kullanıcı açar ve açık
/// bırakır. Ajanlar çalışmaya başlayınca kendiliğinden fırlayan bir
/// pencere, kullanıcı yazarken odağı çalar; kapandığında da kayıt
/// kaybolur. Burada pencere ikinci ekranda açık kalabilir ve sohbet
/// akışı hiç bölünmez.
///
/// iOS'ta bu dosya hiç derlenmez; orada aynı görünüm sağ pano olarak
/// kalır. Pencere arayüzü iOS'ta yoktur ve iki ayrı tasarım tutmak
/// yerine tek görünüm iki yerde gösteriliyor.
enum AjanPenceresi {
    private static var pencere: NSWindow?

    /// Pencereyi açar; açıksa öne getirir (ikinci bir kopya açmaz).
    static func ac(motor: UmayMotor) {
        if let p = pencere {
            p.makeKeyAndOrderFront(nil)
            return
        }
        let govde = AjanlarGorunumu(motor: motor, kapat: {}, penceredeMi: true)
        let p = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 460, height: 640),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false)
        p.title = "UMAY — Ajanlar"
        p.contentView = NSHostingView(rootView: govde)
        p.isReleasedWhenClosed = false      /* kapanınca nesne yaşar; yeniden açılabilir */
        p.center()
        p.setFrameAutosaveName("umay.ajanlar")   /* yeri ve boyu hatırlanır */
        p.makeKeyAndOrderFront(nil)
        pencere = p

        /* Kapatılınca imleç bırakılır, yoksa "açıksa öne getir" dalı
         * kapalı bir pencereyi öne getirmeye çalışır ve hiçbir şey
         * olmaz — kullanıcı düğmeye basar, pencere açılmaz. */
        NotificationCenter.default.addObserver(
            forName: NSWindow.willCloseNotification, object: p, queue: .main) { _ in
            pencere = nil
        }
    }
}
#endif
