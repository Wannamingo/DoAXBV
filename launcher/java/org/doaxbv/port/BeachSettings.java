package org.doaxbv.port;

/** Preference domains shared with the original game activity. */
final class BeachSettings {
    static boolean valid(String aspect,String language,int resolution,int touch){
        return ("source".equals(aspect)||"4:3".equals(aspect)||"16:9".equals(aspect))
            &&("ru".equals(language)||"en".equals(language))
            &&resolution>=0&&resolution<=1&&touch>=0&&touch<=2;
    }
}
