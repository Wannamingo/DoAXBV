package org.doaxbv.port;

/** Shared action numbers: 1..8 Xbox digital bits, 9..16 analog buttons, 17 app menu. */
final class ControlBindings {
    static final int DISABLED=0, MENU=17, COUNT=18;
    static final String[] SLOTS={"up","down","left","right","start","select","l3","r3","a","b","x","y","l1","r1","l2","r2"};
    private static final int[] DEFAULT={1,2,3,4,5,MENU,7,8,9,10,11,12,13,14,15,16};
    static boolean valid(int action){return action>=0&&action<COUNT;}
    static int slot(String id){for(int i=0;i<SLOTS.length;i++)if(SLOTS[i].equals(id))return i;return -1;}
    static int[] defaults(boolean swap){
        int[] result=DEFAULT.clone();
        if(swap){result[8]=10;result[9]=9;result[10]=12;result[11]=11;}
        return result;
    }
    static int[] sanitize(int[] values){
        int[] result=defaults(false);
        if(values!=null&&values.length==SLOTS.length)for(int i=0;i<result.length;i++)if(valid(values[i]))result[i]=values[i];
        return result;
    }
    static String label(int action){return new String[]{"—","↑","↓","←","→","START","BACK","L3","R3","A","B","X","Y","BLACK","WHITE","LT","RT","≡"}[valid(action)?action:0];}
    static int digital(int action){return action>=1&&action<=8?1<<(action-1):0;}
    static int analog(int action){return action>=9&&action<=16?1<<(action-9):0;}
    static int color(int action,int fallback){switch(action){case 9:return 0x53d995;case 10:return 0xff5b66;case 11:return 0x54b9ff;case 12:return 0xffd54a;default:return fallback;}}
}
