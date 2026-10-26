package tr.umay;

/** UMAY'a Java/Kotlin kapisi. Tek iplikten cagirin. */
public final class Umay {
    static { System.loadLibrary("umay"); }
    private Umay() { }

    /** veriDizini: umay.us1 ve alan klasorlerini iceren, YAZILABILIR dizin. 0 = oldu. */
    public static native int baslat(String veriDizini);
    /** Yanit metnini dondurur; hata/bos soruda null. */
    public static native String sor(String soru);
    public static native void kapat();
    public static native String surum();

    /** Agi uygulama getirir (HttpURLConnection vb.). Govdeyi dondurun; hata/ag yoksa null. */
    public interface AgGetirici { String getir(String adres); }
    /** Web arastirmasini acar. null = kapatir (sahte basari yoktur). */
    public static native void agGetiriciAta(AgGetirici g);
    /** konu: arastirir ve kutuphaneye ekler. null: bekleyen bosluklari isler.
     *  Eklenen parca sayisi; negatif = ag yok/kapali. Bloke eder. */
    public static native int ogren(String konu);
}
