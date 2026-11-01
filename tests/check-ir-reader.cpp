#include "viper-ir-wave.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
static void put16(std::ofstream& o, uint16_t x) {
    const char b[] = {char(x), char(x >> 8)}; o.write(b, 2);
}
static void put32(std::ofstream& o, uint32_t x) {
    const char b[] = {char(x), char(x >> 8), char(x >> 16), char(x >> 24)}; o.write(b, 4);
}
static void make(const std::string& file, uint16_t bits, uint16_t format, uint32_t rate) {
    const uint16_t ch = 2; const uint32_t frames=16, bytes=bits/8, data_bytes=frames*ch*bytes;
    std::ofstream o(file, std::ios::binary);
    o.write("RIFF",4); put32(o,36+data_bytes); o.write("WAVEfmt ",8);
    put32(o,16); put16(o,format); put16(o,ch); put32(o,rate);
    put32(o,rate*ch*bytes); put16(o,ch*bytes); put16(o,bits);
    o.write("data",4);put32(o,data_bytes);
    for (uint32_t i=0; i<data_bytes; ++i) o.put(0);
}
int main(int argc, char** argv) {
    assert(argc == 2); const std::string dir=argv[1];
    for (auto bits : {16,24,32}) {
        const auto file=dir+"/ir"+std::to_string(bits)+".irs";
        make(file,bits,1,48000);
        const auto a=viper_ir::read(file);
        assert(a.error.empty() && a.channels==2 && a.rate==48000 && a.samples.size()==32);
    }
    const auto f=dir+"/float32.irs";make(f,32,3,44100);
    const auto a=viper_ir::read(f);
    assert(a.error.empty() && a.rate==44100 && a.samples.size()==32);
    auto up=a;
    assert(viper_ir::resample_to(up,48000));
    assert(up.error.empty() && up.rate==48000 && up.samples.size()==
           2*static_cast<size_t>(std::llround(16.0*48000/44100)));
    auto down=a;
    assert(viper_ir::resample_to(down,32000));
    assert(down.error.empty() && down.rate==32000);
    auto impulse=a;
    impulse.samples.assign(441*2,0.0f);
    impulse.samples[100*2]=1.f;
    impulse.samples[100*2+1]=1.f;
    assert(viper_ir::resample_to(impulse,48000));
    double sumL=0, sumR=0;
    for(size_t i=0;i<impulse.samples.size();i+=2){sumL+=impulse.samples[i];sumR+=impulse.samples[i+1];}
    assert(std::abs(sumL-1.)<0.03 && std::abs(sumR-1.)<0.03);
    // Anti-alias property: a high-frequency filter component above the new
    // Nyquist frequency must be strongly attenuated when downsampling.
    viper_ir::Wave alias;
    alias.rate=96000; alias.channels=1;
    for (int i=0;i<2000;++i)
        alias.samples.push_back(static_cast<float>(std::sin(2.0*3.141592653589793*30000*i/96000)));
    assert(viper_ir::resample_to(alias,48000));
    double power=0;
    for (size_t i=64;i+64<alias.samples.size();++i)
        power+=alias.samples[i]*alias.samples[i];
    assert(std::sqrt(power/(alias.samples.size()-128)) < 0.035);
    const auto invalid=dir+"/invalid.irs";
    {std::ofstream o(invalid);o<<"not-a-wave";}
    assert(!viper_ir::read(invalid).error.empty());
    assert(!viper_ir::read(dir+"/missing.irs").error.empty());
    std::puts("PASS: IR parser, 44.1k->48k and 44.1k->32k resampling, DC gain, invalid files");
}
