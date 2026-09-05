#pragma once
#include "core/Engine.h"
#include "core/Presets.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include <atomic>

namespace dopa {
static_assert(std::atomic<uint64_t>::is_always_lock_free,"GUI held-key snapshot must be lock-free");
using namespace Steinberg;
using namespace Steinberg::Vst;
inline const FUID processorUID(0xAC05D923,0x27D74A13,0x9FF42E21,0xA5D081B9);
inline const FUID controllerUID(0xE4157E65,0xD84344D9,0xB74814CC,0x26090501);
inline constexpr ParamID meterID=1000;
inline constexpr ParamID playheadID=1001;
class Processor final:public AudioEffect {
public:
    Processor(){setControllerClass(controllerUID);}
    static FUnknown* create(void*){return static_cast<IAudioProcessor*>(new Processor);}
    tresult PLUGIN_API initialize(FUnknown*) override;
    tresult PLUGIN_API setupProcessing(ProcessSetup&) override;
    tresult PLUGIN_API setActive(TBool) override;
    tresult PLUGIN_API setProcessing(TBool) override;
    tresult PLUGIN_API canProcessSampleSize(int32 size) override {return size==kSample32||size==kSample64?kResultTrue:kResultFalse;}
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement*,int32,SpeakerArrangement*,int32) override;
    tresult PLUGIN_API process(ProcessData&) override;
    tresult PLUGIN_API getState(IBStream*) override;
    tresult PLUGIN_API setState(IBStream*) override;
    tresult PLUGIN_API notify(IMessage*) override;
    uint32 PLUGIN_API getTailSamples() override {return uint32(engine.sampleRate*10);}
    Engine engine;
    std::atomic<int> auditionNote{-1};
    std::atomic<uint64_t> auditionMask{0};
};
class Controller final:public EditController,public IMidiMapping {
public:
    static FUnknown* create(void*){return static_cast<IEditController*>(new Controller);}
    tresult PLUGIN_API initialize(FUnknown*) override;
    tresult PLUGIN_API setComponentState(IBStream*) override;
    IPlugView* PLUGIN_API createView(FIDString) override;
    tresult PLUGIN_API getMidiControllerAssignment(int32 bus,int16 channel,CtrlNumber cc,ParamID& id) override;
    tresult PLUGIN_API getParamStringByValue(ParamID,ParamValue,String128) override;
    void edit(ParamID id,double v){v=clean(v);beginEdit(id);setParamNormalized(id,v);performEdit(id,v);endEdit(id);}
    Values values(){Values v;for(size_t i=0;i<v.size();++i)v[i]=getParamNormalized(registry[i].id);return v;}
    void apply(const Values& v){for(size_t i=0;i<v.size();++i)edit(registry[i].id,v[i]);}
    void audition(int note);
    void auditionKeys(uint64_t mask);
    OBJ_METHODS(Controller,EditController)
    DEFINE_INTERFACES
        DEF_INTERFACE(IMidiMapping)
    END_DEFINE_INTERFACES(EditController)
    REFCOUNT_METHODS(EditController)
};
IPlugView* makeEditor(Controller*);
}
