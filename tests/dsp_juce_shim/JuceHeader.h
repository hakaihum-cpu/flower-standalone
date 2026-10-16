#pragma once
// Lightweight native JUCE API adapter ONLY for compiled DSP regression tests.
// The real EFFECTS DSP headers are included unchanged by the tests.
// This is not the Android audio backend and does not model device underruns.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
namespace juce {
template <typename T> struct MathConstants {
    static constexpr T pi = (T)3.1415926535897932384626433832795;
    static constexpr T twoPi = (T)6.283185307179586476925286766559;
};
// Match JUCE's single-Type template deductions. The former
// common_type shim wrongly accepted mixed float/double jlimit arguments
// and allowed CircleCI #195 to pass the native DSP preflight.
template <typename T>
constexpr T jmin(T a,T b) {return std::min(a,b);}
template <typename T>
constexpr T jmax(T a,T b) {return std::max(a,b);}
template <typename T>
constexpr T jlimit(T low,T high,T value) {
    return std::clamp(value,low,high);
}
template <typename A,typename B>
constexpr bool isPositiveAndBelow(A value,B limit) {return value>=0&&value<limit;}
inline int roundToInt(double x) {return (int)std::lround(x);}
inline int roundToInt(float x) {return (int)std::lround(x);}
template<typename T> class AudioBuffer {
    std::vector<std::vector<T>> data;
    int samples=0;
public:
    AudioBuffer()=default;
    AudioBuffer(int channels,int frames) {setSize(channels,frames);}
    void setSize(int channels,int frames,bool=false,bool=false,bool=false) {
        data.resize((size_t)std::max(0,channels));
        for(auto& d:data)d.resize((size_t)std::max(0,frames),T{});
        samples=std::max(0,frames);
    }
    int getNumChannels() const {return (int)data.size();}
    int getNumSamples() const {return samples;}
    void clear() {for(auto& d:data)std::fill(d.begin(),d.end(),T{});}
    T getSample(int channel,int index) const {return data.at((size_t)channel).at((size_t)index);}
    void setSample(int channel,int index,T sample) {data.at((size_t)channel).at((size_t)index)=sample;}
    T* getWritePointer(int channel) {return data.at((size_t)channel).data();}
    const T* getReadPointer(int channel) const {return data.at((size_t)channel).data();}
};
class Reverb {
public:
    struct Parameters {
        float roomSize=0.5f,damping=0.5f,wetLevel=0.33f,dryLevel=0.4f,width=1.0f,freezeMode=0.0f;
    };
    void setSampleRate(double) {}
    void reset() {}
    void setParameters(const Parameters& p) {params=p;}
    void processMono(float* left,int count) const {
        for(int i=0;i<count;++i)left[i]*=params.dryLevel;
    }
    void processStereo(float* left,float* right,int count) const {
        for(int i=0;i<count;++i){left[i]*=params.dryLevel;right[i]*=params.dryLevel;}
    }
private:Parameters params;
};
class String {
public:
    std::string s;
    String()=default;
    String(const char* x):s(x) {}
    explicit String(std::string x):s(std::move(x)) {}
    static String toHexString(int value) {
        std::ostringstream out;out<<std::hex<<value;return String(out.str());
    }
    String paddedLeft(char c,int count) const {
        return String(std::string((size_t)std::max(0,count-(int)s.size()),c)+s);
    }
    String substring(int a,int b) const {
        a=std::clamp(a,0,(int)s.size());b=std::clamp(b,a,(int)s.size());
        return String(s.substr((size_t)a,(size_t)(b-a)));
    }
    int getHexValue32() const {return (int)std::strtol(s.c_str(),nullptr,16);}
    int length() const {return (int)s.size();}
    String& operator+=(const String& other) {s+=other.s;return *this;}
    bool operator!=(const char* other) const {return s!=other;}
};
class StringArray {
    std::vector<String> items;
public:
    void add(String s) {items.push_back(std::move(s));}
    int size() const {return (int)items.size();}
    const String& operator[](int index)const{return items.at((size_t)index);}
    String joinIntoString(const char* delim)const {
        std::string r;
        for(const auto& x:items){if(!r.empty())r+=delim;r+=x.s;}
        return String(r);
    }
    static StringArray fromTokens(const String& text,const char* delim,const char*) {
        StringArray out;const auto sep=std::string(delim);
        size_t start=0,at;
        while((at=text.s.find(sep,start))!=std::string::npos){
            out.add(String(text.s.substr(start,at-start)));start=at+sep.size();
        }
        out.add(String(text.s.substr(start)));
        return out;
    }
};
class MidiMessage {
public:
    bool isMidiStart()const{return false;}
    bool isMidiClock()const{return false;}
    bool isForChannel(int)const{return true;}
    bool isController()const{return false;}
    bool isNoteOn()const{return false;}
    bool isNoteOff()const{return false;}
    bool isProgramChange()const{return false;}
    int getControllerNumber()const{return 0;}
    int getControllerValue()const{return 0;}
    int getNoteNumber()const{return 0;}
    int getVelocity()const{return 0;}
    int getProgramChangeNumber()const{return 0;}
};
} // namespace juce
