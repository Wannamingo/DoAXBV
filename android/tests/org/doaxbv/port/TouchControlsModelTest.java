package org.doaxbv.port;

public final class TouchControlsModelTest {
    private static void check(boolean result,String message){if(!result)throw new AssertionError(message);}
    private static void near(float actual,float expected,String message){check(Math.abs(actual-expected)<.0001f,message+": "+actual);}
    private static TouchControlsModel model(){TouchControlsModel m=new TouchControlsModel();m.resize(1280,960);return m;}
    private static void down(TouchControlsModel m,int id,String key){TouchControlsModel.Control c=m.control(key);check(m.down(id,c.x,c.y)==TouchControlsModel.TRACKED,key+" down");}
    private static void neutral(TouchControlsModel m){TouchControlsModel.State s=m.state();check((s.digital|s.analog)==0,"stuck buttons");near(s.lx,0,"stuck LX");near(s.ly,0,"stuck LY");near(s.rx,0,"stuck RX");near(s.ry,0,"stuck RY");}
    public static void main(String[] args){
        for(int[] size:new int[][]{{1280,960},{1920,1080},{2400,1080},{844,390}}){
            TouchControlsModel m=new TouchControlsModel();m.resize(size[0],size[1]);
            check(m.controls.size()==16,"control count");
            for(TouchControlsModel.Control c:m.controls){
                check(c.x-c.halfWidth>=0&&c.y-c.halfHeight>=0&&c.x+c.halfWidth<=size[0]&&c.y+c.halfHeight<=size[1],size[0]+" bounds "+c.id);
                for(TouchControlsModel.Control other:m.controls)if(c!=other){
                    check(Math.abs(c.x-other.x)>=c.halfWidth+other.halfWidth||Math.abs(c.y-other.y)>=c.halfHeight+other.halfHeight,"overlap "+c.id+"/"+other.id);
                }
            }
        }
        TouchControlsModel m=model();
        String[] keys={"a","b","x","y","l1","r1","l2","r2","l3","r3","start"};
        int[] analog={1,2,4,8,16,32,64,128,0,0,0},digital={0,0,0,0,0,0,0,0,64,128,16};
        for(int i=0;i<keys.length;i++){down(m,7,keys[i]);check(m.state().analog==analog[i]&&m.state().digital==digital[i],keys[i]+" Xbox mapping");m.up(7);neutral(m);}

        TouchControlsModel.Control d=m.control("dpad");
        int[] dirs={TouchControlsModel.UP,TouchControlsModel.UP|TouchControlsModel.RIGHT,TouchControlsModel.RIGHT,TouchControlsModel.RIGHT|TouchControlsModel.DOWN,TouchControlsModel.DOWN,TouchControlsModel.DOWN|TouchControlsModel.LEFT,TouchControlsModel.LEFT,TouchControlsModel.LEFT|TouchControlsModel.UP};
        down(m,1,"dpad");
        for(int i=0;i<8;i++){double angle=Math.PI*(i/4.0-.5);m.move(1,d.x+(float)Math.cos(angle)*d.halfWidth*.75f,d.y+(float)Math.sin(angle)*d.halfHeight*.75f);check(m.state().digital==dirs[i],"D-pad sector "+i);}
        m.move(1,d.x,d.y);check(m.state().digital==0,"D-pad neutral center");m.up(1);

        down(m,11,"left-stick");TouchControlsModel.Control left=m.control("left-stick");
        m.move(11,left.x+left.halfWidth*3,left.y-left.halfHeight*4);
        near(m.state().lx,.6f,"stick clamp X");near(m.state().ly,.8f,"stick clamp Y and inversion");
        check(m.state().digital==(TouchControlsModel.UP|TouchControlsModel.RIGHT),"left stick duplicates diagonal D-pad input");
        down(m,42,"a");down(m,99,"x");check(m.state().analog==5,"A+X combo with stick");
        check(m.down(12,left.x,left.y)==TouchControlsModel.IGNORED,"one pointer owns each stick");
        m.up(42);check(m.state().analog==4,"release only the lifted pointer");near(m.state().lx,.6f,"stick kept during release");
        down(m,3,"right-stick");TouchControlsModel.Control right=m.control("right-stick");m.move(3,right.x,right.y+right.halfHeight);near(m.state().ry,-1,"right stick independent");
        m.clear();neutral(m);

        down(m,1,"a");down(m,2,"a");m.up(1);check(m.state().analog==1,"second pointer retains button");m.up(2);neutral(m);
        down(m,1,"a");TouchControlsModel.Control x=m.control("x");m.move(1,x.x,x.y);check(m.state().analog==4,"slide between face buttons");
        m.move(1,640,480);check(m.state().analog==0,"drag outside releases button");m.up(1);neutral(m);

        down(m,1,"left-stick");down(m,2,"b");TouchControlsModel.Control menu=m.control("select");
        check(m.down(3,menu.x,menu.y)==TouchControlsModel.OPEN_MENU,"Select opens host menu");neutral(m);
        down(m,1,"start");m.resize(2400,1080);neutral(m);
        check(m.down(1,1200,500)==TouchControlsModel.IGNORED,"game center is not a button");
        m=model();int[] bindings=ControlBindings.defaults(false);bindings[8]=10;bindings[9]=9;bindings[4]=0;bindings[5]=6;bindings[0]=12;
        m.setBindings(bindings);bindings[8]=0; // Model must own a defensive copy.
        down(m,1,"a");check(m.state().analog==2&&m.label(m.control("a")).equals("B"),"touch A to B with label");
        down(m,2,"b");check(m.state().analog==3,"mapping cycle and simultaneous presses");m.clear();
        down(m,1,"start");neutral(m);m.clear();down(m,1,"select");check(m.state().digital==32,"Select mapped to Xbox Back");m.clear();
        d=m.control("dpad");m.down(1,d.x,d.y-d.halfHeight*.8f);check(m.state().analog==8&&m.pressedDirections()==1,"D-pad remapped to Y with original highlight");m.clear();
        menu=m.control("menu");check(m.down(2,menu.x,menu.y)==TouchControlsModel.OPEN_MENU,"permanent menu survives remapping");
        bindings=ControlBindings.defaults(false);bindings[8]=17;m.setBindings(bindings);TouchControlsModel.Control a=m.control("a");check(m.down(1,a.x,a.y)==TouchControlsModel.OPEN_MENU,"face button mapped to menu");neutral(m);
        bindings[8]=99;m.setBindings(bindings);down(m,1,"a");check(m.state().analog==1,"invalid assignment falls back to default");
        m.setBindings(ControlBindings.defaults(false));neutral(m);down(m,1,"a");check(m.state().analog==1,"reset standard mapping");
        m.clear();down(m,1,"left-stick");left=m.control("left-stick");
        m.move(1,left.x+left.halfWidth*.15f,left.y);check(m.state().digital==0,"left stick direction dead zone");
        for(int i=0;i<8;i++){double angle=Math.PI*(i/4.0-.5);m.move(1,left.x+(float)Math.cos(angle)*left.halfWidth*.8f,left.y+(float)Math.sin(angle)*left.halfHeight*.8f);check(m.state().digital==dirs[i],"left stick direction "+i);}
        m.up(1);neutral(m);
        m=model();TouchControlsModel.Control moved=m.control("a");float oldX=moved.x,oldY=moved.y;
        java.util.Map<String,float[]> original=m.positions();m.moveControl("a",640,480);
        check(m.down(1,640,480)==TouchControlsModel.TRACKED&&m.state().analog==1,"moved button updates input hitbox");m.clear();
        check(m.controlAt(oldX,oldY)==null,"old hitbox released after drag");
        java.util.Map<String,float[]> saved=m.positions();saved.get("a")[0]=0;
        near(m.control("a").x,640,"layout snapshot is independent");
        m.resize(1920,1080);near(m.control("a").x,960,"saved relative X after resize");near(m.control("a").y,540,"saved relative Y after resize");
        m.moveControl("left-stick",-100,10000);TouchControlsModel.Control clipped=m.control("left-stick");
        check(clipped.x>=clipped.halfWidth&&clipped.y+clipped.halfHeight<=1080,"drag clamped to screen");
        m.setPositions(original);m.resize(1280,960);near(m.control("a").x,oldX,"cancel/reset restores default X");near(m.control("a").y,oldY,"cancel/reset restores default Y");
        saved.put("a",new float[]{Float.NaN,.5f});m.setPositions(saved);near(m.control("a").x,oldX,"corrupt layout falls back safely");
        System.out.println("Touch controls passed: four aspect ratios, Xbox mappings, 8-way D-pad, two sticks, pointer ownership, combos, sliding, release, cancel/resize and Select reset.");
    }
}
