import SwiftUI

/// Dil modeli indirme ve ayarlar. Dosya adları Hugging Face'te doğrulandı (Qwen/Qwen3-*-GGUF).
struct ModelSecenegi: Identifiable {
    let id: String          // dosya adı
    let ad: String
    let boyut: String
    let adres: String
    static var liste: [ModelSecenegi] {
        var l = [
            ModelSecenegi(id: "Qwen3-0.6B-Q8_0.gguf", ad: "Küçük (hızlı)", boyut: "0,6 GB",
                          adres: "https://huggingface.co/Qwen/Qwen3-0.6B-GGUF/resolve/main/Qwen3-0.6B-Q8_0.gguf"),
            ModelSecenegi(id: "Qwen3-1.7B-Q8_0.gguf", ad: "Orta (önerilen)", boyut: "1,8 GB",
                          adres: "https://huggingface.co/Qwen/Qwen3-1.7B-GGUF/resolve/main/Qwen3-1.7B-Q8_0.gguf"),
        ]
        #if os(macOS)
        // 4B telefonun bellek sınırını aşabilir; yalnız bilgisayarda sunulur.
        l.append(ModelSecenegi(id: "Qwen3-4B-Q4_K_M.gguf", ad: "Büyük (yavaş, iyi)", boyut: "2,5 GB",
                               adres: "https://huggingface.co/Qwen/Qwen3-4B-GGUF/resolve/main/Qwen3-4B-Q4_K_M.gguf"))
        #endif
        return l
    }
}

final class Indirici: NSObject, ObservableObject, URLSessionDownloadDelegate {
    @Published var ilerleme: [String: Double] = [:]
    @Published var hata = ""
    private var oturum: URLSession!
    private var bekleyen: [Int: ModelSecenegi] = [:]

    override init() {
        super.init()
        oturum = URLSession(configuration: .default, delegate: self, delegateQueue: .main)
    }
    func indir(_ m: ModelSecenegi) {
        guard let u = URL(string: m.adres) else { return }
        hata = ""
        ilerleme[m.id] = 0
        let g = oturum.downloadTask(with: u)
        bekleyen[g.taskIdentifier] = m
        g.resume()
    }
    func urlSession(_ s: URLSession, downloadTask g: URLSessionDownloadTask, didWriteData _: Int64,
                    totalBytesWritten yazilan: Int64, totalBytesExpectedToWrite toplam: Int64) {
        guard let m = bekleyen[g.taskIdentifier], toplam > 0 else { return }
        ilerleme[m.id] = Double(yazilan) / Double(toplam)
    }
    func urlSession(_ s: URLSession, downloadTask g: URLSessionDownloadTask, didFinishDownloadingTo yer: URL) {
        guard let m = bekleyen[g.taskIdentifier] else { return }
        // Yalnız gerçek bir GGUF dosyası model sayılır (başlığı "GGUF").
        let baslik = (try? FileHandle(forReadingFrom: yer))?.readData(ofLength: 4)
        guard baslik == Data("GGUF".utf8) else { hata = "İnen dosya GGUF değil; model kaydedilmedi."; ilerleme[m.id] = nil; return }
        let hedef = UmayMotor.modelDizini.appendingPathComponent(m.id)
        try? FileManager.default.removeItem(at: hedef)
        do { try FileManager.default.moveItem(at: yer, to: hedef); ilerleme[m.id] = 1 }
        catch { hata = "Model kaydedilemedi: \(error.localizedDescription)"; ilerleme[m.id] = nil }
    }
    func urlSession(_ s: URLSession, task g: URLSessionTask, didCompleteWithError e: Error?) {
        if let e = e, let m = bekleyen[g.taskIdentifier] { hata = "İndirme başarısız: \(e.localizedDescription)"; ilerleme[m.id] = nil }
    }
}

struct AyarGorunumu: View {
    @ObservedObject var motor: UmayMotor
    @StateObject private var indirici = Indirici()
    @AppStorage("umay.model") private var secili = ""
    @AppStorage("umay.ogren") private var internettenOgren = false
    @AppStorage("umay.sesli") private var seslidenYanitla = true
    @Environment(\.presentationMode) private var kapat

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack { Text("Ayarlar").font(.title2).bold(); Spacer(); Button("Kapat") { kapat.wrappedValue.dismiss() } }
            Text("Dil modeli").font(.headline)
            ForEach(ModelSecenegi.liste) { m in
                HStack {
                    VStack(alignment: .leading) {
                        Text("\(m.ad) — \(m.boyut)")
                        Text(m.id).font(.caption).foregroundColor(.secondary)
                    }
                    Spacer()
                    if let p = indirici.ilerleme[m.id], p < 1 {
                        ProgressView(value: p).frame(width: 90)
                    } else if indiMi(m) {
                        Button(secili == m.id ? "Seçili" : "Seç") { secili = m.id }
                            .disabled(secili == m.id)
                    } else {
                        Button("İndir") { indirici.indir(m) }
                    }
                }
            }
            if !indirici.hata.isEmpty { Text(indirici.hata).foregroundColor(.red).font(.caption) }
            Toggle("Yanıtları sesli de oku", isOn: $seslidenYanitla)
            Toggle("İnternetten kendiliğinden öğren", isOn: $internettenOgren)
            Text("Model değişikliği ve öğrenme ayarı, uygulama kapatılıp yeniden açılınca işler. İlk model indirilirken uygulamayı açık tutun.")
                .font(.caption).foregroundColor(.secondary)
            Spacer()
        }
        .padding()
        #if os(macOS)
        .frame(width: 460, height: 420)
        #endif
    }

    private func indiMi(_ m: ModelSecenegi) -> Bool {
        FileManager.default.fileExists(atPath: UmayMotor.modelDizini.appendingPathComponent(m.id).path)
    }
}
