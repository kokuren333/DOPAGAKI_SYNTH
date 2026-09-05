#include "plugin/Plugin.h"
#include "ui/Piano.h"
#include "public.sdk/source/common/pluginview.h"
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <commdlg.h>
#include <fstream>
#include <filesystem>
#include <cstring>

namespace dopa {
using namespace Gdiplus;
namespace {
constexpr int width=1200,height=780;
Color ink(255,12,9,24),gold(255,255,208,91),pink(255,255,55,149),cyan(255,67,239,236),white(255,247,244,255),muted(255,170,161,192),panel(255,24,19,40),edge(255,66,46,83);
std::wstring wide(const std::string& s){return std::wstring(s.begin(),s.end());}
void label(Graphics& g,const std::wstring& s,float x,float y,float size,Color c=white,bool bold=false){Font font(L"Segoe UI",size,bold?FontStyleBold:FontStyleRegular,UnitPixel);SolidBrush b(c);g.DrawString(s.c_str(),-1,&font,PointF(x,y),&b);}
void box(Graphics& g,float x,float y,float w,float h,Color fill,Color border){SolidBrush b(fill);g.FillRectangle(&b,x,y,w,h);Pen p(border);g.DrawRectangle(&p,x,y,w,h);}
void neon(Graphics& g,const PointF* points,int count,Color c){for(int n=3;n>0;--n){Pen glow(Color(BYTE(15*n),c.GetR(),c.GetG(),c.GetB()),float(3*n));g.DrawLines(&glow,points,count);}Pen p(c,2);g.DrawLines(&p,points,count);}
HINSTANCE moduleInstance(){HINSTANCE result{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&moduleInstance),&result);return result;}
struct Control {Param id;float x,y,w,h;const wchar_t* title;};
constexpr std::array<Control,6> easy{{{Cutoff,24,450,182,103,L"COLOR"},{Sub,218,450,182,103,L"BODY"},{Detune,412,450,182,103,L"WIDTH"},{Drive,606,450,182,103,L"HEAT"},{DelayMix,800,450,182,103,L"ECHO"},{Release,994,450,182,103,L"TAIL"}}};
constexpr std::array<Param,18> advanced{Shape,WaveMix,WavePosition,WaveMotion,PhaseStart,Octave,Detune,Spread,Sub,Resonance,Punch,Motion,LfoRate,LfoDepth,DelayMix,DelayTime,Feedback,Drive};
constexpr std::array<Control,7> fixed{{{Gain,990,24,186,62,L"MASTER"},{WaveMix,40,335,216,61,L"TABLE MIX"},{WaveMotion,272,335,216,61,L"MORPH"},{Cutoff,540,335,140,61,L"CUTOFF"},{Resonance,696,335,132,61,L"RES"},{Attack,880,335,132,61,L"ATTACK"},{Release,1028,335,132,61,L"RELEASE"}}};
}
class Editor final:public CPluginView {
public:
    explicit Editor(Controller* c):controller(c){controller->addRef();rect=ViewRect(0,0,1320,858);GdiplusStartupInput input;GdiplusStartup(&token,&input,nullptr);}
    ~Editor() override {removed();GdiplusShutdown(token);controller->release();}
    tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override{return type&&std::strcmp(type,kPlatformTypeHWND)==0?kResultTrue:kResultFalse;}
    tresult PLUGIN_API canResize() override{return kResultTrue;}
    tresult PLUGIN_API checkSizeConstraint(ViewRect* r) override {if(!r)return kInvalidArgument;int w=std::clamp(r->getWidth(),960,1800);r->right=r->left+w;r->bottom=r->top+int(w*height/double(width)+.5);return kResultTrue;}
    tresult PLUGIN_API onSize(ViewRect* r) override {if(!r||r->getWidth()<=0||r->getHeight()<=0)return kInvalidArgument;rect=*r;if(window){SetWindowPos(window,nullptr,0,0,r->getWidth(),r->getHeight(),SWP_NOMOVE|SWP_NOZORDER);layout();InvalidateRect(window,nullptr,FALSE);}return kResultTrue;}
    tresult PLUGIN_API attached(void* parent,FIDString type) override {
        if(!parent||isPlatformTypeSupported(type)!=kResultTrue||window)return kResultFalse;
        WNDCLASSW wc{};wc.lpfnWndProc=procedure;wc.hInstance=moduleInstance();wc.lpszClassName=L"DopagakiSynthEditor04";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.style=CS_DBLCLKS;RegisterClassW(&wc);
        window=CreateWindowExW(0,wc.lpszClassName,L"dopagaki_synth",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_TABSTOP,0,0,rect.getWidth(),rect.getHeight(),static_cast<HWND>(parent),nullptr,wc.hInstance,this);
        if(!window)return kResultFalse;systemWindow=parent;
        bank=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL|WS_TABSTOP,10);
        SendMessageW(bank,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"INIT / Custom patch"));for(auto& p:factoryPresets())SendMessageW(bank,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(p.name).c_str()));SendMessageW(bank,CB_SETCURSEL,0,0);
        for(auto entry:std::array<std::pair<int,const wchar_t*>,9>{{{11,L"SAVE"},{12,L"LOAD"},{13,L"INIT"},{14,L"UNDO"},{18,L"EDIT"},{20,L"ZOOM"},{21,L"PANIC"},{22,L"<"},{23,L">"}}})child(L"BUTTON",entry.second,BS_OWNERDRAW|WS_TABSTOP,entry.first);
        layout();SetTimer(window,1,33,nullptr);return kResultTrue;
    }
    tresult PLUGIN_API removed() override {if(window){finishDrag();stopPreview();KillTimer(window,1);DestroyWindow(window);window=nullptr;}systemWindow=nullptr;return kResultTrue;}
