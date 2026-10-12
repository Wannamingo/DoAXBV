package org.doaxbv.port;

import android.content.SharedPreferences;

final class ControlPreferences {
    static String key(boolean gamepad,int slot){return "binding_"+(gamepad?"pad_":"touch_")+ControlBindings.SLOTS[slot];}
    static int[] load(SharedPreferences preferences,boolean gamepad){
        int[] values=ControlBindings.defaults(gamepad&&preferences.getBoolean("swapButtons",false));
        for(int i=0;i<values.length;i++){
            try{int value=preferences.getInt(key(gamepad,i),values[i]);if(ControlBindings.valid(value))values[i]=value;}
            catch(ClassCastException ignored){}
        }
        return values;
    }
    static boolean save(SharedPreferences preferences,boolean gamepad,int slot,int action){
        if(slot<0||slot>=ControlBindings.SLOTS.length||!ControlBindings.valid(action))return false;
        return preferences.edit().putInt(key(gamepad,slot),action).commit();
    }
    static boolean reset(SharedPreferences preferences,boolean gamepad){
        SharedPreferences.Editor edit=preferences.edit();
        for(int i=0;i<ControlBindings.SLOTS.length;i++)edit.remove(key(gamepad,i));
        return edit.commit();
    }
}
