#include "Plugin.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "public.sdk/source/main/pluginfactory.h"
#include <cstring>

namespace dopa {
static bool readState(IBStream* stream,Values& result) {
    if(!stream)return false;
    IBStreamer s(stream,kLittleEndian);uint32 magic=0,version=0,count=0;
    if(!s.readInt32u(magic)||magic!=0x4450474B||!s.readInt32u(version)||(version<1||version>3)||!s.readInt32u(count)||count!=(version==1?22:version==2?41:parameterCount))return false;
    Values v=defaults();std::array<bool,parameterCount> seen{};
    for(uint32 i=0;i<count;++i){uint32 id;double n;if(!s.readInt32u(id)||!s.readDouble(n)||index(id)<0||!std::isfinite(n)||n<0||n>1||seen[index(id)])return false;seen[index(id)]=true;v[index(id)]=n;}
    for(uint32 i=0;i<count;++i)if(!seen[i])return false;
    result=v;return true;
}
tresult PLUGIN_API Processor::initialize(FUnknown* context){auto r=AudioEffect::initialize(context);if(r!=kResultOk)return r;addEventInput(STR16("MIDI"),16);addAudioOutput(STR16("Stereo"),SpeakerArr::kStereo);return kResultOk;}
tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup){if(!std::isfinite(setup.sampleRate)||setup.sampleRate<1000||setup.sampleRate>2000000)return kResultFalse;try{engine.prepare(setup.sampleRate);}catch(...){return kOutOfMemory;}return AudioEffect::setupProcessing(setup);}
tresult PLUGIN_API Processor::setActive(TBool state){engine.reset();return AudioEffect::setActive(state);}
tresult PLUGIN_API Processor::setProcessing(TBool state){engine.reset();return AudioEffect::setProcessing(state);}
tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement*,int32 n,SpeakerArrangement* outs,int32 m){if(n!=0||m!=1||!outs||(outs[0]!=SpeakerArr::kStereo&&outs[0]!=SpeakerArr::kMono))return kResultFalse;getAudioOutput(0)->setArrangement(outs[0]);return kResultTrue;}
tresult PLUGIN_API Processor::getState(IBStream* stream){if(!stream)return kInvalidArgument;IBStreamer s(stream,kLittleEndian);if(!s.writeInt32u(0x4450474B)||!s.writeInt32u(3)||!s.writeInt32u(uint32(parameterCount)))return kResultFalse;for(size_t i=0;i<parameterCount;++i)if(!s.writeInt32u(registry[i].id)||!s.writeDouble(engine.target[i]))return kResultFalse;return kResultOk;}
tresult PLUGIN_API Processor::setState(IBStream* stream){Values v;if(!readState(stream,v))return kResultFalse;engine.target=v;return kResultOk;}
tresult PLUGIN_API Processor::process(ProcessData& data) {
    if(data.symbolicSampleSize!=kSample32&&data.symbolicSampleSize!=kSample64)return kResultFalse;
    // Fixed cursor storage: no allocation, locks, I/O, or UI calls in process.
    std::array<IParamValueQueue*,parameterCount> queues{};
    std::array<int32,parameterCount> cursor{};
    if(data.inputParameterChanges)for(int32 q=0;q<data.inputParameterChanges->getParameterCount();++q){auto* queue=data.inputParameterChanges->getParameterData(q);if(queue&&index(queue->getParameterId())>=0)queues[index(queue->getParameterId())]=queue;}
    auto changes=[&](int32 sample){for(size_t p=0;p<parameterCount;++p)if(auto* q=queues[p]){int32 offset;double value;while(cursor[p]<q->getPointCount()&&q->getPoint(cursor[p],offset,value)==kResultTrue&&offset<=sample){engine.set(registry[p].id,value);++cursor[p];}}};
    if(data.numSamples==0){changes(0);return kResultOk;}
    engine.audition(auditionNote.load(std::memory_order_relaxed));
    engine.auditionKeys(auditionMask.load(std::memory_order_relaxed));
    int32 eventIndex=0,eventCount=data.inputEvents?data.inputEvents->getEventCount():0;Event event{};
    bool have=eventCount>0&&data.inputEvents->getEvent(0,event)==kResultOk;
    bool silent=true;
    for(int32 sample=0;sample<data.numSamples;++sample){
        changes(sample);
        while(have&&event.sampleOffset<=sample){
            if(event.type==Event::kNoteOnEvent)engine.noteOn(event.noteOn.pitch,event.noteOn.velocity,event.noteOn.noteId,event.noteOn.channel);
            if(event.type==Event::kNoteOffEvent)engine.noteOff(event.noteOff.pitch,event.noteOff.noteId,event.noteOff.channel);
            have=++eventIndex<eventCount&&data.inputEvents->getEvent(eventIndex,event)==kResultOk;
        }
        auto frame=engine.tick();silent &= frame[0]==0&&frame[1]==0;
        if(data.numOutputs>0&&data.outputs){auto& out=data.outputs[0];for(int32 c=0;c<out.numChannels;++c){double value=out.numChannels==1?(frame[0]+frame[1])*.5:frame[c%2];if(data.symbolicSampleSize==kSample32){if(out.channelBuffers32&&out.channelBuffers32[c])out.channelBuffers32[c][sample]=float(value);}else if(out.channelBuffers64&&out.channelBuffers64[c])out.channelBuffers64[c][sample]=value;}}
    }
    if(data.numOutputs>0&&data.outputs)data.outputs[0].silenceFlags=silent?((uint64(1)<<data.outputs[0].numChannels)-1):0;
    if(data.outputParameterChanges){int32 q=0;if(auto* queue=data.outputParameterChanges->addParameterData(meterID,q)){int32 point;queue->addPoint(data.numSamples-1,engine.peak,point);}}
    if(data.outputParameterChanges){int32 q=0;if(auto* queue=data.outputParameterChanges->addParameterData(playheadID,q)){int32 point;queue->addPoint(data.numSamples-1,(engine.sequenceStep+1)/16.,point);}}
    return kResultOk;
}
tresult PLUGIN_API Controller::initialize(FUnknown* context){auto r=EditController::initialize(context);if(r!=kResultOk)return r;for(auto& p:registry){String128 title{};UString(title,128).fromAscii(p.name);parameters.addParameter(title,nullptr,p.steps,p.initial,ParameterInfo::kCanAutomate,p.id);}parameters.addParameter(STR16("Output activity"),nullptr,0,0,ParameterInfo::kIsReadOnly,meterID);parameters.addParameter(STR16("Sequence position"),nullptr,16,0,ParameterInfo::kIsReadOnly,playheadID);return kResultOk;}
void Controller::audition(int note){if(auto message=owned(allocateMessage())){message->setMessageID("dopa.audition");message->getAttributes()->setInt("note",note);sendMessage(message);}}
void Controller::auditionKeys(uint64_t mask){if(auto message=owned(allocateMessage())){message->setMessageID("dopa.keys");message->getAttributes()->setInt("mask",int64(mask));sendMessage(message);}}
tresult PLUGIN_API Processor::notify(IMessage* message){if(message&&message->getMessageID()&&std::strcmp(message->getMessageID(),"dopa.keys")==0){int64 mask=0;if(message->getAttributes()->getInt("mask",mask)!=kResultOk||mask<0||uint64_t(mask)>=(uint64_t(1)<<37))return kInvalidArgument;auditionMask.store(uint64_t(mask),std::memory_order_relaxed);return kResultOk;}if(message&&message->getMessageID()&&std::strcmp(message->getMessageID(),"dopa.audition")==0){int64 note=-1;if(message->getAttributes()->getInt("note",note)!=kResultOk||note < -1||note>127)return kInvalidArgument;auditionNote.store(int(note),std::memory_order_relaxed);return kResultOk;}return AudioEffect::notify(message);}
tresult PLUGIN_API Controller::setComponentState(IBStream* stream){Values v;if(!readState(stream,v))return kResultFalse;for(size_t i=0;i<v.size();++i)setParamNormalized(registry[i].id,v[i]);return kResultOk;}
tresult PLUGIN_API Controller::getMidiControllerAssignment(int32 bus,int16 channel,CtrlNumber cc,ParamID& id){if(bus!=0||channel<0||channel>15)return kResultFalse;if(cc==kPitchBend){id=Bend;return kResultTrue;}if(cc==kCtrlSustainOnOff){id=Pedal;return kResultTrue;}if(cc==kCtrlModWheel){id=LfoDepth;return kResultTrue;}return kResultFalse;}
tresult PLUGIN_API Controller::getParamStringByValue(ParamID id,ParamValue v,String128 text){if(index(id)<0)return EditController::getParamStringByValue(id,v,text);UString(text,128).fromAscii(display(Param(id),clean(v)).c_str());return kResultOk;}
IPlugView* PLUGIN_API Controller::createView(FIDString name){return name&&std::strcmp(name,ViewType::kEditor)==0?makeEditor(this):nullptr;}
}
using namespace Steinberg;
using namespace Steinberg::Vst;
BEGIN_FACTORY_DEF("dopagaki_synth contributors","","" )
DEF_CLASS2(INLINE_UID_FROM_FUID(dopa::processorUID),PClassInfo::kManyInstances,kVstAudioEffectClass,"dopagaki_synth",Vst::kDistributable,"Instrument|Synth","0.4.0",kVstVersionString,dopa::Processor::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(dopa::controllerUID),PClassInfo::kManyInstances,kVstComponentControllerClass,"dopagaki_synth Controller",0,"","0.4.0",kVstVersionString,dopa::Controller::create)
END_FACTORY



