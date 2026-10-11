package org.doaxbv.port;
import android.os.Bundle;
import android.view.*;
import android.graphics.*;
import android.content.Context;
import android.content.SharedPreferences;
import android.hardware.input.InputManager;
import java.io.File;
import java.util.*;
import org.libsdl.app.SDLActivity;

public class GameActivity extends SDLActivity {
    private PadView pad;
    private InputManager inputManager;
    private android.widget.TextView fpsView;
    private final android.os.Handler fpsHandler=new android.os.Handler(android.os.Looper.getMainLooper());
    private final Runnable fpsUpdate=new Runnable(){public void run(){
        if(fpsView==null)return;float fps=nativeGetFps();
        fpsView.setText(fps>0 ? String.format(Locale.US,"%.0f FPS",fps) : "-- FPS");
        fpsHandler.postDelayed(this,500);
    }};
    private final InputManager.InputDeviceListener deviceListener=new InputManager.InputDeviceListener(){
        public void onInputDeviceAdded(int id){refreshControls();}
        public void onInputDeviceRemoved(int id){refreshControls();}
        public void onInputDeviceChanged(int id){refreshControls();}
    };
    private static native void nativePad(int digital,int analog,float lx,float ly,float rx,float ry);
    private static native void nativePadReset();
    private static native void nativeRequestExit();
    private static native float nativeGetFps();
    @Override protected String[] getLibraries(){return new String[]{"SDL3","main"};}
    @Override protected String[] getArguments(){SharedPreferences preferences=getSharedPreferences("controls",MODE_PRIVATE);ArrayList<String> args=new ArrayList<>(Arrays.asList("--xbe",new File(getFilesDir(),"disc/default.xbe").getAbsolutePath(),"--vsync"));
        args.add("--android-display="+LauncherActivity.savedAspect(preferences));
        args.add("--android-language="+("en".equals(preferences.getString("language","ru")) ? "en" : "ru"));
        if(preferences.getBoolean("swapButtons",false))args.add("--android-swap-buttons");if(preferences.getInt("resolution",0)==1)args.add("--android-resolution=2");return args.toArray(new String[0]);}
    @Override protected void onCreate(Bundle saved){super.onCreate(saved);if(!mBrokenLibraries){pad=new PadView(this);mLayout.addView(pad,new android.widget.RelativeLayout.LayoutParams(-1,-1));getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);inputManager=(InputManager)getSystemService(Context.INPUT_SERVICE);if(inputManager!=null)inputManager.registerInputDeviceListener(deviceListener,null);request60Hz();refreshControls();int margin=(int)(6*getResources().getDisplayMetrics().density);
        fpsView=new android.widget.TextView(this);fpsView.setTextColor(Color.WHITE);fpsView.setTextSize(12);fpsView.setShadowLayer(2,1,1,Color.BLACK);fpsView.setPadding(margin,margin,margin,margin);fpsView.setBackgroundColor(0x66000000);fpsView.setFocusable(false);fpsView.setText("-- FPS");
        android.widget.RelativeLayout.LayoutParams fpsParams=new android.widget.RelativeLayout.LayoutParams(-2,-2);fpsParams.addRule(android.widget.RelativeLayout.ALIGN_PARENT_TOP);fpsParams.addRule(android.widget.RelativeLayout.ALIGN_PARENT_RIGHT);fpsParams.setMargins(margin,margin,margin,margin);mLayout.addView(fpsView,fpsParams);fpsHandler.post(fpsUpdate);}}
    @Override protected void onResume(){super.onResume();refreshControls();}
    @Override protected void onDestroy(){fpsHandler.removeCallbacks(fpsUpdate);fpsView=null;if(inputManager!=null)inputManager.unregisterInputDeviceListener(deviceListener);boolean terminate=isFinishing();super.onDestroy();if(terminate)android.os.Process.killProcess(android.os.Process.myPid());}
    private void request60Hz(){Display display=getWindowManager().getDefaultDisplay();Display.Mode current=display.getMode(),best=null;float distance=2f;for(Display.Mode mode:display.getSupportedModes()){float delta=Math.abs(mode.getRefreshRate()-60f);if(mode.getPhysicalWidth()==current.getPhysicalWidth()&&mode.getPhysicalHeight()==current.getPhysicalHeight()&&delta<distance){best=mode;distance=delta;}}WindowManager.LayoutParams params=getWindow().getAttributes();params.preferredRefreshRate=60f;if(best!=null)params.preferredDisplayModeId=best.getModeId();getWindow().setAttributes(params);}
    private void refreshControls(){if(pad==null)return;boolean connected=false;for(int id:InputDevice.getDeviceIds()){InputDevice device=InputDevice.getDevice(id);if(device!=null&&(((device.getSources()&InputDevice.SOURCE_GAMEPAD)==InputDevice.SOURCE_GAMEPAD)||((device.getSources()&InputDevice.SOURCE_JOYSTICK)==InputDevice.SOURCE_JOYSTICK))){connected=true;break;}}int mode=getSharedPreferences("controls",MODE_PRIVATE).getInt("touchControls",0);boolean visible=mode==1||(mode==0&&!connected);if(!visible)pad.clear();pad.setVisibility(visible?View.VISIBLE:View.GONE);}
    @Override protected void onPause(){if(pad!=null)pad.clear();super.onPause();}
    @Override public boolean dispatchKeyEvent(KeyEvent event){
        if(event.getKeyCode()==KeyEvent.KEYCODE_BUTTON_SELECT){
            if(event.getAction()==KeyEvent.ACTION_DOWN&&event.getRepeatCount()==0)showGameMenu();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }
    @Override public void onBackPressed(){showGameMenu();}
    // The guest runner exits its dedicated :game process after native cleanup.
    // Calling finish here starts SDLActivity.nativeQuit concurrently with it.
    private void showGameMenu(){boolean english="en".equals(getSharedPreferences("controls",MODE_PRIVATE).getString("language","ru"));new android.app.AlertDialog.Builder(this).setTitle(english ? "Game" : "Игра").setItems(english ? new String[]{"Resume game","Return to app menu"} : new String[]{"Продолжить игру","Выйти в меню приложения"},(dialog,which)->{if(which==1){if(pad!=null)pad.clear();nativeRequestExit();}}).show();}
    private static final class Control {String label;float x,y,r;int digital,analog,stick;final boolean menu;
        Control(String l,float x,float y,float r,int d,int a,int s){this(l,x,y,r,d,a,s,false);}
        Control(String l,float x,float y,float r,int d,int a,int s,boolean menu){label=l;this.x=x;this.y=y;this.r=r;digital=d;analog=a;stick=s;this.menu=menu;}}
    private static final Control[] controls={
        new Control("L",.12f,.69f,.080f,0,0,1),new Control("R",.73f,.78f,.065f,0,0,2),
        new Control("A",.89f,.79f,.037f,0,1,0),new Control("B",.96f,.65f,.037f,0,2,0),new Control("X",.82f,.65f,.037f,0,4,0),new Control("Y",.89f,.51f,.037f,0,8,0),
        new Control("WHITE",.72f,.18f,.037f,0,32,0),new Control("BLACK",.82f,.18f,.037f,0,16,0),
        new Control("LT",.08f,.18f,.040f,0,64,0),new Control("RT",.92f,.18f,.040f,0,128,0),
        new Control("L3",.44f,.18f,.032f,64,0,0),new Control("R3",.56f,.18f,.032f,128,0,0),
        new Control("START",.56f,.92f,.032f,16,0,0),new Control("BACK",.44f,.92f,.032f,0,0,0,true),
        new Control("↑",.27f,.53f,.026f,1,0,0),new Control("↓",.27f,.79f,.026f,2,0,0),new Control("←",.22f,.66f,.026f,4,0,0),new Control("→",.32f,.66f,.026f,8,0,0)};
    private static final class Finger {Control control;float x,y;Finger(Control c,float x,float y){control=c;this.x=x;this.y=y;}}
    private static final class PadView extends View {
        private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);private final Map<Integer,Finger> fingers=new HashMap<>();
        private final GameActivity activity;
        PadView(GameActivity activity){super(activity);this.activity=activity;setFocusable(false);setLayerType(View.LAYER_TYPE_SOFTWARE,null);}
        float radius(Control c){return c.r*getWidth();}
        void clear(){fingers.clear();nativePadReset();invalidate();}
        private void send(){int digital=0,analog=0;float lx=0,ly=0,rx=0,ry=0;for(Finger f:fingers.values()){Control c=f.control;digital|=c.digital;analog|=c.analog;if(c.stick!=0){float x=(f.x-c.x*getWidth())/radius(c),y=(c.y*getHeight()-f.y)/radius(c);float length=(float)Math.sqrt(x*x+y*y);if(length>1){x/=length;y/=length;}if(c.stick==1){lx=x;ly=y;}else{rx=x;ry=y;}}}nativePad(digital,analog,lx,ly,rx,ry);invalidate();}
        @Override public boolean onTouchEvent(MotionEvent event){int action=event.getActionMasked(),index=event.getActionIndex();if(action==MotionEvent.ACTION_CANCEL){clear();return true;}
            if(action==MotionEvent.ACTION_DOWN||action==MotionEvent.ACTION_POINTER_DOWN){float x=event.getX(index),y=event.getY(index);for(Control c:controls){float dx=x-c.x*getWidth(),dy=y-c.y*getHeight();if(dx*dx+dy*dy<=radius(c)*radius(c)){
                // A touch menu action must not send the Xbox BACK bit or leave
                // another finger's stick/button held while the dialog is open.
                if(c.menu){clear();activity.showGameMenu();return true;}
                boolean used=false;for(Finger f:fingers.values())if(f.control==c)used=true;if(!used)fingers.put(event.getPointerId(index),new Finger(c,x,y));break;}}}
            if(action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_POINTER_UP)fingers.remove(event.getPointerId(index));
            for(int i=0;i<event.getPointerCount();++i){Finger f=fingers.get(event.getPointerId(i));if(f!=null){f.x=event.getX(i);f.y=event.getY(i);}}send();return true;}
        @Override protected void onDraw(Canvas canvas){super.onDraw(canvas);paint.setTextAlign(Paint.Align.CENTER);for(Control c:controls){boolean held=false;for(Finger f:fingers.values())if(f.control==c)held=true;float x=c.x*getWidth(),y=c.y*getHeight(),r=radius(c);paint.setColor(held?0xAA4BB4EE:0x55666666);paint.setStyle(Paint.Style.FILL);canvas.drawCircle(x,y,r,paint);paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(2);paint.setColor(0xAADDFFFF);canvas.drawCircle(x,y,r,paint);paint.setStyle(Paint.Style.FILL);paint.setTextSize(r*(c.label.length()>2?.38f:.7f));canvas.drawText(c.label,x,y-(paint.ascent()+paint.descent())/2,paint);}
            for(Finger f:fingers.values())if(f.control.stick!=0){float x=f.control.x*getWidth(),y=f.control.y*getHeight(),dx=f.x-x,dy=f.y-y,r=radius(f.control),length=(float)Math.sqrt(dx*dx+dy*dy);if(length>r){dx*=r/length;dy*=r/length;}paint.setColor(0xAA4BB4EE);canvas.drawCircle(x+dx,y+dy,r*.35f,paint);}}
    }
}