private:
    Controller* controller;HWND window=nullptr,bank=nullptr;ULONG_PTR token=0;
    int selected=index(Cutoff),mouseNote=-1,dragMode=0,lastX=0;Param dragID=Cutoff;
    bool hasUndo=false,editPage=false,modified=false;uint64_t keyboardMask=0;Values undo{};
    double sx()const{return rect.getWidth()/double(width);}double sy()const{return rect.getHeight()/double(height);}
    HWND child(const wchar_t* cls,const wchar_t* title,DWORD style,int id){return CreateWindowExW(0,cls,title,WS_CHILD|WS_VISIBLE|style,0,0,1,1,window,reinterpret_cast<HMENU>(INT_PTR(id)),moduleInstance(),nullptr);}
    void layout(){auto move=[&](int id,int x,int y,int w,int h){MoveWindow(GetDlgItem(window,id),int(x*sx()),int(y*sy()),int(w*sx()),int(h*sy()),TRUE);};
        move(10,651,30,24,420);SendMessageW(bank,CB_SETDROPPEDWIDTH,int(480*sx()),0);SendMessageW(bank,CB_SETITEMHEIGHT,WPARAM(-1),int(28*sy()));SendMessageW(bank,CB_SETITEMHEIGHT,0,int(30*sy()));
        move(22,700,72,38,27);move(23,746,72,38,27);move(11,700,30,58,30);move(12,766,30,58,30);move(13,832,30,54,30);move(14,894,30,70,30);move(18,1084,416,92,27);move(20,876,72,88,27);move(21,28,710,108,32);
    }
    void zoom(){double next=sx()<1.24?1.25:sx()<1.49?1.5:1.;ViewRect r(0,0,int(width*next),int(height*next));if(plugFrame)plugFrame->resizeView(this,&r);else onSize(&r);}
    double value(Param id){return controller->getParamNormalized(id);}
    void remember(){undo=controller->values();hasUndo=true;}
    void change(Param id,double n){modified=true;controller->setParamNormalized(id,clean(n));controller->performEdit(id,clean(n));SendMessageW(bank,CB_SETCURSEL,0,0);InvalidateRect(window,nullptr,FALSE);}
    uint64_t held()const{return keyboardMask|(mouseNote<48?0:uint64_t(1)<<(mouseNote-48));}
    void sendKeys(){controller->auditionKeys(held());InvalidateRect(window,nullptr,FALSE);}
    void stopPreview(){keyboardMask=0;mouseNote=-1;controller->audition(-1);sendKeys();}
    static int computerKey(WPARAM key){const char* mapping="AWSEDFTGYHUJKOLP";const auto* found=std::strchr(mapping,int(key));return key&&found?int(found-mapping):-1;}
    void finishDrag(){if(dragMode==1)controller->endEdit(dragID);if(dragMode==2){controller->endEdit(WavePosition);controller->endEdit(WaveMix);}if(dragMode==3){controller->endEdit(Cutoff);controller->endEdit(Resonance);}if(dragMode>=4)for(auto id:{Attack,Decay,Sustain,Release})controller->endEdit(id);dragMode=0;}
    Control advancedControl(int i){return {advanced[i],float(24+(i%6)*194),float(452+(i/6)*55),182,49,advanced[i]==Spread?L"STEREO SPREAD":advanced[i]==Detune?L"DETUNE":nullptr};}
    int hit(int x,int y){auto contains=[&](const Control& c){return x>=c.x&&x<c.x+c.w&&y>=c.y&&y<c.y+c.h;};for(auto c:fixed)if(contains(c))return index(c.id);if(editPage){for(int i=0;i<18;++i)if(contains(advancedControl(i)))return index(advanced[i]);}else for(auto c:easy)if(contains(c))return index(c.id);return -1;}
    void dragPoint(int x,int y){if(dragMode==1){change(dragID,value(dragID)+(x-lastX)*(GetKeyState(VK_SHIFT)<0?.0005:.006));lastX=x;}if(dragMode==2){change(WavePosition,(x-42)/444.);change(WaveMix,(312-y)/137.);}if(dragMode==3){change(Cutoff,(x-544)/280.);change(Resonance,(312-y)/135.);}if(dragMode==4)change(Attack,(x-890)/60.);if(dragMode==5){change(Decay,(x-966)/72.);change(Sustain,(304-y)/105.);}if(dragMode==6)change(Release,(x-1092)/65.);}
    void slider(Graphics& g,Control c,bool big=false){double v=value(c.id);Color accent=c.id==Drive?pink:c.id==Release?gold:cyan;box(g,c.x,c.y,c.w,c.h,Color(255,32,24,48),selected==index(c.id)?accent:edge);label(g,c.title?c.title:wide(registry[index(c.id)].name),c.x+10,c.y+6,big?16.f:12.f,muted,true);label(g,wide(display(c.id,v)),c.x+10,c.y+(big?29:22),big?24.f:15.f,white,true);SolidBrush track(Color(255,70,50,85)),fill(accent);g.FillRectangle(&track,c.x+10,c.y+c.h-(c.h<55?6:13),c.w-20,4.f);g.FillRectangle(&fill,c.x+10,c.y+c.h-(c.h<55?6:13),float((c.w-20)*v),4.f);}
    void paint(HDC dc){Bitmap buffer(rect.getWidth(),rect.getHeight(),PixelFormat32bppARGB);Graphics g(&buffer);g.ScaleTransform(float(sx()),float(sy()));g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);g.Clear(ink);
        LinearGradientBrush header(Point(0,0),Point(width,120),Color(255,65,20,70),ink);g.FillRectangle(&header,0,0,width,112);
        // Glowing trim and brighter accents follow the real output meter, without strobing.
        LinearGradientBrush trim(Point(0,0),Point(width,0),pink,gold);g.FillRectangle(&trim,0,0,width,3);
        label(g,L"DOPAGAKI",26,19,32,Color(255,165,43,111),true);
        label(g,L"DOPAGAKI",24,17,32,white,true);label(g,L"H Y P E R   S Y N T H",28,58,12,gold,true);
        box(g,285,24,390,42,Color(255,38,25,51),gold);int patch=bank?int(SendMessageW(bank,CB_GETCURSEL,0,0)):0;label(g,patch>0?wide(factoryPresets()[size_t(patch-1)].name):(modified?L"CUSTOM / edited patch":L"INIT / Default"),296,34,16,white,true);
        label(g,L"96 SOUNDS  /  LEADS  +  BASS  +  KEYS  +  TEXTURES",286,77,12,muted);label(g,std::to_wstring(int(sx()*100+.5))+L"%",810,77,13,gold,true);
        double level=value(Param(meterID));SolidBrush levelFill(gold);g.FillRectangle(&levelFill,990.f,94.f,float(186*level),4.f);
        for(int k=0;k<3;++k){float x=24.f+k*8;SolidBrush b(k==0?pink:k==1?gold:cyan);g.FillRectangle(&b,x,111.f,4.f,301.f);}
        box(g,24,118,480,294,panel,edge);box(g,524,118,320,294,panel,edge);box(g,864,118,312,294,panel,edge);
        for(int i=0;i<3;++i){Color c=i==0?cyan:i==1?pink:gold;Pen light(Color(BYTE(80+175*level),c.GetR(),c.GetG(),c.GetB()),float(2+2*level));float x=i==0?24.f:i==1?524.f:864.f;g.DrawLine(&light,x,118.f,x+(i==0?480.f:i==1?320.f:312.f),118.f);}
        label(g,L"01  SOURCE",40,130,15,cyan,true);label(g,L"VA  x  WAVETABLE",262,132,13,muted,true);
        box(g,40,166,448,156,Color(255,12,15,29),Color(255,40,79,83));
        const auto& table=WavetableBank::original();std::array<PointF,223> points{};
        for(int layer=5;layer>=0;--layer){double pos=layer?std::clamp(value(WavePosition)+layer*.055,0.,1.):value(WavePosition);for(int x=0;x<223;++x){double phase=x/222.;int shape=int(value(Shape)*3+.5);double va=shape==0?2*phase-1:shape==1?(phase<.5?1.:-1.):shape==2?std::sin(2*pi*phase):(.7*std::sin(2*pi*phase)+.2*std::sin(6*pi*phase));double wave=va+(table.read(phase,pos,1./2048)-va)*value(WaveMix);points[x]={float(42+x*2),float(244-wave*(47+level*8)+layer*3)};}if(layer){Pen p(Color(255,29,62,74));g.DrawLines(&p,points.data(),int(points.size()));}else neon(g,points.data(),int(points.size()),cyan);}
        label(g,L"DRAG  /  FRAME + BLEND",52,177,11,muted);label(g,L"5 VOICES  /  CENTER + STEREO SIDES",52,302,11,cyan,true);
        label(g,L"02  FILTER",540,130,15,pink,true);label(g,L"LOW PASS",733,132,13,muted,true);
        box(g,540,166,288,156,Color(255,24,12,32),edge);
        Pen grid(Color(255,54,31,57));for(int i=1;i<6;++i)g.DrawLine(&grid,540.f+i*48,166.f,540.f+i*48,322.f);
        std::array<PointF,281> response{};for(int x=0;x<281;++x){double ratio=hz(x/280.)/hz(value(Cutoff)),k=2-1.8*value(Resonance);double mag=1/std::sqrt(std::pow(1-ratio*ratio,2)+k*k*ratio*ratio);response[x]={float(544+x),float(212-std::clamp(20*std::log10(std::max(.001,mag)),-45.,15.)*2)};}neon(g,response.data(),int(response.size()),pink);label(g,L"DRAG  /  CUTOFF + RES",552,302,11,muted);
        label(g,L"03  ENVELOPE",880,130,15,gold,true);label(g,L"AMP",1116,132,13,muted,true);box(g,880,166,280,156,Color(255,25,19,23),edge);
        float sustain=float(304-105*value(Sustain));PointF envelope[]={{886,304},{float(890+60*value(Attack)),199},{float(966+72*value(Decay)),sustain},{1080,sustain},{float(1092+65*value(Release)),304}};neon(g,envelope,5,gold);SolidBrush handle(white);for(int i:{1,2,4})g.FillEllipse(&handle,envelope[i].X-4,envelope[i].Y-4,8.f,8.f);
        label(g,L"A",897,177,11,muted);label(g,L"D / S",982,177,11,muted);label(g,L"R",1125,177,11,muted);
        label(g,editPage?L"SOUND DESIGN  /  ADVANCED":L"MAKE IT HIT",24,422,16,white,true);label(g,editPage?L"DRAG TO EDIT  /  SHIFT FOR FINE ADJUSTMENT":L"SIX CONTROLS. STRAIGHT TO THE SOUND.",editPage?400.f:235.f,425,12,muted);
        for(auto c:fixed)slider(g,c);
        if(editPage){for(int i=0;i<18;++i)slider(g,advancedControl(i));}else{
            for(auto c:easy)slider(g,c,true);
            const wchar_t* hints[]={L"Dark / bright",L"Sub weight",L"Unison detune",L"Saturation",L"Ping-pong delay",L"Release time"};for(int i=0;i<6;++i)label(g,hints[i],float(34+i*194),561,13,muted);
            LinearGradientBrush rush(Point(24,600),Point(1176,600),Color(255,105,25,83),Color(255,28,51,62));g.FillRectangle(&rush,24,589,1152,31);label(g,L"R U S H",36,594,14,gold,true);label(g,L"PLAY A CHORD. PUSH THE HEAT. OPEN THE COLOR.",150,596,12,white,true);
            for(int i=0;i<32;++i){SolidBrush b(i<level*32?(i>24?pink:gold):Color(255,64,42,67));g.FillRectangle(&b,float(874+i*9),597.f,5.f,14.f);}
        }
        label(g,L"PLAY",28,647,20,white,true);label(g,L"A W S E D ...",28,679,12,cyan,true);label(g,L"Hold for chords",28,696,11,muted);
        for(bool black:{false,true})for(const auto& k:piano::keys())if(k.black==black){bool down=(held()&(uint64_t(1)<<(k.note-48)))!=0;Color top=down?(black?pink:cyan):(black?Color(255,50,40,66):Color(255,250,244,255)),bottom=down?(black?Color(255,146,28,89):Color(255,86,165,181)):(black?Color(255,12,9,24):Color(255,198,190,213));LinearGradientBrush b(PointF(k.x,k.y),PointF(k.x,k.y+k.h),top,bottom);g.FillRectangle(&b,k.x,k.y,k.w,k.h);Pen outline(ink);g.DrawRectangle(&outline,k.x,k.y,k.w,k.h);if(!black&&k.note%12==0)label(g,L"C"+std::to_wstring(k.note/12-1),k.x+8,k.y+81,12,Color(255,55,40,70),true);}
        label(g,L"DRAG KEYS TO GLIDE  /  COMPUTER KEYS FOR CHORDS  /  DOUBLE-CLICK CONTROL TO RESET",164,762,10,muted);
        Graphics output(dc);output.DrawImage(&buffer,0,0);
    }
    void file(bool save);
    static LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM w,LPARAM l){auto* s=reinterpret_cast<Editor*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_NCCREATE){s=static_cast<Editor*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));}if(!s)return DefWindowProcW(hwnd,msg,w,l);
        int x=int(GET_X_LPARAM(l)/s->sx()),y=int(GET_Y_LPARAM(l)/s->sy());
        switch(msg){
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT p;auto dc=BeginPaint(hwnd,&p);s->paint(dc);EndPaint(hwnd,&p);return 0;}
        case WM_PRINTCLIENT:s->paint(reinterpret_cast<HDC>(w));return 0;
        case WM_TIMER:InvalidateRect(hwnd,nullptr,FALSE);return 0;
        case WM_MEASUREITEM:reinterpret_cast<MEASUREITEMSTRUCT*>(l)->itemHeight=UINT(30*s->sy());return TRUE;
        case WM_DRAWITEM:{auto* item=reinterpret_cast<DRAWITEMSTRUCT*>(l);Graphics g(item->hDC);auto r=item->rcItem;SolidBrush b(item->itemState&ODS_SELECTED?Color(255,81,37,85):Color(255,40,29,55));g.FillRectangle(&b,int(r.left),int(r.top),int(r.right-r.left),int(r.bottom-r.top));wchar_t text[160]{};if(item->CtlID==10){int n=int(item->itemID);if(n>=0)SendMessageW(item->hwndItem,CB_GETLBTEXT,n,reinterpret_cast<LPARAM>(text));}else GetWindowTextW(item->hwndItem,text,160);label(g,text,float(r.left+7*s->sx()),float(r.top+5*s->sy()),float(12*s->sy()),gold,true);return TRUE;}
        case WM_COMMAND:{int id=LOWORD(w);if(id==10&&HIWORD(w)==CBN_SELCHANGE){int n=int(SendMessageW(s->bank,CB_GETCURSEL,0,0));s->remember();s->controller->apply(n>0?factoryPresets()[size_t(n-1)].values:defaults());s->modified=false;}
            else if(id==11)s->file(true);else if(id==12)s->file(false);else if(id==13){s->remember();s->controller->apply(defaults());s->modified=false;SendMessageW(s->bank,CB_SETCURSEL,0,0);}else if(id==14&&s->hasUndo){auto v=s->controller->values();s->controller->apply(s->undo);s->undo=v;s->modified=true;SendMessageW(s->bank,CB_SETCURSEL,0,0);}else if(id==18){s->editPage=!s->editPage;SetWindowTextW(GetDlgItem(hwnd,18),s->editPage?L"EASY":L"EDIT");}else if(id==22||id==23){int n=int(SendMessageW(s->bank,CB_GETCURSEL,0,0));n=n<=0?(id==23?1:96):1+(n-1+(id==23?1:95))%96;s->remember();s->controller->apply(factoryPresets()[size_t(n-1)].values);s->modified=false;SendMessageW(s->bank,CB_SETCURSEL,n,0);}else if(id==20)s->zoom();else if(id==21){s->stopPreview();s->controller->edit(SeqRun,0);}InvalidateRect(hwnd,nullptr,FALSE);return 0;}
        case WM_LBUTTONDOWN:{SetFocus(hwnd);if(x>=285&&x<675&&y>=24&&y<66){SendMessageW(s->bank,CB_SHOWDROPDOWN,TRUE,0);return 0;}int note=piano::hit(float(x),float(y));if(note>=0){s->mouseNote=note;s->sendKeys();SetCapture(hwnd);return 0;}
            int hit=s->hit(x,y);if(hit>=0){s->remember();s->selected=hit;s->dragID=registry[hit].id;s->dragMode=1;s->lastX=x;s->controller->beginEdit(s->dragID);SetCapture(hwnd);return 0;}
            if(x>=40&&x<=488&&y>=166&&y<=322){s->remember();s->dragMode=2;s->controller->beginEdit(WavePosition);s->controller->beginEdit(WaveMix);}
            if(x>=540&&x<=828&&y>=166&&y<=322){s->remember();s->dragMode=3;s->controller->beginEdit(Cutoff);s->controller->beginEdit(Resonance);}
            if(x>=880&&x<=1160&&y>=166&&y<=322){s->remember();s->dragMode=x<960?4:x<1086?5:6;for(auto id:{Attack,Decay,Sustain,Release})s->controller->beginEdit(id);}if(s->dragMode){SetCapture(hwnd);s->dragPoint(x,y);}return 0;}
        case WM_MOUSEMOVE:if(s->dragMode)s->dragPoint(x,y);else if(GetCapture()==hwnd){int note=piano::hit(float(x),float(y));if(note!=s->mouseNote){s->mouseNote=note;s->sendKeys();}}return 0;
        case WM_LBUTTONUP:ReleaseCapture();s->finishDrag();s->mouseNote=-1;s->sendKeys();return 0;
        case WM_CAPTURECHANGED:s->finishDrag();s->mouseNote=-1;s->sendKeys();return 0;
        case WM_KILLFOCUS:ReleaseCapture();s->finishDrag();s->stopPreview();return 0;
        case WM_LBUTTONDBLCLK:{int hit=s->hit(x,y);if(hit>=0){s->remember();s->controller->edit(registry[hit].id,registry[hit].initial);}return 0;}
        case WM_KEYDOWN:case WM_KEYUP:{int key=computerKey(w);if(key>=0){uint64_t bit=uint64_t(1)<<key;if(msg==WM_KEYDOWN)s->keyboardMask|=bit;else s->keyboardMask&=~bit;s->sendKeys();return 0;}if(msg==WM_KEYDOWN&&(w==VK_LEFT||w==VK_RIGHT||w==VK_UP||w==VK_DOWN)){auto& p=registry[s->selected];s->remember();double step=p.steps?1./p.steps:(GetKeyState(VK_SHIFT)<0?.001:.01);s->controller->edit(p.id,s->value(p.id)+((w==VK_RIGHT||w==VK_UP)?step:-step));return 0;}break;}
        }
        return DefWindowProcW(hwnd,msg,w,l);
    }
};
    void Editor::file(bool save){wchar_t path[MAX_PATH]=L"My Dopagaki Patch.dopa";OPENFILENAMEW f{};f.lStructSize=sizeof(f);f.hwndOwner=window;f.lpstrFilter=L"Dopagaki preset (*.dopa)\0*.dopa\0\0";f.lpstrFile=path;f.nMaxFile=MAX_PATH;f.lpstrDefExt=L"dopa";f.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
        if(!(save?GetSaveFileNameW(&f):GetOpenFileNameW(&f)))return;
        try {if(save){std::ofstream s(std::filesystem::path(path),std::ios::binary);s<<encode(controller->values());if(!s)throw 1;}
        else {std::ifstream s(std::filesystem::path(path),std::ios::binary|std::ios::ate);auto size=s.tellg();if(size<0||size>16384)throw 1;s.seekg(0);std::string data(size_t(size),'\0');s.read(data.data(),std::streamsize(size));Values v;if(!s||!decode(data,v))throw 1;remember();controller->apply(v);modified=true;SendMessageW(bank,CB_SETCURSEL,0,0);}}
        catch(...){MessageBoxW(window,L"The preset could not be read or written. Use a valid version 1, 2 or 3 .dopa file.",L"dopagaki_synth",MB_OK|MB_ICONERROR);}
    }

IPlugView* makeEditor(Controller* controller){return new Editor(controller);}
}
