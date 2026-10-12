package org.doaxbv.port;

import android.content.SharedPreferences;
import java.lang.reflect.Proxy;
import java.util.HashMap;
import java.util.Map;

/** Exercises the real persistence code against the SharedPreferences interface. */
public final class ControlPreferencesTest {
    private static void check(boolean value,String message){if(!value)throw new AssertionError(message);}
    public static void main(String[] args){
        Map<String,Object> values=new HashMap<>();Map<String,Object> pending=new HashMap<>();
        SharedPreferences.Editor edit=(SharedPreferences.Editor)Proxy.newProxyInstance(ControlPreferencesTest.class.getClassLoader(),new Class[]{SharedPreferences.Editor.class},(proxy,method,a)->{
            switch(method.getName()){
                case "putInt":case "putFloat":pending.put((String)a[0],a[1]);return proxy;
                case "clear":values.clear();pending.clear();return proxy;
                case "remove":pending.put((String)a[0],null);return proxy;
                case "commit":for(Map.Entry<String,Object> e:pending.entrySet())if(e.getValue()==null)values.remove(e.getKey());else values.put(e.getKey(),e.getValue());pending.clear();return true;
                default:throw new AssertionError(method);
            }
        });
        SharedPreferences prefs=(SharedPreferences)Proxy.newProxyInstance(ControlPreferencesTest.class.getClassLoader(),new Class[]{SharedPreferences.class},(proxy,method,a)->{
            switch(method.getName()){
                case "edit":return edit;
                case "getInt":Object v=values.getOrDefault(a[0],a[1]);if(!(v instanceof Integer))throw new ClassCastException();return v;
                case "getFloat":Object f=values.getOrDefault(a[0],a[1]);if(!(f instanceof Float))throw new ClassCastException();return f;
                case "getBoolean":return values.getOrDefault(a[0],a[1]);
                default:throw new AssertionError(method);
            }
        });
        values.put("resolution",2);values.put("swapButtons",true);
        check(ControlPreferences.load(prefs,true)[8]==10,"legacy swap migrated as default");
        check(ControlPreferences.load(prefs,false)[8]==9,"touch unaffected by physical swap");
        check(ControlPreferences.save(prefs,true,8,12),"save pad");check(ControlPreferences.save(prefs,false,8,10),"save touch");
        check(ControlPreferences.load(prefs,true)[8]==12&&ControlPreferences.load(prefs,false)[8]==10,"separate persistent layouts and custom override");
        values.put(ControlPreferences.key(true,9),99);values.put(ControlPreferences.key(true,10),"invalid");
        check(ControlPreferences.load(prefs,true)[9]==9&&ControlPreferences.load(prefs,true)[10]==12,"invalid values fall back safely");
        check(!ControlPreferences.save(prefs,false,-1,0)&&!ControlPreferences.save(prefs,false,0,18),"invalid assignments rejected");
        ControlPreferences.reset(prefs,true);check(ControlPreferences.load(prefs,true)[8]==10&&ControlPreferences.load(prefs,false)[8]==10,"reset only selected layout");
        check(values.get("resolution").equals(2)&&values.get("swapButtons").equals(true),"unrelated preferences retained");
        values.clear();Map<String,float[]> positions=new HashMap<>();positions.put("a",new float[]{.5f,.6f});positions.put("b",new float[]{Float.NaN,.2f});
        check(TouchLayoutStore.save(prefs,positions),"save layout positions");
        check(TouchLayoutStore.load(prefs).get("a")[1]==.6f&&!TouchLayoutStore.load(prefs).containsKey("b"),"layout persistence and invalid positions");
        values.put("b_x","invalid");values.put("b_y",.5f);check(!TouchLayoutStore.load(prefs).containsKey("b"),"invalid preference types rejected");
        TouchLayoutStore.save(prefs,new HashMap<>());check(TouchLayoutStore.load(prefs).isEmpty(),"persist layout reset");
        System.out.println("Preferences passed: persistence, layout isolation, swap compatibility, validation, reset and retained settings.");
    }
}
