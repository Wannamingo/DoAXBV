package org.doaxbv.port;

import android.content.SharedPreferences;
import java.util.LinkedHashMap;
import java.util.Map;

/** Separate preference file: only the game process writes layout positions. */
final class TouchLayoutStore {
    private static final String[] IDS={"l1","l2","l3","r3","r2","r1","dpad","left-stick","right-stick","a","b","x","y","select","menu","start"};
    static Map<String,float[]> load(SharedPreferences preferences){
        Map<String,float[]> values=new LinkedHashMap<>();
        for(String id:IDS)try{
            float x=preferences.getFloat(id+"_x",-1),y=preferences.getFloat(id+"_y",-1);
            if(Float.isFinite(x)&&Float.isFinite(y)&&x>=0&&x<=1&&y>=0&&y<=1)values.put(id,new float[]{x,y});
        }catch(ClassCastException ignored){}
        return values;
    }
    static boolean save(SharedPreferences preferences,Map<String,float[]> values){
        SharedPreferences.Editor edit=preferences.edit().clear();
        for(String id:IDS){float[] p=values.get(id);if(p!=null&&p.length==2&&Float.isFinite(p[0])&&Float.isFinite(p[1])&&p[0]>=0&&p[0]<=1&&p[1]>=0&&p[1]<=1)edit.putFloat(id+"_x",p[0]).putFloat(id+"_y",p[1]);}
        return edit.commit();
    }
}
