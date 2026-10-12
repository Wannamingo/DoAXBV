package org.doaxbv.port;

import android.app.Activity;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.*;

/** Separate persistent layouts for the touch overlay and SDL gamepads. */
public final class ControlMappingActivity extends Activity {
    private SharedPreferences preferences;
    private boolean english,gamepad;
    private LinearLayout rows;
    private TextView hint,status;
    private Button touchTab,padTab;
    private int dp(float value){return Math.round(value*getResources().getDisplayMetrics().density);}
    private String text(String ru,String en){return english?en:ru;}
    private TextView label(String value,int size,int color){TextView v=new TextView(this);v.setText(value);v.setTextSize(size);v.setTextColor(color);return v;}
    private GradientDrawable background(int color,int border){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(10));d.setStroke(dp(1),border);return d;}
    private Button button(String value){Button b=new Button(this);b.setText(value);b.setAllCaps(false);b.setTextColor(0xffffddbd);b.setBackground(background(0xff42252d,0xff805146));return b;}
    private LinearLayout.LayoutParams weighted(){LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,dp(48),1);p.setMargins(dp(4),0,dp(4),0);return p;}
    @Override public void onCreate(Bundle saved){
        super.onCreate(saved);preferences=getSharedPreferences("controls",MODE_PRIVATE);
        english="en".equals(preferences.getString("language","ru"));gamepad=saved!=null&&saved.getBoolean("gamepad");
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);immersive();
        LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(0xff211018);root.setPadding(dp(24),dp(18),dp(24),dp(16));
        TextView title=label(text("Назначение кнопок","Button assignments"),25,Color.WHITE);title.setTypeface(null,Typeface.BOLD);root.addView(title);
        TextView subtitle=label(text("Выберите кнопку и действие Xbox для неё. Изменения сохраняются сразу.","Choose the Xbox action for each button. Changes are saved automatically."),13,0xffd0aba0);subtitle.setPadding(0,dp(8),0,dp(16));root.addView(subtitle);
        LinearLayout tabs=new LinearLayout(this);touchTab=button(text("Экранные кнопки","Touch controls"));padTab=button(text("Геймпад","Gamepad"));tabs.addView(touchTab,weighted());tabs.addView(padTab,weighted());root.addView(tabs);
        touchTab.setOnClickListener(v->select(false));padTab.setOnClickListener(v->select(true));
        hint=label("",12,0xffd0aba0);hint.setPadding(0,dp(12),0,dp(10));root.addView(hint);
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(false);rows=new LinearLayout(this);rows.setOrientation(LinearLayout.VERTICAL);scroll.addView(rows);root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        LinearLayout footer=new LinearLayout(this);footer.setGravity(Gravity.CENTER_VERTICAL);footer.setPadding(0,dp(12),0,0);
        Button reset=button(text("Сбросить раскладку","Reset layout"));footer.addView(reset,weighted());
        status=label(text("Сохранено","Saved"),12,0xffffd0ae);status.setGravity(Gravity.CENTER);footer.addView(status,new LinearLayout.LayoutParams(0,dp(48),1));
        Button done=button(text("Готово","Done"));done.setTextColor(0xff4b1d14);done.setBackground(background(0xffffc294,0xffffdbb3));footer.addView(done,weighted());root.addView(footer);
        reset.setOnClickListener(v->{boolean ok=ControlPreferences.reset(preferences,gamepad);saved(ok);if(ok)select(gamepad);});done.setOnClickListener(v->finish());
        setContentView(root);select(gamepad);
    }
    private void saved(boolean success){status.setText(success?text("Сохранено","Saved"):text("Ошибка сохранения","Save failed"));}
    private void select(boolean gamepad){
        this.gamepad=gamepad;touchTab.setSelected(!gamepad);padTab.setSelected(gamepad);
        touchTab.setBackground(background(gamepad?0xff42252d:0xff75422f,0xffbf805b));padTab.setBackground(background(gamepad?0xff75422f:0xff42252d,0xffbf805b));
        hint.setText(gamepad?text("A/B/X/Y — позиции кнопок по стандарту Xbox. Индивидуальные назначения имеют приоритет над перестановкой A/B и X/Y. Применяются при следующем запуске игры.","A/B/X/Y follow Xbox button positions. Individual assignments override A/B and X/Y swapping. Applied the next time the game starts."):text("Подписи экранных кнопок показывают назначенные действия. Кнопка ≡ всегда открывает меню игры. Применяются при следующем запуске игры.","Touch labels show assigned actions. The ≡ button always opens the game menu. Applied the next time the game starts."));
        if(getIntent().getBooleanExtra("inGame",false))hint.setText(hint.getText().toString().replace("Применяются при следующем запуске игры.","Применятся при возвращении в игру.").replace("Applied the next time the game starts.","Applies when you return to gameplay."));
        rows.removeAllViews();int[] values=ControlPreferences.load(preferences,gamepad);
        String[] names=english?new String[]{"D-pad ↑","D-pad ↓","D-pad ←","D-pad →","Start","Select","L3 · stick click","R3 · stick click","A · bottom","B · right","X · left","Y · top","L1","R1","L2","R2"}:new String[]{"Крестовина ↑","Крестовина ↓","Крестовина ←","Крестовина →","Start","Select","L3 · нажатие стика","R3 · нажатие стика","A · снизу","B · справа","X · слева","Y · сверху","L1","R1","L2","R2"};
        String[] actions=english?new String[]{"Disabled","D-pad ↑","D-pad ↓","D-pad ←","D-pad →","Start · pause","Back","Left stick click","Right stick click","A","B","X","Y","Black","White","Left trigger (LT)","Right trigger (RT)","App menu ≡"}:new String[]{"Не назначено","Крестовина ↑","Крестовина ↓","Крестовина ←","Крестовина →","Start · пауза","Back","Нажатие левого стика","Нажатие правого стика","A","B","X","Y","Black","White","Левый триггер (LT)","Правый триггер (RT)","Меню приложения ≡"};
        for(int i:new int[]{8,9,10,11,12,13,14,15,4,5,6,7,0,1,2,3}){
            final int slot=i,initial=values[i];final boolean padLayout=gamepad;
            LinearLayout row=new LinearLayout(this);row.setGravity(Gravity.CENTER_VERTICAL);row.setPadding(dp(12),dp(4),dp(8),dp(4));
            row.setBackgroundColor(i%2==0?0xff2c1820:0xff24121a);
            TextView name=label(names[i],15,0xfffff0e5);row.addView(name,new LinearLayout.LayoutParams(0,dp(52),1));name.setGravity(Gravity.CENTER_VERTICAL);
            Spinner action=new Spinner(this);action.setContentDescription(names[i]);
            ArrayAdapter<String> adapter=new ArrayAdapter<String>(this,android.R.layout.simple_spinner_item,actions);adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);action.setAdapter(adapter);action.setSelection(initial);
            row.addView(action,new LinearLayout.LayoutParams(0,dp(52),1));rows.addView(row);
            action.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){int previous=initial;public void onNothingSelected(AdapterView<?> parent){}public void onItemSelected(AdapterView<?> parent,View view,int position,long id){if(position==previous)return;if(ControlPreferences.save(preferences,padLayout,slot,position)){previous=position;saved(true);}else{saved(false);action.setSelection(previous);}}});
        }
    }
    private void immersive(){getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY|View.SYSTEM_UI_FLAG_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_FULLSCREEN);}
    @Override public boolean dispatchKeyEvent(KeyEvent event){
        if(event.getKeyCode()==KeyEvent.KEYCODE_BUTTON_B){if(event.getAction()==KeyEvent.ACTION_UP)onBackPressed();return true;}
        if(event.getKeyCode()==KeyEvent.KEYCODE_BUTTON_A){
            return super.dispatchKeyEvent(new KeyEvent(event.getDownTime(),event.getEventTime(),event.getAction(),KeyEvent.KEYCODE_DPAD_CENTER,event.getRepeatCount(),event.getMetaState(),event.getDeviceId(),event.getScanCode(),event.getFlags(),event.getSource()));
        }
        return super.dispatchKeyEvent(event);
    }
    @Override public void onWindowFocusChanged(boolean focused){super.onWindowFocusChanged(focused);if(focused)immersive();}
    @Override protected void onSaveInstanceState(Bundle saved){saved.putBoolean("gamepad",gamepad);super.onSaveInstanceState(saved);}
}
