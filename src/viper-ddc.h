#pragma once
// Read and validate Viper4Android .vdc dual-sample-rate IIR coefficients.
// All file IO and allocation must occur on the control/loader worker,
// never inside PipeWire's real-time process callback.
#include <array>
#include <cmath>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace viper_ddc {
using Section = std::array<float, 5>; // b0,b1,b2,a1,a2 (positive feedback sign)
struct Profile {
    std::vector<Section> at_44100;
    std::vector<Section> at_48000;
    std::string error;
    explicit operator bool() const { return error.empty() && !at_44100.empty(); }
};
inline std::string trim(std::string s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last + 1 - first);
}
inline bool stable(const Section& b) {
    // y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] + a1*y[n-1] + a2*y[n-2].
    // Jury stability test on 1 - a1*z^-1 - a2*z^-2.
    return 1.0 - b[3] - b[4] > 0.00000001 &&
           1.0 + b[3] - b[4] > 0.00000001 &&
           1.0 + b[4] > 0.00000001;
}
inline bool parse_sections(const std::string& raw, std::vector<Section>& out) {
    std::istringstream stream(raw);
    std::vector<float> coeff;
    std::string token;
    while (std::getline(stream, token, ',')) {
        token = trim(token);
        if (token.empty()) return false;
        try {
            std::size_t used = 0;
            const float value = std::stof(token, &used);
            if (used != token.size() || !std::isfinite(value) || std::abs(value) > 32.f)
                return false;
            coeff.push_back(value);
        } catch (...) { return false; }
        if (coeff.size() > 320) return false; // Max 64 biquad sections.
    }
    if (coeff.empty() || coeff.size() % 5 != 0) return false;
    for (std::size_t i = 0; i < coeff.size(); i += 5) {
        Section s{coeff[i], coeff[i+1], coeff[i+2], coeff[i+3], coeff[i+4]};
        if (!stable(s)) return false;
        out.push_back(s);
    }
    return true;
}
inline Profile read(const std::string& path) {
    Profile result;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) { result.error = "Cannot open DDC profile"; return result; }
    if (file.tellg() > 128*1024) { result.error = "DDC profile too large"; return result; }
    file.seekg(0);
    std::string line;
    bool found44=false, found48=false;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';' || line.rfind("//", 0) == 0)
            continue;
        if (line.rfind("SR_44100:", 0) == 0) {
            if (found44 || !parse_sections(line.substr(9), result.at_44100)) {
                result.error = "Invalid 44.1 kHz coefficients"; return result;
            }
            found44 = true;
        } else if (line.rfind("SR_48000:", 0) == 0) {
            if (found48 || !parse_sections(line.substr(9), result.at_48000)) {
                result.error = "Invalid 48 kHz coefficients"; return result;
            }
            found48 = true;
        } else if (line.find(':') != std::string::npos && line.rfind("SR_",0) == 0) {
            result.error = "Unsupported DDC sample-rate header"; return result;
        } // Ignore human-readable metadata from standard DDC exports.
    }
    if (!found44 || !found48 || result.at_44100.size() != result.at_48000.size()) {
        result.error = "DDC needs equal-length SR_44100 and SR_48000 sections";
    }
    return result;
}
} // namespace viper_ddc
