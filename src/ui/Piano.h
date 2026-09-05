#pragma once
#include <array>
namespace dopa::piano {
inline constexpr int firstNote=48,keyCount=37;
struct Key {int note;float x,y,w,h;bool black;};
inline const std::array<Key,keyCount>& keys(){
    static const auto result=[] {std::array<Key,keyCount> out{};int white=0;for(int i=0;i<keyCount;++i){int tone=i%12;bool black=tone==1||tone==3||tone==6||tone==8||tone==10;
        out[i]={firstNote+i,black?164.f+white*46-14:164.f+white*46,646,black?28.f:45.f,black?64.f:106.f,black};if(!black)++white;}return out;}();return result;
}
inline int hit(float x,float y){for(bool black:{true,false})for(auto& key:keys())if(key.black==black&&x>=key.x&&x<key.x+key.w&&y>=key.y&&y<key.y+key.h)return key.note;return -1;}
}
