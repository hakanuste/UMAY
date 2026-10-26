import tr.umay.Umay;

public class UmayJniSina {
    static int gecen, kalan;
    static void dene(String ad, boolean ok) {
        System.out.println("  [" + (ok ? "GECTI" : "KALDI") + "] " + ad);
        if (ok) gecen++; else kalan++;
    }
    public static void main(String[] a) {
        dene("surum", "0.1.0".equals(Umay.surum()));
        dene("baslatmadan sor null", Umay.sor("merhaba") == null);
        dene("baslat", Umay.baslat(a.length > 0 ? a[0] : null) == 0);
        String y = Umay.sor("güneş paneli nedir");
        dene("yanit geldi", y != null && !y.isEmpty());
        dene("Turkce karakterler Java String'ine dogru gecti", y != null && y.contains("güneş"));
        dene("dayanak gosterildi", y != null && y.contains("Dayan"));
        dene("bos soru null", Umay.sor("") == null);
        Umay.kapat();
        System.out.println("=== JNI SONUC: " + gecen + " gecti, " + kalan + " kaldi ===");
        System.exit(kalan == 0 ? 0 : 1);
    }
}
