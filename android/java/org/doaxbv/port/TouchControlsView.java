package org.doaxbv.port;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.CornerPathEffect;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Toast;
import java.util.Map;
import java.util.LinkedHashMap;

/** Transparent outline controller, drawn with native Android Canvas. */
final class TouchControlsView extends View {
    interface Listener {
        void input(int digital,int analog,float lx,float ly,float rx,float ry);
        void reset();
        void menu();
        void settings();
        void editing(boolean active);
    }
    private final Listener listener;
    private final TouchControlsModel model=new TouchControlsModel();
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF rect=new RectF();
    private final Path dpadArm=new Path();
    private final RectF editButton=new RectF(),settingsButton=new RectF(),toolbar=new RectF();
    private final SharedPreferences layoutPreferences;
    private boolean english;
    private boolean editing,controlsVisible=true;
    private Map<String,float[]> beforeEdit;
    private int dragPointer=-1;
    private String dragged;
    private float dragOffsetX,dragOffsetY;
    private CornerPathEffect corners;
    private float stroke;
    private static final int OUTLINE=0x78ffffff,ACTIVE=0xe6ffffff;

    TouchControlsView(Context context,Listener listener) {
        super(context);this.listener=listener;setFocusable(false);
        layoutPreferences=context.getSharedPreferences("touch-layout",Context.MODE_PRIVATE);
        english="en".equals(context.getSharedPreferences("controls",Context.MODE_PRIVATE).getString("language","ru"));
        model.setPositions(TouchLayoutStore.load(layoutPreferences));
        setContentDescription("Touch gamepad: D-pad, two sticks, A B X Y, L1 L2 L3, R1 R2 R3, Select and Start");
        paint.setStrokeCap(Paint.Cap.ROUND);paint.setStrokeJoin(Paint.Join.ROUND);
        paint.setTypeface(Typeface.create("sans-serif",Typeface.BOLD));
    }
    void clear(){model.clear();listener.reset();invalidate();}
    void setBindings(int[] bindings){model.setBindings(bindings);listener.reset();invalidate();}
    void setLanguage(boolean english){this.english=english;invalidate();}
    void setControlsVisible(boolean visible){controlsVisible=visible;if(!visible)clear();invalidate();}
    boolean isEditing(){return editing;}
    void cancelEditing(){if(editing)finishEditing(false);}
    void saveEditing(){if(editing)finishEditing(true);}
    void beginEditing(){
        if(editing)return;
        beforeEdit=model.positions();dragPointer=-1;dragged=null;clear();editing=true;
        listener.editing(true);invalidate();
    }
    private void finishEditing(boolean save){
        if(save&&!TouchLayoutStore.save(layoutPreferences,model.positions())){Toast.makeText(getContext(),english?"Could not save layout":"Не удалось сохранить расположение",Toast.LENGTH_SHORT).show();return;}
        if(!save)model.setPositions(beforeEdit);
        dragPointer=-1;dragged=null;editing=false;clear();listener.editing(false);invalidate();
    }
    @Override protected void onSizeChanged(int w,int h,int oldw,int oldh) {
        super.onSizeChanged(w,h,oldw,oldh);cancelEditing();model.resize(w,h);listener.reset();
        stroke=Math.max(1.5f,model.unit()*.0022f);
        corners=new CornerPathEffect(model.unit()*.005f);
        float r=model.unit()*.13f;
        dpadArm.reset();dpadArm.moveTo(-r*.27f,-r);dpadArm.lineTo(r*.27f,-r);
        dpadArm.lineTo(r*.27f,-r*.32f);dpadArm.lineTo(0,-r*.065f);
        dpadArm.lineTo(-r*.27f,-r*.32f);dpadArm.close();
        float s=model.unit(),side=Math.max(40,s*.052f),cx=w*.5f+s*.11f;
        editButton.set(cx-side*.5f,s*.012f,cx+side*.5f,s*.012f+side);
        settingsButton.set(editButton.right+s*.012f,editButton.top,editButton.right+s*.012f+side,editButton.bottom);
        float tw=Math.min(w-s*.08f,s*.85f),top=h*.14f;
        toolbar.set((w-tw)*.5f,top,(w+tw)*.5f,top+s*.065f);
    }
    @Override protected void onDetachedFromWindow(){cancelEditing();clear();super.onDetachedFromWindow();}
    private void send() {
        TouchControlsModel.State state=model.state();
        if(state.menu){clear();listener.menu();return;}
        listener.input(state.digital,state.analog,state.lx,state.ly,state.rx,state.ry);invalidate();
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
        int action=event.getActionMasked(),index=event.getActionIndex();
        if(editing)return editTouch(event);
        if(action==MotionEvent.ACTION_DOWN&&editButton.contains(event.getX(),event.getY())){beginEditing();return true;}
        if(action==MotionEvent.ACTION_DOWN&&settingsButton.contains(event.getX(),event.getY())){clear();listener.settings();return true;}
        if(!controlsVisible)return false;
        if(action==MotionEvent.ACTION_CANCEL){clear();return true;}
        if(action==MotionEvent.ACTION_DOWN||action==MotionEvent.ACTION_POINTER_DOWN) {
            int result=model.down(event.getPointerId(index),event.getX(index),event.getY(index));
            if(result==TouchControlsModel.OPEN_MENU){listener.reset();invalidate();listener.menu();return true;}
        }
        if(action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_POINTER_UP)model.up(event.getPointerId(index));
        for(int i=0;i<event.getPointerCount();i++)
            model.move(event.getPointerId(i),event.getX(i),event.getY(i));
        send();return true;
    }
    private boolean editTouch(MotionEvent event){
        int action=event.getActionMasked(),index=event.getActionIndex();
        if(action==MotionEvent.ACTION_CANCEL){dragPointer=-1;dragged=null;invalidate();return true;}
        if(action==MotionEvent.ACTION_DOWN){
            float x=event.getX(),y=event.getY();
            if(toolbar.contains(x,y)){
                int choice=(int)((x-toolbar.left)/(toolbar.width()/3));
                if(choice==0)finishEditing(true);
                else if(choice==1){model.setPositions(new LinkedHashMap<>());dragPointer=-1;dragged=null;invalidate();}
                else finishEditing(false);
                return true;
            }
            TouchControlsModel.Control c=model.controlAt(x,y);
            if(c!=null){dragged=c.id;dragPointer=event.getPointerId(0);dragOffsetX=c.x-x;dragOffsetY=c.y-y;invalidate();}
        }
        if(action==MotionEvent.ACTION_MOVE&&dragged!=null){
            int pointer=event.findPointerIndex(dragPointer);
            if(pointer>=0){model.moveControl(dragged,event.getX(pointer)+dragOffsetX,event.getY(pointer)+dragOffsetY);invalidate();}
        }
        if((action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_POINTER_UP)&&event.getPointerId(index)==dragPointer){dragPointer=-1;dragged=null;invalidate();}
        return true;
    }
    private void style(int color,Paint.Style style,float width) {
        paint.setColor(color);paint.setStyle(style);paint.setStrokeWidth(width);paint.setPathEffect(null);
    }
    private void label(Canvas canvas,String text,float x,float y,float size,int color) {
        style(color,Paint.Style.FILL,stroke);paint.setTextAlign(Paint.Align.CENTER);paint.setTextSize(size);
        canvas.drawText(text,x,y-(paint.ascent()+paint.descent())*.5f,paint);
    }
    private void button(Canvas canvas,TouchControlsModel.Control c) {
        boolean held=model.pressed(c);int color=held?ACTIVE:OUTLINE;
        rect.set(c.x-c.halfWidth,c.y-c.halfHeight,c.x+c.halfWidth,c.y+c.halfHeight);
        float radius=model.unit()*.012f;
        if(held){style(0x25ffffff,Paint.Style.FILL,stroke);canvas.drawRoundRect(rect,radius,radius,paint);}
        style(color,Paint.Style.STROKE,stroke);canvas.drawRoundRect(rect,radius,radius,paint);
        String text=model.label(c);
        label(canvas,text,c.x,c.y,Math.min(c.halfHeight*.94f,c.halfWidth*3f/Math.max(1,text.length())),held?ACTIVE:0xa8ffffff);
    }
    private void face(Canvas canvas,TouchControlsModel.Control c) {
        boolean held=model.pressed(c);int rgb=ControlBindings.color(model.action(c),0xcccccc),color=(held?0xff000000:0xd0000000)|rgb;
        if(held){style(0x28000000|rgb,Paint.Style.FILL,stroke);canvas.drawCircle(c.x,c.y,c.halfWidth,paint);}
        style(color,Paint.Style.STROKE,stroke*1.55f);canvas.drawCircle(c.x,c.y,c.halfWidth,paint);
        String text=model.label(c);label(canvas,text,c.x,c.y,Math.min(c.halfWidth*1.05f,c.halfWidth*2.6f/Math.max(1,text.length())),color);
    }
    private void stick(Canvas canvas,TouchControlsModel.Control c) {
        boolean held=model.pressed(c);float[] offset=model.stick(c);
        style(held?0xa8ffffff:OUTLINE,Paint.Style.STROKE,stroke*1.15f);
        canvas.drawCircle(c.x,c.y,c.halfWidth,paint);
        float x=c.x+offset[0]*c.halfWidth*.60f,y=c.y-offset[1]*c.halfHeight*.60f,r=c.halfWidth*.39f;
        if(held){style(0x1cffffff,Paint.Style.FILL,stroke);canvas.drawCircle(x,y,r,paint);}
        style(held?ACTIVE:OUTLINE,Paint.Style.STROKE,stroke);canvas.drawCircle(x,y,r,paint);
    }
    private void dpad(Canvas canvas,TouchControlsModel.Control c,int digital) {
        int[] bits={TouchControlsModel.UP,TouchControlsModel.RIGHT,TouchControlsModel.DOWN,TouchControlsModel.LEFT};
        canvas.save();canvas.translate(c.x,c.y);
        for(int bit:bits) {
            boolean held=(digital&bit)!=0;
            if(held){style(0x25ffffff,Paint.Style.FILL,stroke);paint.setPathEffect(corners);canvas.drawPath(dpadArm,paint);}
            style(held?ACTIVE:OUTLINE,Paint.Style.STROKE,stroke);paint.setPathEffect(corners);canvas.drawPath(dpadArm,paint);
            canvas.rotate(90);
        }
        paint.setPathEffect(null);canvas.restore();
    }
    private void editButton(Canvas canvas){
        style(0x55000000,Paint.Style.FILL,stroke);canvas.drawRoundRect(editButton,9,9,paint);
        style(0x99ffffff,Paint.Style.STROKE,stroke);canvas.drawRoundRect(editButton,9,9,paint);
        float cx=editButton.centerX(),cy=editButton.centerY(),r=editButton.width()*.21f;
        canvas.drawLine(cx-r,cy+r,cx+r,cy-r,paint);canvas.drawLine(cx-r,cy+r,cx-r*.9f,cy+r*.4f,paint);canvas.drawLine(cx-r,cy+r,cx-r*.4f,cy+r*.9f,paint);
        style(0x55000000,Paint.Style.FILL,stroke);canvas.drawRoundRect(settingsButton,9,9,paint);
        style(0x99ffffff,Paint.Style.STROKE,stroke);canvas.drawRoundRect(settingsButton,9,9,paint);
        label(canvas,"⚙",settingsButton.centerX(),settingsButton.centerY(),settingsButton.width()*.64f,0xbfffffff);
    }
    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);TouchControlsModel.State state=model.state();
        if(!controlsVisible&&!editing){editButton(canvas);return;}
        if(editing){style(0x55000000,Paint.Style.FILL,stroke);canvas.drawRect(0,0,getWidth(),getHeight(),paint);}
        for(TouchControlsModel.Control c:model.controls) {
            switch(c.kind) {
                case TouchControlsModel.DPAD:dpad(canvas,c,model.pressedDirections());break;
                case TouchControlsModel.LEFT_STICK:case TouchControlsModel.RIGHT_STICK:stick(canvas,c);break;
                case TouchControlsModel.FACE:face(canvas,c);break;
                default:button(canvas,c);
            }
            if(editing){style(c.id.equals(dragged)?0xffffc294:0x55ffc294,Paint.Style.STROKE,stroke);canvas.drawRoundRect(c.x-c.halfWidth-5,c.y-c.halfHeight-5,c.x+c.halfWidth+5,c.y+c.halfHeight+5,8,8,paint);}
        }
        if(!editing)editButton(canvas);
        if(editing){
            style(0xf0211018,Paint.Style.FILL,stroke);canvas.drawRoundRect(toolbar,12,12,paint);
            style(0xddffc294,Paint.Style.STROKE,stroke);canvas.drawRoundRect(toolbar,12,12,paint);
            String[] labels=english?new String[]{"Save","Reset","Cancel"}:new String[]{"Сохранить","Сбросить","Отмена"};
            for(int i=0;i<3;i++){float x=toolbar.left+toolbar.width()*(i+.5f)/3;label(canvas,labels[i],x,toolbar.centerY(),model.unit()*.022f,0xffffddbd);if(i>0){style(0x55ffc294,Paint.Style.STROKE,stroke);x=toolbar.left+toolbar.width()*i/3;canvas.drawLine(x,toolbar.top+10,x,toolbar.bottom-10,paint);}}
            label(canvas,english?"Drag controls · Game paused":"Перетащите элементы · Игра на паузе",getWidth()*.5f,toolbar.bottom+model.unit()*.03f,model.unit()*.020f,0xffffddbd);
        }
    }
}
