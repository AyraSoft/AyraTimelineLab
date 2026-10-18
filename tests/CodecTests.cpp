/** @file CodecTests.cpp
    Roundtrip del codec JUCE e rifiuto atomico degli input invalidi.
    Questi test richiedono JUCE e NON sono stati eseguiti alla consegna.
    @see SessionCodec.cpp, verification/STATUS.md */
#include "SessionCodec.h"
#include <iostream>
#include <stdexcept>

int main()
{
    using namespace ayra::lab;
    int checks = 0;
    const auto require = [&checks](bool condition, const char* message)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(message);
    };

    try
    {
        auto source = makeFixture();
        source.tracks[0].name = "Nome \"quoted\" / Unicode \u00e8";
        auto restored = makeFixture();
        require(decodeSession(encodeSession(source), restored).wasOk(), "codec roundtrip rejected");
        require(source == restored, "codec roundtrip changed fields");

        const auto baseline = restored;
        for (const auto& input : {juce::String("{}"), juce::String("["), juce::String("null")})
        {
            require(decodeSession(input, restored).failed(), "invalid document accepted");
            require(restored == baseline, "failed decode changed destination");
        }
        const auto wrongVersion = encodeSession(source).replace("ayra-timeline-lab/1", "ayra-timeline-lab/2");
        require(decodeSession(wrongVersion, restored).failed(), "future schema accepted");
        require(restored == baseline, "schema rejection changed destination");

        source.tracks[1].order = {0, 0, 2, 3};
        require(decodeSession(encodeSession(source), restored).failed(), "duplicate channel admitted");
        require(restored == baseline, "invalid mapping changed destination");
        const auto oversized = juce::String::repeatedString("x", 2 * 1024 * 1024 + 1);
        require(decodeSession(oversized, restored).failed(), "oversized input accepted");
        require(restored == baseline, "oversized rejection changed destination");
        std::cout << "PASS: " << checks << " JUCE codec assertions\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
