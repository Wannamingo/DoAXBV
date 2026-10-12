#pragma once
#include <stdint.h>

// Keep action IDs aligned with ControlBindings.java. Inputs are sampled before
// mapping, so swaps, cycles and several sources sharing one action are safe.
namespace doaxbv {
inline unsigned stick_directions(int x,int y) {
    if(int64_t(x)*x+int64_t(y)*y<int64_t(9830)*9830)return 0;
    const int ax=x<0?-x:x,ay=y<0?-y:y;unsigned bits=0;
    if(ax*10000>ay*4142)bits|=x<0?4:8;
    if(ay*10000>ax*4142)bits|=y<0?2:1;
    return bits;
}
struct ButtonState { uint16_t digital{}; uint8_t analog[8]{}; bool menu{}; };
inline ButtonState map_buttons(const uint8_t source[16],const int binding[16]) {
    ButtonState result;
    for(unsigned i=0;i<16;++i) {
        const int action=binding[i];const uint8_t value=source[i];
        if(!value)continue;
        if(action>=1&&action<=8) { if(value>=128)result.digital|=uint16_t(1u<<(action-1)); }
        else if(action>=9&&action<=16) { uint8_t &output=result.analog[action-9];if(value>output)output=value; }
        else if(action==17&&value>=128)result.menu=true;
    }
    if((result.digital&3)==3)result.digital&=~3;
    if((result.digital&12)==12)result.digital&=~12;
    return result;
}
}
