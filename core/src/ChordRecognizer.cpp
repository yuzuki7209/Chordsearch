#include "chordsearch/ChordRecognizer.h"

#include <algorithm>
#include <bit>

namespace chordsearch
{

ChordCandidate ChordRecognizer::evaluate (PitchClassSet pcs, int root, int bassPc)
{
    const auto rel = pcs.relativeTo (root);
    auto h = [&] (int i) { return rel.contains (i); };

    Chord c;
    c.root = mod12 (root);
    c.bass = mod12 (bassPc);
    c.rootless = ! h (0);

    float penalty = 0.0f;

    // --- third
    if (h (4))
    {
        c.third = Third::major;
        if (h (3))
            c.tensions |= Tension::sharp9;
    }
    else if (h (3)) c.third = Third::minor;
    else if (h (5)) c.third = Third::sus4;
    else if (h (2)) c.third = Third::sus2;

    // --- seventh
    if (h (10))
    {
        c.seventh = Seventh::minor;
        if (h (11))
            penalty += 2.5f; // b7 and M7 together: unexplained
    }
    else if (h (11))
    {
        c.seventh = Seventh::major;
    }

    // --- fifth
    if (h (7))
    {
        c.fifth = Fifth::perfect;
    }
    else if (h (6) && h (8))
    {
        if (c.third == Third::minor) c.fifth = Fifth::flat; // 8 becomes b13
        // major third: both become tensions (#11, b13) -> altered dominant
    }
    else if (h (6))
    {
        if (c.third != Third::major)
            c.fifth = Fifth::flat; // dim / m7b5; for major third keep as #11
    }
    else if (h (8))
    {
        const bool alteredNine = h (1) || (h (3) && c.third == Third::major);
        if (c.third == Third::major && ! (c.seventh == Seventh::minor && alteredNine))
            c.fifth = Fifth::sharp;
        else if (c.third == Third::none)
            c.fifth = Fifth::sharp;
        // minor third: b13
    }

    // --- diminished seventh / sixth / thirteenth
    bool nineConsumed = false;
    if (c.third == Third::minor && c.fifth == Fifth::flat && c.seventh == Seventh::none && h (9))
    {
        c.seventh = Seventh::diminished;
        nineConsumed = true;
    }
    else if (h (9))
    {
        if (c.seventh == Seventh::none) c.seventh = Seventh::sixth;
        else                            c.tensions |= Tension::thirteen;
        nineConsumed = true;
    }
    (void) nineConsumed;

    // --- tensions
    if (h (1)) c.tensions |= Tension::flat9;
    if (h (2) && c.third != Third::sus2) c.tensions |= Tension::nine;
    if (h (5) && c.third != Third::sus4) c.tensions |= Tension::eleven;
    if (h (6) && c.fifth != Fifth::flat) c.tensions |= Tension::sharp11;
    if (h (8) && c.fifth != Fifth::sharp) c.tensions |= Tension::flat13;

    // --- score
    float s = 0.0f;

    if (! c.rootless) s += 3.0f;
    else              s -= 1.5f;

    if (c.bass == c.root && ! c.rootless) s += 2.0f;

    switch (c.third)
    {
        case Third::major:
        case Third::minor: s += 2.0f; break;
        case Third::sus4:  s += 0.8f; break;
        case Third::sus2:  s += 0.5f; break;
        case Third::none:  break;
    }

    switch (c.fifth)
    {
        case Fifth::perfect: s += 1.0f; break;
        case Fifth::flat:    s += c.third == Third::minor ? 0.8f : 0.2f; break;
        case Fifth::sharp:   s += 0.3f; break;
        case Fifth::none:    break;
    }

    switch (c.seventh)
    {
        case Seventh::minor:
        case Seventh::major:      s += 1.2f; break;
        case Seventh::diminished: s += 1.5f; break;
        case Seventh::sixth:      s += 0.6f; break;
        case Seventh::none:       break;
    }

    const int numTensions = std::popcount (unsigned (c.tensions));
    const int numAltered = std::popcount (unsigned (c.tensions & Tension::altered));
    s -= 0.35f * (float) numTensions + 0.15f * (float) numAltered;

    // Triad-type interpretations missing their fifth are usually inversions of something simpler
    if (c.fifth == Fifth::none && (c.seventh == Seventh::none || c.seventh == Seventh::sixth))
        s -= 0.8f;

    // Tensions other than add9 on a chord without a seventh are unusual
    if (c.seventh == Seventh::none)
        s -= 1.0f * (float) std::popcount (unsigned (c.tensions & ~Tension::nine));

    // Avoid / clash combinations
    if (c.third == Third::major && c.hasTension (Tension::eleven))     s -= 1.2f;
    if (c.seventh == Seventh::major && c.hasTension (Tension::flat9))  s -= 1.5f;
    if (c.third == Third::minor && c.hasTension (Tension::flat9))      s -= 0.6f;
    if (c.family() != Family::dominant && c.hasTension (Tension::sharp9)) s -= 1.0f;

    // Rootless voicings are only plausible as guide-tone based 7th chords
    if (c.rootless)
    {
        const bool guideTones = (c.third == Third::major || c.third == Third::minor)
                             && (c.seventh == Seventh::minor || c.seventh == Seventh::major);
        if (! guideTones || pcs.size() < 4)
            s -= 6.0f;
    }

    s -= penalty;
    return { c, s };
}

std::vector<ChordCandidate> ChordRecognizer::recognize (PitchClassSet pcs, int bassPc, std::size_t maxResults)
{
    std::vector<ChordCandidate> out;
    if (pcs.size() < 2)
        return out;

    out.reserve (12);
    for (int root = 0; root < 12; ++root)
        out.push_back (evaluate (pcs, root, bassPc));

    std::stable_sort (out.begin(), out.end(), [] (const auto& a, const auto& b) { return a.score > b.score; });

    if (out.size() > maxResults)
        out.resize (maxResults);
    return out;
}

} // namespace chordsearch
