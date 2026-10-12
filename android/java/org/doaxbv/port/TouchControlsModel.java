package org.doaxbv.port;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Geometry and multi-pointer Xbox input, independent of Android drawing APIs. */
final class TouchControlsModel {
    static final int BUTTON=0, FACE=1, DPAD=2, LEFT_STICK=3, RIGHT_STICK=4, MENU=5;
    static final int IGNORED=0, TRACKED=1, OPEN_MENU=2;
    static final int UP=1, DOWN=2, LEFT=4, RIGHT=8;

    static final class Control {
        final String id,label;
        final int kind,digital,analog,color;
        float x,y;
        final float halfWidth,halfHeight;
        Control(String id,String label,int kind,float x,float y,float halfWidth,float halfHeight,
                int digital,int analog,int color) {
            this.id=id;this.label=label;this.kind=kind;this.x=x;this.y=y;
            this.halfWidth=halfWidth;this.halfHeight=halfHeight;
            this.digital=digital;this.analog=analog;this.color=color;
        }
        boolean contains(float px,float py) {
            float dx=px-x,dy=py-y;
            if(kind==BUTTON||kind==MENU)return Math.abs(dx)<=halfWidth*1.12f&&Math.abs(dy)<=halfHeight*1.25f;
            if(kind==DPAD)return Math.abs(dx)<=halfWidth&&Math.abs(dy)<=halfHeight;
            float radius=halfWidth*(kind==FACE?1.12f:1f);
            return dx*dx+dy*dy<=radius*radius;
        }
    }
    static final class State {
        int digital,analog;
        float lx,ly,rx,ry;
        boolean menu;
    }
    private static final class Touch {
        final Control origin;
        Control button;
        float x,y;
        Touch(Control control,float x,float y) {origin=button=control;this.x=x;this.y=y;}
    }

    final List<Control> controls=new ArrayList<>();
    private final Map<Integer,Touch> touches=new LinkedHashMap<>();
    private float unit;
    private int width,height;
    private final Map<String,float[]> positions=new LinkedHashMap<>();
    private int[] bindings=ControlBindings.defaults(false);
    void setBindings(int[] values){clear();bindings=ControlBindings.sanitize(values);}
    int action(Control c){int slot=ControlBindings.slot(c.id);return slot<0?0:bindings[slot];}
    String label(Control c){int slot=ControlBindings.slot(c.id);return slot<0||bindings[slot]==ControlBindings.defaults(false)[slot]?c.label:ControlBindings.label(bindings[slot]);}

