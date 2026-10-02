// Standalone audio tests. Including this implementation deliberately exposes only
// its translation-unit parser helpers to CPU tests; do not also link Audio.cpp.
// Native checks play a silent PCM waveform; the optional MP3 check uses gain zero.
// Run via tools/test_audio_runtime.ps1; temporary outputs stay in generated/.
#include "../DirectX/engine/audio/Audio.cpp"
#include <cassert>
#include <chrono>
#include <iostream>

using namespace cg2;

void Put16(std::vector<BYTE>& data,size_t index,uint16_t value) {
    data[index]=static_cast<BYTE>(value); data[index+1]=static_cast<BYTE>(value>>8);
}
void Put32(std::vector<BYTE>& data,size_t index,uint32_t value) {
    for(size_t n=0;n<4;++n) data[index+n]=static_cast<BYTE>(value>>(n*8));
}
std::vector<BYTE> GoodWave(uint16_t channels=1,uint32_t rate=48000) {
    const uint32_t pcmSize=rate/10*channels*2;
    std::vector<BYTE> data(44+pcmSize,0);
    std::memcpy(data.data(),"RIFF",4); Put32(data,4,static_cast<uint32_t>(data.size()-8));
    std::memcpy(data.data()+8,"WAVEfmt ",8); Put32(data,16,16);
    Put16(data,20,1); Put16(data,22,channels); Put32(data,24,rate);
    Put32(data,28,rate*channels*2); Put16(data,32,channels*2); Put16(data,34,16);
    std::memcpy(data.data()+36,"data",4); Put32(data,40,pcmSize);
    return data; // silence: native smoke tests do not produce audible output
}
bool Parse(const std::vector<BYTE>& data) {
    WAVEFORMATEX format{}; size_t offset=0,size=0;
    return ParsePcm16Wave(data,format,offset,size);
}
void Write(const std::filesystem::path& path,const std::vector<BYTE>& data) {
    std::ofstream stream(path,std::ios::binary|std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));
    assert(stream.good());
}
int wmain(int argc, wchar_t** argv) {
    const auto good=GoodWave();
    assert(Parse(good) && Parse(GoodWave(2)) && Parse(GoodWave(1,8000)) && Parse(GoodWave(2,192000)));
    WAVEFORMATEX format{}; size_t offset=0,size=0;
    assert(ParsePcm16Wave(good,format,offset,size));
    assert(offset==44 && size==9600 && format.nBlockAlign==2 && format.nAvgBytesPerSec==96000);
    auto bad=good; bad[0]='X'; assert(!Parse(bad));
    bad=good; bad[8]='X'; assert(!Parse(bad));
    bad=good; Put32(bad,4,0xffffffffu); assert(!Parse(bad));
    bad=good; Put32(bad,16,0xffffffffu); assert(!Parse(bad));
    bad=good; Put32(bad,40,0xffffffffu); assert(!Parse(bad));
    bad=good; Put16(bad,20,3); assert(!Parse(bad)); // unsupported IEEE float
    bad=good; Put16(bad,34,8); assert(!Parse(bad));
    bad=good; Put16(bad,22,0); assert(!Parse(bad));
    bad=good; Put16(bad,22,3); assert(!Parse(bad));
    bad=good; Put32(bad,24,0); assert(!Parse(bad));
    bad=good; Put32(bad,28,1); assert(!Parse(bad));
    bad=good; Put16(bad,32,4); assert(!Parse(bad));
    bad=good; bad.resize(43); assert(!Parse(bad));
    bad=good; bad.pop_back(); Put32(bad,4,static_cast<uint32_t>(bad.size()-8)); assert(!Parse(bad));
    bad=good; Put32(bad,40,9599); assert(!Parse(bad));
    bad=good; bad.resize(44); Put32(bad,4,36); Put32(bad,40,0); assert(!Parse(bad));
    bad=good; bad.insert(bad.end(),good.begin()+12,good.begin()+36);
    Put32(bad,4,static_cast<uint32_t>(bad.size()-8)); assert(!Parse(bad)); // duplicate fmt
    bad=good; bad.insert(bad.end(),good.begin()+36,good.end());
    Put32(bad,4,static_cast<uint32_t>(bad.size()-8)); assert(!Parse(bad)); // duplicate data
    auto metadata=good; const BYTE junk[]={ 'J','U','N','K',1,0,0,0,7,0 };
    metadata.insert(metadata.begin()+12,std::begin(junk),std::end(junk));
    Put32(metadata,4,static_cast<uint32_t>(metadata.size()-8)); assert(Parse(metadata));
    auto extended=good; extended.insert(extended.begin()+36,{0,0});
    Put32(extended,16,18); Put32(extended,4,static_cast<uint32_t>(extended.size()-8)); assert(Parse(extended));
    extended[36]=1; assert(!Parse(extended));
    // Malformed parser input cannot partly overwrite a caller's decoded output.
    format.nChannels=7; offset=99; size=88;
    assert(!ParsePcm16Wave(bad,format,offset,size));
    assert(format.nChannels==7 && offset==99 && size==88);
    std::vector<BYTE> tooLarge(Audio::kMaxPcmWaveBytes+1,0); assert(!Parse(tooLarge));
    const float nan=std::numeric_limits<float>::quiet_NaN();
    assert(SafeGain(-1)==0 && SafeGain(2)==1 && SafeGain(nan)==0);
    assert(SafePitch(-1)==.25f && SafePitch(5)==2 && SafePitch(nan)==1);
    auto* audio=Audio::GetInstance();
    const auto path=std::filesystem::current_path()/L"無音確認.wav";
    const auto invalidPath=std::filesystem::current_path()/L"破損.wav";
    Write(path,good); Write(invalidPath,bad);
    assert(!audio->IsReady());
    assert(!audio->TryLoadPcmWave(L"test",path.wstring(),1));
    assert(!audio->TryPlayAudio(L"test").IsValid());
    assert(!audio->IsVoicePlaying({})); audio->StopAudio(Audio::VoiceHandle{});
    assert(!audio->SetPitch({},1)); audio->SetVolume({},nan); audio->UnloadAudio(L"missing");
    std::cout<<"PASS CPU: PCM16 RIFF formats, chunk/overflow/alignment guards, transactional outputs, gain/pitch, uninitialized API\n";
    assert(SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)));
    audio->Initialize();
    if(!audio->IsReady()) { std::cout<<"SKIP native voice checks: audio endpoint unavailable; app remains alive\n"; return 0; }
    assert(audio->TryLoadPcmWave(L"test",path.wstring(),1));
    const auto first=audio->TryPlayAudio(L"test",true,0,1);
    assert(first.IsValid() && audio->IsVoicePlaying(first));
    assert(!audio->TryLoadPcmWave(L"test",invalidPath.wstring(),1) && audio->IsVoicePlaying(first));
    assert(!audio->TryLoadPcmWave(L"test",L"does-not-exist.wav",1) && audio->IsVoicePlaying(first));
    assert(!audio->TryLoadPcmWave(L"test",path.wstring(),0) && audio->IsVoicePlaying(first));
    assert(!audio->TryLoadPcmWave(L"test",path.wstring(),17) && audio->IsVoicePlaying(first));
    const auto reused=audio->TryPlayAudio(L"test",true,0,1);
    assert(reused.IsValid() && reused.generation!=first.generation && !audio->IsVoicePlaying(first));
    audio->StopAudio(first); assert(audio->IsVoicePlaying(reused));
    assert(audio->SetPitch(reused,.75f) && audio->SetPitch(reused,nan));
    audio->SetVolume(reused,nan); audio->PauseAudio(reused); assert(!audio->IsVoicePlaying(reused));
    audio->ResumeAudio(reused); assert(audio->IsVoicePlaying(reused));
    audio->StopAudio(reused); assert(!audio->IsVoicePlaying(reused));
    const auto replaced=audio->TryPlayAudio(L"test",true,0,1);
    assert(audio->TryLoadPcmWave(L"test",path.wstring(),2));
    assert(!audio->IsVoicePlaying(replaced));
    const auto a=audio->TryPlayAudio(L"test",true,0,1),b=audio->TryPlayAudio(L"test",true,0,1);
    assert(a.IsValid() && b.IsValid() && a.voiceIndex!=b.voiceIndex);
    assert(audio->IsVoicePlaying(a) && audio->IsVoicePlaying(b));
    audio->StopAudio(L"test"); assert(!audio->IsVoicePlaying(a) && !audio->IsVoicePlaying(b));
    audio->UnloadAudio(L"test"); assert(!audio->TryPlayAudio(L"test").IsValid());
    audio->UnloadAudio(L"test");
    if(argc>1) {
        audio->LoadAudio(L"legacy_mp3",argv[1],5);
        const auto legacy=audio->PlayAudioSE(L"legacy_mp3",0);
        assert(legacy.IsValid());
        audio->StopAudio(legacy); audio->UnloadAudio(L"legacy_mp3");
        std::cout<<"PASS legacy: Media Foundation MP3 decode and PlayAudioSE handle (gain zero)\n";
    } else std::cout<<"SKIP legacy MP3 check: no asset path argument\n";
    // Voice limits constrain reserved source voices, including idle banks.
    for(size_t bank=0;bank<Audio::kMaxLoadedVoices/Audio::kMaxVoicesPerSound;++bank)
        assert(audio->TryLoadPcmWave(L"cap_"+std::to_wstring(bank),path.wstring(),Audio::kMaxVoicesPerSound));
    const auto atCap=audio->TryPlayAudio(L"cap_0",true,0,1);
    assert(atCap.IsValid() && audio->IsVoicePlaying(atCap));
    assert(!audio->TryLoadPcmWave(L"over_cap",path.wstring(),1) && audio->IsVoicePlaying(atCap));
    assert(audio->TryLoadPcmWave(L"cap_0",path.wstring(),Audio::kMaxVoicesPerSound));
    assert(!audio->IsVoicePlaying(atCap));
    for(size_t bank=0;bank<Audio::kMaxLoadedVoices/Audio::kMaxVoicesPerSound;++bank)
        audio->UnloadAudio(L"cap_"+std::to_wstring(bank));
    std::cout<<"PASS limits: 16 voices per bank / 128 loaded total; failed over-cap load retains old playback\n";
    std::cout<<"PASS native (silent): looping, gain/pitch, stop/pause/resume, stale handles, pool rotation, failed/successful replacement, scoped unload\n";
}

