#include "../src/viper-ddc.h"
#include <cassert>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <cstdio>
int main() {
    using namespace viper_ddc;
    assert(stable({1,0,0,0,0}));
    assert(!stable({1,0,0,2,0}));
    assert(!stable({1,0,0,0,1}));
    std::vector<Section> v;
    assert(parse_sections("1, 0,0,0,0", v) && v.size()==1);
    v.clear(); assert(!parse_sections("1,0,0,0,nan", v));
    v.clear(); assert(!parse_sections("1,0,0,0,0,2", v));
    const std::string path = "/tmp/viper-ddc-parse-test-" + std::to_string(getpid()) + ".vdc";
    { std::ofstream f(path); f << "# Neutral filter\nSR_44100:1,0,0,0,0\nSR_48000:1,0,0,0,0\n"; }
    const auto profile=read(path);
    assert(profile && profile.at_44100.size()==1 && profile.at_48000.size()==1);
    { std::ofstream f(path); f << "SR_44100:1,0,0,0,0\n"; }
    assert(!read(path));
    std::remove(path.c_str());
    std::cout << "PASS: DDC parser neutral, invalid, unstable, missing-rate profiles\n";
}
