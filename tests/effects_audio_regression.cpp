// Execute the real EFFECTS HazeProcessor and VongonInspiredProcessor
// as an off-line 48-kHz stream. Run before spending any APK build minutes.
// Built with tests/dsp_juce_shim/JuceHeader.h (only JUCE primitives mocked).
#include "../Source/HazeProcessor.h"
#include "../Source/VongonInspiredProcessor.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

constexpr int sampleRate = 48000;
constexpr int blockSize = 128;
constexpr double pi = 3.14159265358979323846;
struct Results {
    std::vector<float> mono;
    double totalRms = 0, tailRms = 0;
    float largestStep = 0;
    int dropoutSamples = 0, longestZeroRun = 0;
    int nonfinite = 0;
};
static float sampleInput(int t) {
    const double time = (double)t / sampleRate;
    // Harmonic-rich, sustained guitar-like test signal.
    return (float) (0.13 * std::sin(2*pi*220*time)
                  + 0.06 * std::sin(2*pi*440*time)
                  + 0.03 * std::sin(2*pi*660*time));
}
static void appendBlock(Results& r,juce::AudioBuffer<float>& b) {
    for(int i=0;i<b.getNumSamples();++i) {
        r.mono.push_back(b.getSample(0,i));
    }
}
static void wav(const std::string& filename,const std::vector<float>& data) {
    std::ofstream f(filename,std::ios::binary);
    if(!f)throw std::runtime_error("Cannot write "+filename);
    auto w16=[&](uint16_t n){f.put((char)(n&255));f.put((char)((n>>8)&255));};
    auto w32=[&](uint32_t n){w16((uint16_t)(n&65535));w16((uint16_t)(n>>16));};
    const uint32_t bytes=(uint32_t)(data.size()*2);
    f.write("RIFF",4);w32(36+bytes);f.write("WAVEfmt ",8);
    w32(16);w16(1);w16(1);w32(sampleRate);w32(sampleRate*2);w16(2);w16(16);
    f.write("data",4);w32(bytes);
    for(const auto x:data) {
        const auto y=(int)std::lround(
            std::clamp(x,-1.0f,1.0f)*32767.0f);
        w16((uint16_t)(int16_t)y);
    }
}
static Results inspect(Results r,const std::string& mode) {
    if(r.mono.empty())throw std::runtime_error(mode+" no samples");
    double energy=0,tailEnergy=0;
    int tailCount=0,consecutive=0;
    // Measure after a 2-second initial delay/history warmup.
    const size_t warmup=(size_t)sampleRate*2;
    for(size_t i=0;i<r.mono.size();++i){
        const float x=r.mono[i];
        if(!std::isfinite(x)){++r.nonfinite;continue;}
        energy+=(double)x*x;
        if(i>0)r.largestStep=std::max(
            r.largestStep,std::abs(x-r.mono[i-1]));
        if(i>=warmup){
            tailEnergy+=(double)x*x;
            ++tailCount;
            if(std::abs(x)<0.00001f){
                ++r.dropoutSamples;++consecutive;
                r.longestZeroRun=std::max(r.longestZeroRun,consecutive);
            }else consecutive=0;
        }
    }
    r.totalRms=std::sqrt(energy/(double)r.mono.size());
    r.tailRms=tailCount?std::sqrt(tailEnergy/(double)tailCount):0;
    std::cout<<mode<<" tailRms="<<r.tailRms
             <<" largestStep="<<r.largestStep
             <<" longestZeroMs="<<1000.0*r.longestZeroRun/sampleRate
             <<" nonfinite="<<r.nonfinite<<"\n";
    if(r.nonfinite)throw std::runtime_error(mode+" NaN/Inf in output");
    if(r.tailRms<0.0001)
        throw std::runtime_error(mode+" sustained input produced inaudible output");
    if(r.longestZeroRun>sampleRate*0.13)
        throw std::runtime_error(mode+" dropout >130ms on steady source");
    if(r.largestStep>0.65f)
        throw std::runtime_error(mode+" abrupt step >0.65 FS");
    return r;
}
static Results hazeTest() {
    chordfx::HazeProcessor dsp;
    dsp.prepare(sampleRate);
    chordfx::HazeProcessor::Params p;
    p.mix=0.85f;p.time=0.14f;p.haze=0.5f;p.repeat=0.60f;
    p.filter=0.5f;p.mod=0.5f;p.reverb=0.0f;p.loops=0;
    p.speed=1;p.bypass=false;p.echo=false;p.og=false;
    dsp.setParams(p);
    Results r;
    juce::AudioBuffer<float> b(2,blockSize);
    for(int t=0;t<sampleRate*8;t+=blockSize) {
        for(int i=0;i<blockSize;++i) {
            float x=sampleInput(t+i);
            b.setSample(0,i,x);b.setSample(1,i,x);
        }
        if(t==sampleRate*3){p.time=0.42f;p.repeat=0.82f;dsp.setParams(p);}
        if(t==sampleRate*5){p.time=0.14f;p.repeat=0.45f;dsp.setParams(p);}
        dsp.captureBlock(b);
        dsp.renderBlock(b);
        appendBlock(r,b);
    }
    return r;
}
static Results polyTest() {
    VongonInspiredProcessor dsp;
    dsp.prepare(sampleRate);
    // Force shortest 85-ms time to make clear that no direct signal passes.
    dsp.setDirectCc(VongonInspiredProcessor::polyphrase,8,0);
    dsp.setDirectCc(VongonInspiredProcessor::polyphrase,7,127);
    dsp.setDirectCc(VongonInspiredProcessor::polyphrase,1,72);
    dsp.setDirectCc(VongonInspiredProcessor::polyphrase,3,64);
    dsp.setDirectCc(VongonInspiredProcessor::polyphrase,17,127);
    Results r;
    juce::AudioBuffer<float> b(2,blockSize);
    for(int t=0;t<sampleRate*8;t+=blockSize) {
        for(int i=0;i<blockSize;++i) {
            const float x=sampleInput(t+i);
            b.setSample(0,i,x);b.setSample(1,i,x);
        }
        if(t==sampleRate*3)
            dsp.setDirectCc(VongonInspiredProcessor::polyphrase,8,70);
        if(t==sampleRate*5)
            dsp.setDirectCc(VongonInspiredProcessor::polyphrase,8,0);
        dsp.process(b,VongonInspiredProcessor::polyphrase,0.2f,0.63f,false);
        appendBlock(r,b);
    }
    return r;
}
static Results roseTest() {
    VongonInspiredProcessor dsp;
    dsp.prepare(sampleRate);
    dsp.setDirectCc(VongonInspiredProcessor::rosetones,80,127);
    dsp.setDirectCc(VongonInspiredProcessor::rosetones,85,126);
    dsp.setDirectCc(VongonInspiredProcessor::rosetones,86,120);
    dsp.setDirectCc(VongonInspiredProcessor::rosetones,81,127);
    Results r;
    juce::AudioBuffer<float> b(2,blockSize);
    for(int t=0;t<sampleRate*6;t+=blockSize) {
        for(int i=0;i<blockSize;++i) {
            const float x=sampleInput(t+i);
            b.setSample(0,i,x);b.setSample(1,i,x);
        }
        dsp.process(b,VongonInspiredProcessor::rosetones,0.5f,0.98f,false);
        appendBlock(r,b);
    }
    return r;
}
int main() {
    try {
        std::filesystem::create_directories("audio-regression");
        const auto haze=inspect(hazeTest(),"EUREKA");
        wav("audio-regression/eureka.wav",haze.mono);
        const auto poly=inspect(polyTest(),"POLYPHRASE");
        wav("audio-regression/polyphrase.wav",poly.mono);
        const auto rose=inspect(roseTest(),"ROSETONES-upper-XY");
        // Regress every quadrant of the previously silent upper XY half.
        for(float y : {0.55f,0.75f,0.95f}) {
            for(float x : {0.12f,0.50f,0.88f}) {
                VongonInspiredProcessor voice;
                voice.prepare(sampleRate);
                Results probe;
                juce::AudioBuffer<float> b(2,blockSize);
                for(int t=0;t<sampleRate*3;t+=blockSize) {
                    for(int i=0;i<blockSize;++i) {
                        const float input=sampleInput(t+i);
                        b.setSample(0,i,input);b.setSample(1,i,input);
                    }
                    voice.process(b,VongonInspiredProcessor::rosetones,x,y,false);
                    appendBlock(probe,b);
                }
                inspect(std::move(probe),"ROSETONES XY "+
                    std::to_string(x)+","+std::to_string(y));
            }
        }
        wav("audio-regression/rosetones-upper.wav",rose.mono);
        std::ofstream log("audio-regression/metrics.csv");
        log<<"mode,tail_rms,largest_step,longest_zero_samples,nonfinite\n";
        const auto writeMetrics=[&](const char* name,const Results& r) {
            log<<name<<","<<r.tailRms<<","<<r.largestStep<<","
               <<r.longestZeroRun<<","<<r.nonfinite<<"\n";
        };
        writeMetrics("EUREKA",haze);
        writeMetrics("POLYPHRASE",poly);
        writeMetrics("ROSETONES",rose);
        std::cout<<"Actual DSP offline regression PASSED\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"DSP audio regression FAILED: "<<error.what()<<"\n";
        return 1;
    }
}
