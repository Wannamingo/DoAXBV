package org.doaxbv.port;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.*;
import android.widget.*;

/** Settings stay in the launcher process, the sole writer of controls preferences. */
public final class GameSettingsActivity extends Activity {
    private SharedPreferences preferences;
    private boolean english,building;
    private TextView status;
    private int dp(float v){return Math.round(v*getResources().getDisplayMetrics().density);}
    private String text(String ru,String en){return english?en:ru;}
    private GradientDrawable background(int color){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(10));d.setStroke(dp(1),0xff805146);return d;}
    private TextView label(String s,int size,int color){TextView v=new TextView(this);v.setText(s);v.setTextSize(size);v.setTextColor(color);return v;}
    private Button button(String s){Button b=new Button(this);b.setText(s);b.setAllCaps(false);b.setTextColor(0xffffddbd);b.setBackground(background(0xff42252d));return b;}
    @Override public void onCreate(Bundle saved){
        super.onCreate(saved);preferences=getSharedPreferences("controls",MODE_PRIVATE);
        // A failed GPU allocation may have retained the previous active scale.
        int active=getIntent().getIntExtra("activeResolution",-1);
        if(saved==null&&active>=0&&active<=2)preferences.edit().putInt("resolution",active).commit();
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);build();
    }
    private void build(){
        building=true;english="en".equals(preferences.getString("language","ru"));
        LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setPadding(dp(24),dp(18),dp(24),dp(16));root.setBackgroundColor(0xff211018);
        TextView title=label(text("Настройки во время игры","In-game settings"),25,Color.WHITE);title.setTypeface(null,Typeface.BOLD);root.addView(title);
        TextView hint=label(text("Игра на паузе. Изменения применятся при возвращении в игру.","Game paused. Changes apply when you return to gameplay."),13,0xffd0aba0);hint.setPadding(0,dp(10),0,dp(14));root.addView(hint);
        ScrollView scroll=new ScrollView(this);LinearLayout rows=new LinearLayout(this);rows.setOrientation(LinearLayout.VERTICAL);scroll.addView(rows);root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        choice(rows,text("Внутреннее разрешение","Internal resolution"),new String[]{"1× · 720 × 480","2× · 1440 × 960","3× · 2160 × 1440"},preferences.getInt("resolution",0),position->save(preferences.edit().putInt("resolution",position)));
        String aspect=LauncherActivity.savedAspect(preferences);
        choice(rows,text("Формат изображения","Aspect ratio"),new String[]{text("Исходные пропорции","Original proportions"),"4:3","16:9"},"16:9".equals(aspect)?2:"4:3".equals(aspect)?1:0,position->save(preferences.edit().putString("displayAspect",new String[]{"source","4:3","16:9"}[position])));
        choice(rows,text("Экранные кнопки","Touch controls"),new String[]{text("Авто — скрывать с геймпадом","Auto — hide with gamepad"),text("Показывать","Show"),text("Скрывать","Hide")},preferences.getInt("touchControls",0),position->save(preferences.edit().putInt("touchControls",position)));
        CheckBox swap=new CheckBox(this);swap.setText(text("Поменять A/B и X/Y на геймпаде","Swap gamepad A/B and X/Y"));swap.setTextColor(0xfffff0e5);swap.setChecked(preferences.getBoolean("swapButtons",false));rows.addView(swap,new LinearLayout.LayoutParams(-1,dp(56)));swap.setOnCheckedChangeListener((v,on)->save(preferences.edit().putBoolean("swapButtons",on)));
        choice(rows,text("Язык приложения","App language"),new String[]{"Русский","English"},english?1:0,position->{if(save(preferences.edit().putString("language",position==1?"en":"ru")))build();});
        addButton(rows,text("Назначение кнопок экрана и геймпада","Touch and gamepad button assignments"),()->startActivity(new Intent(this,ControlMappingActivity.class).putExtra("inGame",true)));
        addButton(rows,text("Расположение экранных элементов","Touch control positions"),()->returnToGame(true));
        LinearLayout footer=new LinearLayout(this);footer.setGravity(Gravity.CENTER_VERTICAL);footer.setPadding(0,dp(12),0,0);
        status=label(text("Настройки сохраняются автоматически","Settings save automatically"),12,0xffd0aba0);footer.addView(status,new LinearLayout.LayoutParams(0,dp(48),1));
        Button done=button(text("Продолжить игру","Resume game"));done.setTextColor(0xff4b1d14);done.setBackground(background(0xffffc294));footer.addView(done,new LinearLayout.LayoutParams(dp(200),dp(48)));done.setOnClickListener(v->returnToGame(false));root.addView(footer);
        setContentView(root);immersive();building=false;
    }
    private interface Selection {void changed(int position);}
    private void choice(LinearLayout rows,String name,String[] values,int initial,Selection listener){
        LinearLayout row=new LinearLayout(this);row.setGravity(Gravity.CENTER_VERTICAL);row.setPadding(dp(12),dp(4),dp(8),dp(4));row.setBackgroundColor(0xff2c1820);
        TextView label=label(name,15,0xfffff0e5);row.addView(label,new LinearLayout.LayoutParams(0,dp(56),1));label.setGravity(Gravity.CENTER_VERTICAL);
        Spinner select=new Spinner(this);select.setContentDescription(name);ArrayAdapter<String> adapter=new ArrayAdapter<>(this,android.R.layout.simple_spinner_item,values);adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);select.setAdapter(adapter);select.setSelection(Math.max(0,Math.min(values.length-1,initial)));row.addView(select,new LinearLayout.LayoutParams(0,dp(56),1));rows.addView(row);
        select.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){int previous=initial;public void onNothingSelected(AdapterView<?> p){}public void onItemSelected(AdapterView<?> p,View v,int position,long id){if(building||position==previous)return;previous=position;listener.changed(position);}});
    }
    private void addButton(LinearLayout rows,String name,Runnable action){Button b=button(name);LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,dp(52));p.topMargin=dp(10);rows.addView(b,p);b.setOnClickListener(v->action.run());}
    private boolean save(SharedPreferences.Editor edit){boolean ok=edit.commit();if(status!=null)status.setText(ok?text("Сохранено · Применится при возвращении","Saved · Applies on return"):text("Ошибка сохранения","Save failed"));return ok;}
    private void returnToGame(boolean editLayout){
        Intent result=new Intent().putExtra("resolution",preferences.getInt("resolution",0)).putExtra("aspect",LauncherActivity.savedAspect(preferences)).putExtra("language",preferences.getString("language","ru")).putExtra("touchControls",preferences.getInt("touchControls",0)).putExtra("touchBindings",ControlPreferences.load(preferences,false)).putExtra("padBindings",ControlPreferences.load(preferences,true)).putExtra("editLayout",editLayout);
        setResult(RESULT_OK,result);finish();
    }
    @Override public void onBackPressed(){returnToGame(false);}
    private void immersive(){getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY|View.SYSTEM_UI_FLAG_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_FULLSCREEN);}
    @Override public void onWindowFocusChanged(boolean focus){super.onWindowFocusChanged(focus);if(focus)immersive();}
    @Override public boolean dispatchKeyEvent(KeyEvent event){
        if(event.getKeyCode()==KeyEvent.KEYCODE_BUTTON_B){if(event.getAction()==KeyEvent.ACTION_UP)onBackPressed();return true;}
        if(event.getKeyCode()==KeyEvent.KEYCODE_BUTTON_A)return super.dispatchKeyEvent(new KeyEvent(event.getDownTime(),event.getEventTime(),event.getAction(),KeyEvent.KEYCODE_DPAD_CENTER,event.getRepeatCount(),event.getMetaState(),event.getDeviceId(),event.getScanCode(),event.getFlags(),event.getSource()));
        return super.dispatchKeyEvent(event);
    }
}
