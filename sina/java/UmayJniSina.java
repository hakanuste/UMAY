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

        /* web arastirmasi: ag Java'dan gelir (burada sahte) */
        dene("getirici yokken ogren kapali (negatif)", Umay.ogren("zeolit toprak") < 0);
        dene("getirici yokken konu bilinmiyor", !(Umay.sor("zeolit nedir") + "").contains("mineral"));
        final int[] cagri = {0};
        Umay.agGetiriciAta(new Umay.AgGetirici() {
            public String getir(String adres) {
                cagri[0]++;
                if (adres.contains("gsrsearch=zeolit"))
                    return "{\"query\":{\"pages\":[{\"title\":\"Zeolit\",\"extract\":"
                         + "\"Zeolit g\\u00f6zenekli bir mineraldir. Tar\\u0131mda toprak d\\u00fczenleyici olarak kullan\\u0131l\\u0131r.\"}]}}";
                return null;
            }
        });
        int e = Umay.ogren("zeolit toprak");
        dene("ogren parca ekledi", e >= 1);
        dene("Java getiricisi cagrildi", cagri[0] >= 1);
        String z = Umay.sor("zeolit nedir");
        dene("ogrenilen bilgi yanitta, Turkce karakterler dogru", z != null && z.contains("mineral") && z.contains("Tarımda"));
        dene("kaynak 'dogrulanmamis' diye anilir", z != null && z.contains("doğrulanmamış"));
        Umay.agGetiriciAta(null);
        dene("getirici kalkinca ogren yine kapali", Umay.ogren("baska konu") < 0);
        Umay.kapat();
        System.out.println("=== JNI SONUC: " + gecen + " gecti, " + kalan + " kaldi ===");
        System.exit(kalan == 0 ? 0 : 1);
    }
}
