package org.doaxbv.port;
import android.os.Bundle;
import android.view.*;
import android.graphics.Color;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.Intent;
import android.hardware.input.InputManager;
import java.io.File;
import java.util.*;
import org.libsdl.app.SDLActivity;

public class GameActivity extends SDLActivity {
    private TouchControlsView pad;
    private android.app.AlertDialog gameMenu;
    private int[] gamepadBindings;
    private boolean activityResumed,settingsOpen,editAfterSettings,english;
    private int touchMode,resolution;
    private static final int SETTINGS_REQUEST=731;
    private long editorStartFrame;
    private final Runnable editorAfterSettings=new Runnable(){public void run(){
        if(!activityResumed||settingsOpen||gameMenu!=null||pad==null)return;
        if(nativePresentedFrames()!=editorStartFrame)pad.beginEditing();
        else fpsHandler.postDelayed(this,16);
    }};
    private final Runnable menuUpdate=new Runnable(){public void run(){
        if(nativeConsumeMenuRequest())showGameMenu();
        fpsHandler.postDelayed(this,32);
    }};
    private InputManager inputManager;
    private android.widget.TextView fpsView;
    private final android.os.Handler fpsHandler=new android.os.Handler(android.os.Looper.getMainLooper());
    private final Runnable fpsUpdate=new Runnable(){public void run(){
        if(fpsView==null)return;float fps=nativeGetFps();
        fpsView.setText(fps>0 ? String.format(Locale.US,"%.0f FPS",fps) : "-- FPS");
        int failed=nativeConsumeDisplayFailure();
        if(failed!=0){resolution=(failed&3)-1;android.widget.Toast.makeText(GameActivity.this,english?"Could not change resolution. Previous resolution retained.":"Не удалось изменить разрешение. Сохранено прежнее разрешение.",android.widget.Toast.LENGTH_LONG).show();}
        fpsHandler.postDelayed(this,500);
    }};
    private final InputManager.InputDeviceListener deviceListener=new InputManager.InputDeviceListener(){
        public void onInputDeviceAdded(int id){refreshControls();}
        public void onInputDeviceRemoved(int id){refreshControls();}
        public void onInputDeviceChanged(int id){refreshControls();}
    };
    private static native void nativePad(int digital,int analog,float lx,float ly,float rx,float ry);
    private static native void nativePadReset();
    private static native void nativeSetGamepadBindings(int[] bindings);
    private static native boolean nativeConsumeMenuRequest();
    private static native void nativeRequestExit();
    private static native float nativeGetFps();
    private static native void nativeDisplaySettings(int scale,int aspect);
    private static native int nativeConsumeDisplayFailure();
    private static native long nativePresentedFrames();
    @Override protected String[] getLibraries(){return new String[]{"SDL3","main"};}
    @Override protected String[] getArguments(){SharedPreferences preferences=getSharedPreferences("controls",MODE_PRIVATE);ArrayList<String> args=new ArrayList<>(Arrays.asList("--xbe",new File(getFilesDir(),"disc/default.xbe").getAbsolutePath(),"--vsync"));
        args.add("--android-display="+LauncherActivity.savedAspect(preferences));
        args.add("--android-language="+("en".equals(preferences.getString("language","ru")) ? "en" : "ru"));
        args.add("--android-resolution="+(Math.max(0,Math.min(2,preferences.getInt("resolution",0)))+1));return args.toArray(new String[0]);}
    @Override protected void onCreate(Bundle saved){super.onCreate(saved);if(!mBrokenLibraries){pad=new TouchControlsView(this,new TouchControlsView.Listener(){
        public void input(int digital,int analog,float lx,float ly,float rx,float ry){nativePad(digital,analog,lx,ly,rx,ry);}
        public void reset(){nativePadReset();}
        public void menu(){showGameMenu();}
        public void settings(){openSettings();}
        public void editing(boolean active){
            if(active)pauseNativeThread();else resumeIfReady();
            if(fpsView!=null)fpsView.setVisibility(active?View.GONE:View.VISIBLE);
        }
    });SharedPreferences controls=getSharedPreferences("controls",MODE_PRIVATE);
        english="en".equals(controls.getString("language","ru"));touchMode=controls.getInt("touchControls",0);resolution=controls.getInt("resolution",0);
        gamepadBindings=ControlPreferences.load(controls,true);nativeSetGamepadBindings(gamepadBindings);pad.setBindings(ControlPreferences.load(controls,false));
        mLayout.addView(pad,new android.widget.RelativeLayout.LayoutParams(-1,-1));getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);inputManager=(InputManager)getSystemService(Context.INPUT_SERVICE);if(inputManager!=null)inputManager.registerInputDeviceListener(deviceListener,null);request60Hz();refreshControls();int margin=(int)(6*getResources().getDisplayMetrics().density);
        fpsView=new android.widget.TextView(this);fpsView.setTextColor(Color.WHITE);fpsView.setTextSize(12);fpsView.setShadowLayer(2,1,1,Color.BLACK);fpsView.setPadding(margin,margin,margin,margin);fpsView.setBackgroundColor(0x66000000);fpsView.setFocusable(false);fpsView.setText("-- FPS");
        android.widget.RelativeLayout.LayoutParams fpsParams=new android.widget.RelativeLayout.LayoutParams(-2,-2);fpsParams.addRule(android.widget.RelativeLayout.ALIGN_PARENT_TOP);fpsParams.addRule(android.widget.RelativeLayout.CENTER_HORIZONTAL);fpsParams.setMargins(margin,margin,margin,margin);mLayout.addView(fpsView,fpsParams);fpsHandler.post(fpsUpdate);}}
    @Override protected void onResume(){super.onResume();activityResumed=true;refreshControls();if(editAfterSettings&&pad!=null){editAfterSettings=false;editorStartFrame=nativePresentedFrames();fpsHandler.postDelayed(editorAfterSettings,16);}if(!mBrokenLibraries){fpsHandler.removeCallbacks(menuUpdate);fpsHandler.post(menuUpdate);}}
    @Override protected void onDestroy(){fpsHandler.removeCallbacks(menuUpdate);fpsHandler.removeCallbacks(fpsUpdate);fpsView=null;if(inputManager!=null)inputManager.unregisterInputDeviceListener(deviceListener);boolean terminate=isFinishing();super.onDestroy();if(terminate)android.os.Process.killProcess(android.os.Process.myPid());}
    private void request60Hz(){Display display=getWindowManager().getDefaultDisplay();Display.Mode current=display.getMode(),best=null;float distance=2f;for(Display.Mode mode:display.getSupportedModes()){float delta=Math.abs(mode.getRefreshRate()-60f);if(mode.getPhysicalWidth()==current.getPhysicalWidth()&&mode.getPhysicalHeight()==current.getPhysicalHeight()&&delta<distance){best=mode;distance=delta;}}WindowManager.LayoutParams params=getWindow().getAttributes();params.preferredRefreshRate=60f;if(best!=null)params.preferredDisplayModeId=best.getModeId();getWindow().setAttributes(params);}
    private void refreshControls(){if(pad==null)return;boolean connected=false;for(int id:InputDevice.getDeviceIds()){InputDevice device=InputDevice.getDevice(id);if(device!=null&&(((device.getSources()&InputDevice.SOURCE_GAMEPAD)==InputDevice.SOURCE_GAMEPAD)||((device.getSources()&InputDevice.SOURCE_JOYSTICK)==InputDevice.SOURCE_JOYSTICK))){connected=true;break;}}boolean visible=touchMode==1||(touchMode==0&&!connected);pad.setControlsVisible(visible);}
    @Override protected void onPause(){activityResumed=false;fpsHandler.removeCallbacks(menuUpdate);fpsHandler.removeCallbacks(editorAfterSettings);if(pad!=null){pad.cancelEditing();pad.clear();}super.onPause();}
    @Override public boolean dispatchKeyEvent(KeyEvent event){
        if(pad!=null&&pad.isEditing()){
            int key=event.getKeyCode();
            if(key==KeyEvent.KEYCODE_BACK||key==KeyEvent.KEYCODE_BUTTON_SELECT){if(event.getAction()==KeyEvent.ACTION_DOWN&&event.getRepeatCount()==0)pad.cancelEditing();return true;}
            if(key==KeyEvent.KEYCODE_BUTTON_START){if(event.getAction()==KeyEvent.ACTION_DOWN&&event.getRepeatCount()==0)pad.saveEditing();return true;}
            if(physicalSlot(key)>=0)return true;
        }
        if(event.getKeyCode()==KeyEvent.KEYCODE_BACK){
            if(event.getAction()==KeyEvent.ACTION_DOWN&&event.getRepeatCount()==0)showGameMenu();
            return true;
        }
        int slot=physicalSlot(event.getKeyCode());
        // Consume menu keys before SDL sees a press. Dialogs own their release
        // event, so forwarding the down event could leave SDL holding the key.
        if(slot>=0&&gamepadBindings!=null&&gamepadBindings[slot]==ControlBindings.MENU){
            if(event.getAction()==KeyEvent.ACTION_DOWN&&event.getRepeatCount()==0)showGameMenu();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }
    private static int physicalSlot(int key){switch(key){
        case KeyEvent.KEYCODE_DPAD_UP:return 0;case KeyEvent.KEYCODE_DPAD_DOWN:return 1;
        case KeyEvent.KEYCODE_DPAD_LEFT:return 2;case KeyEvent.KEYCODE_DPAD_RIGHT:return 3;
        case KeyEvent.KEYCODE_BUTTON_START:return 4;case KeyEvent.KEYCODE_BUTTON_SELECT:return 5;
        case KeyEvent.KEYCODE_BUTTON_THUMBL:return 6;case KeyEvent.KEYCODE_BUTTON_THUMBR:return 7;
        case KeyEvent.KEYCODE_BUTTON_A:return 8;case KeyEvent.KEYCODE_BUTTON_B:return 9;
        case KeyEvent.KEYCODE_BUTTON_X:return 10;case KeyEvent.KEYCODE_BUTTON_Y:return 11;
        case KeyEvent.KEYCODE_BUTTON_L1:return 12;case KeyEvent.KEYCODE_BUTTON_R1:return 13;
        case KeyEvent.KEYCODE_BUTTON_L2:return 14;case KeyEvent.KEYCODE_BUTTON_R2:return 15;
        default:return -1;
    }}
    @Override public void onBackPressed(){if(pad!=null&&pad.isEditing())pad.cancelEditing();else showGameMenu();}
    private void resumeIfReady(){if(activityResumed&&!settingsOpen&&gameMenu==null&&(pad==null||!pad.isEditing()))resumeNativeThread();}
    private void openSettings(){
        if(isFinishing()||mBrokenLibraries||settingsOpen)return;
        if(pad!=null){pad.cancelEditing();pad.clear();}settingsOpen=true;pauseNativeThread();
        try{startActivityForResult(new Intent(this,GameSettingsActivity.class).putExtra("activeResolution",resolution),SETTINGS_REQUEST);}
        catch(RuntimeException unavailable){settingsOpen=false;resumeIfReady();android.widget.Toast.makeText(this,english?"Could not open settings":"Не удалось открыть настройки",android.widget.Toast.LENGTH_SHORT).show();}
    }
    @Override protected void onActivityResult(int request,int result,Intent data){
        super.onActivityResult(request,result,data);if(request!=SETTINGS_REQUEST)return;settingsOpen=false;
        if(result==RESULT_OK&&data!=null&&pad!=null){
            resolution=Math.max(0,Math.min(2,data.getIntExtra("resolution",resolution)));
            String aspect=data.getStringExtra("aspect");nativeDisplaySettings(resolution+1,"16:9".equals(aspect)?2:"4:3".equals(aspect)?1:0);
            english="en".equals(data.getStringExtra("language"));pad.setLanguage(english);
            touchMode=Math.max(0,Math.min(2,data.getIntExtra("touchControls",touchMode)));
            gamepadBindings=ControlBindings.sanitize(data.getIntArrayExtra("padBindings"));nativeSetGamepadBindings(gamepadBindings);
            pad.setBindings(ControlBindings.sanitize(data.getIntArrayExtra("touchBindings")));refreshControls();
            editAfterSettings=data.getBooleanExtra("editLayout",false);
        }
    }
    // The guest runner exits its dedicated :game process after native cleanup.
    // Calling finish here starts SDLActivity.nativeQuit concurrently with it.
    private void showGameMenu(){
        if(isFinishing()||mBrokenLibraries||gameMenu!=null||settingsOpen)return;
        if(pad!=null){pad.cancelEditing();pad.clear();}pauseNativeThread();
        gameMenu=new android.app.AlertDialog.Builder(this).setTitle(english?"Game":"Игра")
            .setItems(english?new String[]{"Resume game","Settings","Touch control positions","Return to app menu"}:new String[]{"Продолжить игру","Настройки","Расположение экранных элементов","Выйти в меню приложения"},(dialog,which)->{
                if(which==1)openSettings();
                else if(which==2&&pad!=null)pad.beginEditing();
                else if(which==3){if(pad!=null)pad.clear();nativeRequestExit();}
            }).create();
        gameMenu.setOnDismissListener(dialog->{gameMenu=null;resumeIfReady();});gameMenu.show();
    }
}
