// Loads the actual Release DLL, never the DSP classes directly.
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/gui/iplugview.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/common/memorystream.h"
#include <array>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <cstring>
using namespace Steinberg;
using namespace Steinberg::Vst;
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
// Exercise the host-owned resize handshake instead of resizing only the child HWND.
struct TestFrame final:IPlugFrame {
    int calls=0;
    tresult PLUGIN_API queryInterface(const TUID,void** object) override{if(object)*object=nullptr;return kNoInterface;}
    uint32 PLUGIN_API addRef() override{return 1;}
    uint32 PLUGIN_API release() override{return 1;}
    tresult PLUGIN_API resizeView(IPlugView* view,ViewRect* size) override{++calls;return view->onSize(size);}
};
int wmain(int argc,wchar_t** argv){try{
    check(argc>=3,"usage: dopa_plugin_tests DLL screenshot.png");
    auto module=LoadLibraryW(argv[1]);check(module,"LoadLibrary");
    auto init=reinterpret_cast<bool(*)()>(GetProcAddress(module,"InitDll"));auto exit=reinterpret_cast<bool(*)()>(GetProcAddress(module,"ExitDll"));check(init&&exit&&init(),"InitDll");
    auto factoryFn=reinterpret_cast<IPluginFactory*(PLUGIN_API*)()>(GetProcAddress(module,"GetPluginFactory"));check(factoryFn,"factory export");auto* factory=factoryFn();
    IComponent* component=nullptr;IEditController* controller=nullptr;IAudioProcessor* processor=nullptr;
    for(int32 i=0;i<factory->countClasses();++i){PClassInfo info{};factory->getClassInfo(i,&info);if(std::strcmp(info.category,kVstAudioEffectClass)==0)factory->createInstance(info.cid,IComponent::iid,reinterpret_cast<void**>(&component));}
    check(component,"component factory");HostApplication host;check(component->initialize(&host)==kResultOk,"initialize");
    TUID controllerID{};check(component->getControllerClassId(controllerID)==kResultOk,"controller ID");check(factory->createInstance(controllerID,IEditController::iid,reinterpret_cast<void**>(&controller))==kResultOk,"controller create");check(controller->initialize(&host)==kResultOk,"controller initialize");check(component->queryInterface(IAudioProcessor::iid,reinterpret_cast<void**>(&processor))==kResultOk,"processor query");
    ProcessSetup setup{kRealtime,kSample32,512,48000};check(processor->setupProcessing(setup)==kResultOk,"setup");component->activateBus(kAudio,kOutput,0,true);component->activateBus(kEvent,kInput,0,true);component->setActive(true);processor->setProcessing(true);
    IConnectionPoint* cp=nullptr;IConnectionPoint* cc=nullptr;check(component->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&cp))==kResultOk,"component connection");check(controller->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&cc))==kResultOk,"controller connection");cc->connect(cp);cp->connect(cc);
    MemoryStream state;check(component->getState(&state)==kResultOk&&state.getSize()==552,"state save");
    std::array<float,512> l{},r{};float* buffers[]={l.data(),r.data()};AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=buffers;
    ProcessData data{};data.processMode=kRealtime;data.symbolicSampleSize=kSample32;data.numOutputs=1;data.outputs=&bus;
    EventList events;ParameterChanges changes(45),output(2);data.inputEvents=&events;data.inputParameterChanges=&changes;data.outputParameterChanges=&output;
    auto render=[&](int block){std::vector<float> samples;int total=8192;for(int pos=0;pos<total;){int count=std::min(block,total-pos);events.clear();changes.clearQueue();output.clearQueue();
        if(pos<=73&&pos+count>73){Event e{};e.type=Event::kNoteOnEvent;e.sampleOffset=73-pos;e.noteOn.pitch=60;e.noteOn.velocity=1;e.noteOn.noteId=7;events.addEvent(e);}
        if(pos<=2000&&pos+count>2000){int32 q=0,p=0;changes.addParameterData(105,q)->addPoint(2000-pos,.2,p);}
        if(pos<=5000&&pos+count>5000){Event e{};e.type=Event::kNoteOffEvent;e.sampleOffset=5000-pos;e.noteOff.pitch=60;e.noteOff.noteId=7;events.addEvent(e);}
        data.numSamples=count;check(processor->process(data)==kResultOk,"process");for(int i=0;i<count;++i){check(std::isfinite(l[i])&&std::isfinite(r[i]),"finite");samples.push_back(l[i]);samples.push_back(r[i]);}pos+=count;}
        return samples;};
    auto a=render(512);double energy=0;for(int i=0;i<146;++i)check(a[i]==0,"note offset");for(float v:a)energy+=v*v;check(energy>0,"plugin silent");
    processor->setProcessing(false);component->setActive(false);state.seek(0,IBStream::kIBSeekSet,nullptr);check(component->setState(&state)==kResultOk,"restore");state.seek(0,IBStream::kIBSeekSet,nullptr);check(controller->setComponentState(&state)==kResultOk,"controller restore");check(std::abs(controller->getParamNormalized(105)-.8)<1e-9,"controller value");component->setActive(true);processor->setProcessing(true);auto b=render(127);check(a==b,"block-size/state deterministic audio");
    MemoryStream changed;component->getState(&changed);changed.seek(0,IBStream::kIBSeekSet,nullptr);controller->setComponentState(&changed);check(std::abs(controller->getParamNormalized(105)-.2)<1e-9,"automation persistence");
    processor->setProcessing(false);component->setActive(false);
    {std::array<char,504> bytes{};std::memcpy(bytes.data(),state.getData(),bytes.size());uint32 version=2,count=41;std::memcpy(bytes.data()+4,&version,4);std::memcpy(bytes.data()+8,&count,4);MemoryStream legacy(bytes.data(),bytes.size());check(component->setState(&legacy)==kResultOk,"v2 component migration");legacy.seek(0,IBStream::kIBSeekSet,nullptr);check(controller->setComponentState(&legacy)==kResultOk&&controller->getParamNormalized(141)==0,"v2 table bypass migration");}
    {std::array<char,276> bytes{};std::memcpy(bytes.data(),state.getData(),bytes.size());uint32 version=1,count=22;std::memcpy(bytes.data()+4,&version,4);std::memcpy(bytes.data()+8,&count,4);MemoryStream legacy(bytes.data(),bytes.size());check(component->setState(&legacy)==kResultOk,"v1 component migration");legacy.seek(0,IBStream::kIBSeekSet,nullptr);check(controller->setComponentState(&legacy)==kResultOk&&controller->getParamNormalized(138)==0,"v1 controller migration");}
    auto parent=CreateWindowExW(0,L"STATIC",L"Dopagaki integration",WS_OVERLAPPEDWINDOW,0,0,1120,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);check(parent,"parent window");
    DWORD before=0,after=0;
    for(int n=0;n<25;++n){auto* view=controller->createView(ViewType::kEditor);check(view,"editor create");check(view->attached(parent,kPlatformTypeHWND)==kResultOk,"editor attach");ViewRect rect{};view->getSize(&rect);check(rect.getWidth()==1320&&rect.getHeight()==858,"editor size");auto editor=GetWindow(parent,GW_CHILD);check(editor,"editor HWND");
        // Exercise the real combo -> controller preset path, reset, undo, keyboard and FX switch.
        auto combo=GetDlgItem(editor,10);SendMessageW(combo,CB_SETCURSEL,9,0);SendMessageW(editor,WM_COMMAND,MAKEWPARAM(10,CBN_SELCHANGE),reinterpret_cast<LPARAM>(combo));check(std::abs(controller->getParamNormalized(117)-.25)<1e-9,"preset combo");
        SendMessageW(editor,WM_COMMAND,13,0);check(std::abs(controller->getParamNormalized(117)-.5)<1e-9,"init button");SendMessageW(editor,WM_COMMAND,14,0);check(std::abs(controller->getParamNormalized(117)-.25)<1e-9,"undo button");
        check(view->canResize()==kResultTrue,"editor resizable");
        ViewRect constrained(0,0,800,600);view->checkSizeConstraint(&constrained);check(constrained.getWidth()==960&&constrained.getHeight()==624,"size constraint");
        ViewRect scaled(0,0,1500,975);check(view->onSize(&scaled)==kResultOk,"editor resize");
        TestFrame frame;view->setFrame(&frame);SendMessageW(editor,WM_COMMAND,20,0);view->getSize(&rect);check(frame.calls==1&&rect.getWidth()==1800&&rect.getHeight()==1170,"150 percent host zoom");SendMessageW(editor,WM_COMMAND,20,0);view->getSize(&rect);check(frame.calls==2&&rect.getWidth()==1200,"100 percent host zoom");view->onSize(&scaled);
        auto mouse=[&](UINT message,int x,int y){SendMessageW(editor,message,MK_LBUTTON,MAKELPARAM(int(x*1.25),int(y*1.25)));};
        mouse(WM_LBUTTONDOWN,300,230);mouse(WM_MOUSEMOVE,486,175);check(controller->getParamNormalized(141)>.99&&controller->getParamNormalized(142)>.99,"scaled source canvas edits");mouse(WM_LBUTTONUP,486,175);
        SendMessageW(editor,WM_CAPTURECHANGED,0,0);
        SendMessageW(editor,WM_COMMAND,18,0);double previous=controller->getParamNormalized(143);mouse(WM_LBUTTONDOWN,640,475);mouse(WM_MOUSEMOVE,660,475);mouse(WM_LBUTTONUP,660,475);check(controller->getParamNormalized(143)>previous,"advanced morph control");SendMessageW(editor,WM_COMMAND,18,0);
        if(n==0){SendMessageW(editor,WM_COMMAND,23,0);check(SendMessageW(combo,CB_GETCURSEL,0,0)==1,"next preset");SendMessageW(editor,WM_COMMAND,22,0);check(SendMessageW(combo,CB_GETCURSEL,0,0)==96,"previous preset wraps");}
        if(n==0){component->setActive(true);processor->setProcessing(true);events.clear();changes.clearQueue();output.clearQueue();data.numSamples=512;
            SendMessageW(editor,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(225,900));check(processor->process(data)==kResultOk,"keyboard process");double auditionEnergy=0;for(float v:l)auditionEnergy+=v*v;check(auditionEnergy>1e-5,"GUI keyboard did not reach DSP");SendMessageW(editor,WM_LBUTTONUP,0,MAKELPARAM(225,900));SendMessageW(editor,WM_CAPTURECHANGED,0,0);
            for(int block=0;block<500;++block)processor->process(data);double tail=0;for(float v:l)tail+=v*v;check(tail<1e-8,"GUI keyboard stuck note");
            SendMessageW(editor,WM_KEYDOWN,'A',0);SendMessageW(editor,WM_KEYDOWN,'D',0);SendMessageW(editor,WM_KEYDOWN,'G',0);processor->process(data);double chordEnergy=0;for(float v:l)chordEnergy+=v*v;check(chordEnergy>1e-5,"computer chord did not reach DSP");
            SendMessageW(editor,WM_KEYUP,'D',0);processor->process(data);SendMessageW(editor,WM_KILLFOCUS,0,0);for(int block=0;block<500;++block)processor->process(data);double chordTail=0;for(float v:l)chordTail+=v*v;check(chordTail<1e-8,"focus loss left stuck chord");
processor->setProcessing(false);component->setActive(false);}
        if(n<=2){if(n==1)SendMessageW(editor,WM_COMMAND,18,0);if(n==2){ViewRect compact(0,0,960,624);view->onSize(&compact);}SendMessageW(combo,CB_SETCURSEL,6,0);SendMessageW(editor,WM_COMMAND,MAKEWPARAM(10,CBN_SELCHANGE),reinterpret_cast<LPARAM>(combo));controller->setParamNormalized(1000,.62);HDC dc=GetDC(editor),memory=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,n==2?960:1500,n==2?624:975);auto old=SelectObject(memory,bitmap);SendMessageW(editor,WM_PRINT,reinterpret_cast<WPARAM>(memory),PRF_CLIENT|PRF_CHILDREN|PRF_ERASEBKGND);Gdiplus::GdiplusStartupInput gi;ULONG_PTR token;Gdiplus::GdiplusStartup(&token,&gi,nullptr);{Gdiplus::Bitmap image(bitmap,nullptr);CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0,0,0xf8,0x1e,0xf3,0x2e}};std::wstring path=argv[2];if(n==1)path+=L".advanced.png";if(n==2)path+=L".compact.png";check(image.Save(path.c_str(),&png,nullptr)==Gdiplus::Ok,"screenshot");}Gdiplus::GdiplusShutdown(token);SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(editor,dc);}
        view->setFrame(nullptr);check(view->removed()==kResultOk,"editor remove");view->release();if(n==0)before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    }
    after=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);check(after<=before+2,"GDI leak");DestroyWindow(parent);
    cc->disconnect(cp);cp->disconnect(cc);cc->release();cp->release();controller->terminate();controller->release();processor->release();component->terminate();component->release();factory->release();check(exit(),"ExitDll");FreeLibrary(module);
    std::cout<<"PASS: actual DLL, sample-offset MIDI, automation, state/controller restore, identical 512/127-block audio, 25 editor cycles, preset/init/undo/advanced controls, scaling, piano, GDI objects "<<before<<" -> "<<after<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