    void resize(int width,int height) {
        this.width=width;this.height=height;
        clear();controls.clear();unit=Math.min(width,height);
        if(unit<=0)return;
        float s=unit,top=.075f*height,w=.055f*s,h=.028f*s,step=.14f*s,edge=.075f*s;
        add("l1","L1",BUTTON,edge,top,w,h,0,16,0);
        add("l2","L2",BUTTON,edge+step,top,w,h,0,64,0);
        add("l3","L3",BUTTON,edge+step*2,top,w,h,64,0,0);
        add("r3","R3",BUTTON,width-edge-step*2,top,w,h,128,0,0);
        add("r2","R2",BUTTON,width-edge-step,top,w,h,0,128,0);
        add("r1","R1",BUTTON,width-edge,top,w,h,0,32,0);

        add("dpad","",DPAD,.20f*s,.38f*height,.13f*s,.13f*s,0,0,0);
        add("left-stick","",LEFT_STICK,.23f*s,.78f*height,.13f*s,.13f*s,0,0,0);
        add("right-stick","",RIGHT_STICK,width-.23f*s,.38f*height,.13f*s,.13f*s,0,0,0);

        float cx=width-.23f*s,cy=.78f*height,spacing=.125f*s,r=.045f*s;
        add("a","A",FACE,cx,cy+spacing,r,r,0,1,0x53d995);
        add("b","B",FACE,cx+spacing,cy,r,r,0,2,0xff5b66);
        add("x","X",FACE,cx-spacing,cy,r,r,0,4,0x54b9ff);
        add("y","Y",FACE,cx,cy-spacing,r,r,0,8,0xffd54a);

        add("select","SELECT",BUTTON,width*.5f-.09f*s,.92f*height,.060f*s,.027f*s,0,0,0);
        add("menu","≡",MENU,width*.5f,.92f*height,.022f*s,.027f*s,0,0,0);
        add("start","START",BUTTON,width*.5f+.09f*s,.92f*height,.060f*s,.027f*s,16,0,0);
        for(Control c:controls){float[] value=positions.get(c.id);if(value!=null)place(c,value[0]*width,value[1]*height);}
    }
    Map<String,float[]> positions(){Map<String,float[]> copy=new LinkedHashMap<>();for(Map.Entry<String,float[]> e:positions.entrySet())copy.put(e.getKey(),e.getValue().clone());return copy;}
    void setPositions(Map<String,float[]> values){
        positions.clear();
        if(values!=null)for(Map.Entry<String,float[]> e:values.entrySet()){
            float[] p=e.getValue();if(p!=null&&p.length==2&&Float.isFinite(p[0])&&Float.isFinite(p[1])&&p[0]>=0&&p[0]<=1&&p[1]>=0&&p[1]<=1)positions.put(e.getKey(),p.clone());
        }
        resize(width,height);
    }
    void moveControl(String id,float x,float y){Control c=control(id);place(c,x,y);positions.put(id,new float[]{c.x/width,c.y/height});}
    private void place(Control c,float x,float y){
        float margin=unit*.006f;
        c.x=Math.max(c.halfWidth+margin,Math.min(width-c.halfWidth-margin,x));
        c.y=Math.max(c.halfHeight+margin,Math.min(height-c.halfHeight-margin,y));
    }
    Control controlAt(float x,float y){return at(x,y,false);}
    private void add(String id,String label,int kind,float x,float y,float w,float h,int digital,int analog,int color) {
        controls.add(new Control(id,label,kind,x,y,w,h,digital,analog,color));
    }
    float unit(){return unit;}
    Control control(String id) {
        for(Control c:controls)if(c.id.equals(id))return c;
        throw new IllegalArgumentException("Unknown control "+id);
    }
    private Control at(float x,float y,boolean buttonsOnly) {
        for(int i=controls.size()-1;i>=0;i--) {Control c=controls.get(i);
            if(buttonsOnly&&c.kind!=BUTTON&&c.kind!=FACE)continue;
            if(c.contains(x,y))return c;
        }
        return null;
    }
    int down(int pointer,float x,float y) {
        Control c=at(x,y,false);
        if(c==null)return IGNORED;
        if(c.kind==MENU||((c.kind==BUTTON||c.kind==FACE)&&action(c)==ControlBindings.MENU)){clear();return OPEN_MENU;}
        if(c.kind==LEFT_STICK||c.kind==RIGHT_STICK)
            for(Touch touch:touches.values())if(touch.origin==c)return IGNORED;
        touches.put(pointer,new Touch(c,x,y));return TRACKED;
    }
    void move(int pointer,float x,float y) {
        Touch touch=touches.get(pointer);if(touch==null)return;
        touch.x=x;touch.y=y;
        if(touch.origin.kind==BUTTON||touch.origin.kind==FACE)touch.button=at(x,y,true);
    }
    void up(int pointer){touches.remove(pointer);}
    void clear(){touches.clear();}
    boolean pressed(Control c) {
        for(Touch t:touches.values())if(t.button==c)return true;
        return false;
    }
    // Raw normalized stick position is also used to place the visible inner ring.
    float[] stick(Control c) {
        for(Touch t:touches.values())if(t.origin==c) {
            float x=(t.x-c.x)/c.halfWidth,y=(c.y-t.y)/c.halfHeight;
            float length=(float)Math.sqrt(x*x+y*y);
            if(length>1){x/=length;y/=length;}
            return new float[]{x,y};
        }
        return new float[]{0,0};
    }
    private int directions(Touch t) {
        float dx=(t.x-t.origin.x)/t.origin.halfWidth,dy=(t.y-t.origin.y)/t.origin.halfHeight;
        float ax=Math.abs(dx),ay=Math.abs(dy);
        if(Math.max(ax,ay)<.18f)return 0;
        int bits=0;
        // Eight directions: adjacent cardinal sectors share a diagonal sector.
        if(ax>ay*.41421356f)bits|=dx<0?LEFT:RIGHT;
        if(ay>ax*.41421356f)bits|=dy<0?UP:DOWN;
        return bits;
    }
    State state() {
        State result=new State();
        for(Touch t:touches.values()) {
            if(t.origin.kind==DPAD){int bits=directions(t);for(int i=0;i<4;i++)if((bits&(1<<i))!=0)apply(result,bindings[i]);}
            else if(t.origin.kind==BUTTON||t.origin.kind==FACE) {
                if(t.button!=null)apply(result,action(t.button));
            }
        }
        for(Control c:controls)if(c.kind==LEFT_STICK||c.kind==RIGHT_STICK) {
            float[] value=stick(c);
            // Suppress tiny finger movements without shrinking full stick travel.
            float length=(float)Math.sqrt(value[0]*value[0]+value[1]*value[1]);
            if(length<=.10f)value[0]=value[1]=0;
            else {float gain=(length-.10f)/(.90f*length);value[0]*=gain;value[1]*=gain;}
            if(c.kind==LEFT_STICK){
                result.lx=value[0];result.ly=value[1];
                int bits=stickDirections(value[0],value[1]);
                for(int i=0;i<4;i++)if((bits&(1<<i))!=0)apply(result,bindings[i]);
            }
            else {result.rx=value[0];result.ry=value[1];}
        }
        if((result.digital&(UP|DOWN))==(UP|DOWN))result.digital&=~(UP|DOWN);
        if((result.digital&(LEFT|RIGHT))==(LEFT|RIGHT))result.digital&=~(LEFT|RIGHT);
        return result;
    }
    private static int stickDirections(float x,float y){
        if(x*x+y*y<.30f*.30f)return 0;
        float ax=Math.abs(x),ay=Math.abs(y);int bits=0;
        if(ax>ay*.41421356f)bits|=x<0?LEFT:RIGHT;
        if(ay>ax*.41421356f)bits|=y<0?DOWN:UP;
        return bits;
    }
    int pressedDirections(){int bits=0;for(Touch t:touches.values())if(t.origin.kind==DPAD)bits|=directions(t);return bits;}
    private static void apply(State state,int action){state.digital|=ControlBindings.digital(action);state.analog|=ControlBindings.analog(action);state.menu|=action==ControlBindings.MENU;}
}
