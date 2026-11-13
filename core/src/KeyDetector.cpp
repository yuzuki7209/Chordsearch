#include "chordsearch/KeyDetector.h"
#include "chordsearch/Harmony.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace chordsearch
{

namespace
{
    // Krumhansl & Kessler (1982) probe-tone key profiles.
    constexpr std::array<double, 12> majorProfile { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    constexpr std::array<double, 12> minorProfile { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

    double correlation (const std::array<double, 12>& x, const std::array<double, 12>& profile, int tonic)
    {
        const double mx = std::accumulate (x.begin(), x.end(), 0.0) / 12.0;
        const double my = std::accumulate (profile.begin(), profile.end(), 0.0) / 12.0;

        double num = 0.0, dx = 0.0, dy = 0.0;
        for (int pc = 0; pc < 12; ++pc)
        {
            const double a = x[(std::size_t) pc] - mx;
            const double b = profile[(std::size_t) mod12 (pc - tonic)] - my;
            num += a * b;
            dx += a * a;
            dy += b * b;
        }
        if (dx <= 1e-12 || dy <= 1e-12)
            return 0.0;
        return num / std::sqrt (dx * dy);
    }

    bool isTonicChord (const Chord& c, const Key& k)
    {
        if (c.root != k.tonic)
            return false;
        const auto f = c.family();
        return k.mode == Mode::major ? f == Family::major : f == Family::minor;
    }

    double cadenceScore (const Chord& a, const Chord& b, const Key& k)
    {
        const int ra = mod12 (a.root - k.tonic);
        const int rb = mod12 (b.root - k.tonic);
        const auto fa = a.family();
        const auto fb = b.family();
        const bool major = k.mode == Mode::major;
        const bool tonicB = isTonicChord (b, k);

        if (tonicB)
        {
            if (fa == Family::dominant && ra == 7)                  return 1.0; // V7 → I
            if (fa == Family::major && ra == 7)                     return 0.7; // V → I
            if (fa == Family::dominant && ra == 1)                  return 0.6; // subV → I
            if (fa == Family::diminished && ra == 11)               return 0.5; // vii°7 → I
            if (fa == Family::dominant && ra == 10 && major)        return 0.5; // backdoor
            if (ra == 5 && (fa == Family::major || fa == Family::minor)) return 0.3; // plagal
        }

        if (fb == Family::dominant && rb == 7 && ra == 2)
        {
            if (fa == Family::minor && major)                 return 0.6;  // ii–V
            if (fa == Family::halfDiminished && ! major)      return 0.7;  // iiø–V
            if (fa == Family::minor || fa == Family::halfDiminished) return 0.3;
        }
        return 0.0;
    }
} // namespace

void KeyDetector::reset()
{
    histogram.fill (0.0);
    recent.clear();
    sorted.clear();
    currentKey.reset();
    challenger.reset();
    if (locked)
        currentKey = locked;
}

void KeyDetector::addWeights (const Chord& chord, double amount)
{
    Chord basic = chord;
    basic.tensions = 0;
    const auto tones = basic.pitchClasses();
    const auto all = chord.pitchClasses();

    for (int pc = 0; pc < 12; ++pc)
    {
        double w = 0.0;
        if (tones.contains (pc)) w += 1.0;
        else if (all.contains (pc)) w += 0.4;
        if (pc == chord.root && ! chord.rootless) w += 1.0;
        if (pc == chord.bass) w += 0.5;
        histogram[(std::size_t) pc] += w * amount;
    }
}

void KeyDetector::addChordEvent (const Chord& chord)
{
    recent.push_back (chord);
    while (recent.size() > maxRecent)
        recent.pop_front();
    addWeights (chord, 0.6);
}

void KeyDetector::replaceLastChord (const Chord& chord)
{
    if (recent.empty())
        addChordEvent (chord);
    else
        recent.back() = chord;
}

void KeyDetector::accumulate (const Chord& chord, double seconds)
{
    if (seconds > 0.0)
        addWeights (chord, std::min (seconds, 0.5));
}

void KeyDetector::decay (double seconds)
{
    if (seconds <= 0.0)
        return;
    const double f = std::exp (-seconds / 40.0);
    for (auto& v : histogram)
        v *= f;
}

bool KeyDetector::evaluate (double now)
{
    if (recent.empty())
        return false;

    sorted.clear();
    sorted.reserve (24);

    for (int i = 0; i < 24; ++i)
    {
        const auto key = Key::fromIndex (i);
        const double profile = correlation (histogram, key.mode == Mode::major ? majorProfile : minorProfile, key.tonic);

        double fitSum = 0.0, cadSum = 0.0, wSum = 0.0;
        double w = 1.0;
        for (std::size_t n = recent.size(); n-- > 0;)
        {
            const auto& c = recent[n];
            double fit = analyzeHarmony (c, key).fit;
            if (isTonicChord (c, key))
                fit += 0.5;
            fitSum += w * fit;

            if (n > 0)
                cadSum += w * cadenceScore (recent[n - 1], c, key);

            wSum += w;
            w *= 0.82;
        }

        const double prog = fitSum / wSum;
        const double cad = cadSum / wSum;
        const double total = 0.8 * profile + 1.0 * prog + 1.6 * cad + (key.mode == Mode::major ? 0.03 : 0.0);
        sorted.push_back ({ key, (float) total, 0.0f });
    }

    std::stable_sort (sorted.begin(), sorted.end(), [] (const auto& a, const auto& b) { return a.score > b.score; });

    // Softmax for display probabilities.
    constexpr double temperature = 0.3;
    double z = 0.0;
    for (const auto& e : sorted)
        z += std::exp ((e.score - sorted.front().score) / temperature);
    for (auto& e : sorted)
        e.probability = (float) (std::exp ((e.score - sorted.front().score) / temperature) / z);

    if (locked)
    {
        const bool changed = currentKey != locked;
        currentKey = locked;
        return changed;
    }

    const auto best = sorted.front();
    if (! currentKey)
    {
        currentKey = best.key;
        return true;
    }

    if (best.key == *currentKey)
    {
        challenger.reset();
        return false;
    }

    float currentScore = 0.0f;
    for (const auto& e : sorted)
        if (e.key == *currentKey)
            currentScore = e.score;

    if (challenger != best.key)
    {
        challenger = best.key;
        challengerSince = now;
    }

    const float margin = best.score - currentScore;
    if (margin > 0.35f || (now - challengerSince > 1.5 && margin > 0.08f))
    {
        currentKey = best.key;
        challenger.reset();
        return true;
    }
    return false;
}

float KeyDetector::currentProbability() const noexcept
{
    if (! currentKey)
        return 0.0f;
    for (const auto& e : sorted)
        if (e.key == *currentKey)
            return e.probability;
    return 0.0f;
}

std::optional<Key> KeyDetector::localCenter() const
{
    if (recent.empty() || ! currentKey)
        return std::nullopt;

    std::optional<Key> centre;
    const auto& last = recent.back();

    if (recent.size() >= 2)
    {
        const auto& prev = recent[recent.size() - 2];
        // V7 → X resolution
        if (prev.family() == Family::dominant && mod12 (prev.root + 5) == last.root
            && (last.family() == Family::major || last.family() == Family::minor))
            centre = Key { last.root, last.family() == Family::minor ? Mode::minor : Mode::major };

        // ii–V pointing somewhere
        if (! centre && last.family() == Family::dominant && mod12 (prev.root + 5) == last.root
            && (prev.family() == Family::minor || prev.family() == Family::halfDiminished))
            centre = Key { mod12 (last.root + 5), prev.family() == Family::halfDiminished ? Mode::minor : Mode::major };
    }

    if (centre && centre->tonic == currentKey->tonic)
        return std::nullopt;
    return centre;
}

} // namespace chordsearch
