#include "button_bindings.h"
#include <stdio.h>
#define CHECK(c) do { if(!(c)){fprintf(stderr,"Binding check failed at line %d: %s\n",__LINE__,#c);return 1;} } while(0)

int main() {
    int map[16]={1,2,3,4,5,17,7,8,9,10,11,12,13,14,15,16};
    uint8_t raw[16]={};
    for(int i=0;i<16;++i){raw[i]=255;auto s=doaxbv::map_buttons(raw,map);if(i==5)CHECK(s.menu&&!s.digital);else if(i<8)CHECK(s.digital==(1u<<i));else CHECK(s.analog[i-8]==255);raw[i]=0;}
    map[8]=10;map[9]=9;raw[8]=255;auto s=doaxbv::map_buttons(raw,map);CHECK(s.analog[1]==255&&!s.analog[0]);
    raw[9]=255;s=doaxbv::map_buttons(raw,map);CHECK(s.analog[0]==255&&s.analog[1]==255);
    map[8]=15;map[14]=15;raw[14]=170;s=doaxbv::map_buttons(raw,map);CHECK(s.analog[6]==255);raw[8]=0;s=doaxbv::map_buttons(raw,map);CHECK(s.analog[6]==170);
    map[14]=5;raw[14]=127;s=doaxbv::map_buttons(raw,map);CHECK(!(s.digital&16));raw[14]=128;s=doaxbv::map_buttons(raw,map);CHECK(s.digital&16);
    map[9]=0;map[14]=99;s=doaxbv::map_buttons(raw,map);CHECK(!s.digital&&!s.menu);for(auto a:s.analog)CHECK(!a);
    CHECK(doaxbv::stick_directions(0,0)==0&&doaxbv::stick_directions(3000,3000)==0);
    CHECK(doaxbv::stick_directions(32767,0)==8&&doaxbv::stick_directions(-32768,0)==4);
    CHECK(doaxbv::stick_directions(0,32767)==1&&doaxbv::stick_directions(0,-32768)==2);
    CHECK(doaxbv::stick_directions(32767,32767)==9&&doaxbv::stick_directions(-32768,32767)==5);
    CHECK(doaxbv::stick_directions(32767,-32768)==10&&doaxbv::stick_directions(-32768,-32768)==6);
    puts("Native bindings passed: defaults, cycles, simultaneous presses, shared actions, analog pressure, threshold, disabled/invalid actions.");
}
