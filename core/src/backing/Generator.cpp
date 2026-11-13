#include "chordsearch/backing/Backing.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>
#include <map>

namespace chordsearch::backing
{

const char* genreName (Genre g) noexcept
{
    switch (g)
    {
        case Genre::swing:   return "Swing";
        case Genre::bossa:   return "Bossa Nova";
        case Genre::rock:    return "Rock";
        case Genre::funk:    return "Funk";
        case Genre::ballad:  return "Ballad";
        case Genre::shuffle: return "Blues Shuffle";
    }
    return "";
}

const char* genreDescription (Genre g) noexcept
{
    switch (g)
    {
        case Genre::swing:   return "워킹 베이스 · 라이드 스윙 · 루트리스 컴핑 · 4비트 기타";
        case Genre::bossa:   return "루트-5도 베이스 · 크로스스틱 클라베 · 엄지+핑거 기타";
        case Genre::rock:    return "8비트 베이스 · 8비트 드럼 · 파워코드 팜뮤트";
        case Genre::funk:    return "16비트 싱코페이션 베이스 · 고스트노트 · 치킨 스크래치";
        case Genre::ballad:  return "2분음표 베이스 · 소프트 8비트 · 아르페지오 기타";
        case Genre::shuffle: return "부기 베이스 · 셔플 드럼 · 부기 더블스톱";
    }
    return "";
}

double defaultTempo (Genre g) noexcept
{
    switch (g)
    {
        case Genre::swing:   return 160.0;
        case Genre::bossa:   return 136.0;
        case Genre::rock:    return 118.0;
        case Genre::funk:    return 100.0;
        case Genre::ballad:  return 72.0;
        case Genre::shuffle: return 112.0;
    }
    return 120.0;
}

// ---------------------------------------------------------------------------
std::vector<int> voiceLead (const std::vector<int>& pitchClasses, int low, int high,
                            const std::vector<int>& previous, bool firstIsBottom)
{
    if (pitchClasses.empty())
        return {};

    const int n = (int) pitchClasses.size();
    std::vector<int> best;
    double bestCost = std::numeric_limits<double>::max();

    auto sortedPrev = previous;
    std::sort (sortedPrev.begin(), sortedPrev.end());

    for (int r = 0; r < (firstIsBottom ? 1 : n); ++r)
    {
        std::vector<int> order;
        for (int k = 0; k < n; ++k)
            order.push_back (mod12 (pitchClasses[(std::size_t) ((r + k) % n)]));

        int start = low;
        while (mod12 (start) != order[0])
            ++start;

        for (int oct : { 0, 12 })
        {
            std::vector<int> notes { start + oct };
            for (int k = 1; k < n; ++k)
            {
                int next = notes.back() + 1;
                while (mod12 (next) != order[(std::size_t) k])
                    ++next;
                notes.push_back (next);
            }
            if (notes.back() > high)
                continue;

            double cost = 0.0;
            if (sortedPrev.empty())
            {
                double centre = 0.0;
                for (int x : notes) centre += x;
                cost = std::abs (centre / n - (low + high) * 0.5);
            }
            else
            {
                const std::size_t m = std::min (notes.size(), sortedPrev.size());
                for (std::size_t i = 0; i < m; ++i)
                    cost += std::abs (notes[i] - sortedPrev[i]);
                cost += 3.0 * (double) (std::max (notes.size(), sortedPrev.size()) - m);
            }

            if (cost < bestCost)
            {
                bestCost = cost;
                best = notes;
            }
        }
    }

    if (best.empty()) // register too narrow: place each pitch class as close to the centre as possible
    {
        const int centre = (low + high) / 2;
        for (int pc : pitchClasses)
        {
            int note = centre - 6;
            while (mod12 (note) != mod12 (pc))
                ++note;
            best.push_back (note);
        }
        std::sort (best.begin(), best.end());
    }
    return best;
}

namespace
{
    struct Rng
    {
        std::uint32_t s;
        explicit Rng (std::uint32_t seed) : s (seed == 0 ? 0x9e3779b9u : seed) {}

        float next01()
        {
            s ^= s << 13;
            s ^= s >> 17;
            s ^= s << 5;
            return (float) (s & 0xffffff) / (float) 0x1000000;
        }
        float range (float a, float b) { return a + (b - a) * next01(); }
        bool chance (float p) { return next01() < p; }
        int pick (int n) { return std::min (n - 1, (int) (next01() * (float) n)); }
    };

    struct Segment
    {
        Chord chord;
        double start;
        double length;
    };

    /** Chord tone intervals used for voicing. -1 = absent. */
    struct Tones
    {
        int third = 4, fifth = 7, seventh = -1, ninth = 2, thirteenth = -1;
    };

    Tones tonesOf (const Chord& c)
    {
        Tones t;
        const auto fam = c.family();
        const bool altered = c.hasTension (Tension::flat9) || c.hasTension (Tension::sharp9) || c.hasTension (Tension::flat13);

        switch (c.third)
        {
            case Third::minor: t.third = 3; break;
            case Third::major: t.third = 4; break;
            case Third::sus4:  t.third = 5; break;
            case Third::sus2:  t.third = 2; break;
            case Third::none:  t.third = -1; break;
        }
        switch (c.fifth)
        {
            case Fifth::flat:  t.fifth = 6; break;
            case Fifth::sharp: t.fifth = 8; break;
            default:           t.fifth = 7; break;
        }
        switch (c.seventh)
        {
            case Seventh::minor:      t.seventh = 10; break;
            case Seventh::major:      t.seventh = 11; break;
            case Seventh::diminished: t.seventh = 9; break;
            case Seventh::sixth:      t.seventh = 9; break;
            case Seventh::none:       t.seventh = -1; break;
        }

        if (c.hasTension (Tension::flat9))                                   t.ninth = 1;
        else if (c.hasTension (Tension::sharp9) && c.third == Third::major)  t.ninth = 3;
        else                                                                 t.ninth = 2;

        if (c.hasTension (Tension::thirteen))     t.thirteenth = 9;
        else if (c.hasTension (Tension::flat13))  t.thirteenth = 8;
        else if (fam == Family::dominant)         t.thirteenth = altered ? 8 : 9;
        else if (fam == Family::major)            t.thirteenth = 9;

        return t;
    }

    /** Jazz piano: 4-note rootless colour voicing (as intervals above the root). */
    std::vector<int> jazzVoicing (const Chord& c)
    {
        const auto t = tonesOf (c);
        switch (c.family())
        {
            case Family::dominant:
                if (c.isSus()) return { 5, 10, 2, 7 };
                return { t.third, t.thirteenth >= 0 ? t.thirteenth : t.fifth, 10, t.ninth };
            case Family::major:
                return { 4, 7, c.seventh == Seventh::major ? 11 : 9, 2 };
            case Family::minor:
                if (c.seventh == Seventh::minor) return { 3, 7, 10, 2 };
                if (c.seventh == Seventh::major) return { 3, 7, 11, 2 };
                return { 3, 7, 9, 2 };
            case Family::halfDiminished: return { 0, 3, 6, 10 };
            case Family::diminished:     return { 0, 3, 6, 9 };
            case Family::augmented:      return { 4, 8, c.seventh == Seventh::major ? 11 : 0, 2 };
            case Family::suspended:      return { 0, t.third, 7, 2 };
            case Family::power:          return { 0, 7 };
        }
        return { 0, 4, 7 };
    }

    /** Triad (pop/rock). */
    std::vector<int> triadVoicing (const Chord& c)
    {
        const auto t = tonesOf (c);
        if (t.third < 0) return { 0, 7 };
        return { 0, t.third, t.fifth };
    }

    /** Funk upper-structure: 3-7-9 (or 3-5-9 for triads). */
    std::vector<int> funkVoicing (const Chord& c)
    {
        const auto t = tonesOf (c);
        const int third = t.third < 0 ? 7 : t.third;
        if (c.family() == Family::halfDiminished || c.family() == Family::diminished)
            return { 3, 6, t.seventh >= 0 ? t.seventh : 0 };
        return { third, t.seventh >= 0 ? t.seventh : t.fifth, t.ninth };
    }

    /** Guitar shell (root at the bottom): R-3-7 / R-3-6. */
    std::vector<int> shellVoicing (const Chord& c)
    {
        const auto t = tonesOf (c);
        return { 0, t.third < 0 ? 7 : t.third, t.seventh >= 0 ? t.seventh : t.fifth };
    }

    std::vector<int> absolute (const std::vector<int>& intervals, int root)
    {
        std::vector<int> out;
        for (int iv : intervals)
            out.push_back (mod12 (root + iv));
        return out;
    }

    int nearestNote (int pc, int target, int lo, int hi)
    {
        int best = -1, bestD = std::numeric_limits<int>::max();
        for (int n = lo; n <= hi; ++n)
        {
            if (mod12 (n) != mod12 (pc))
                continue;
            const int d = std::abs (n - target);
            if (d < bestD)
            {
                bestD = d;
                best = n;
            }
        }
        return best >= 0 ? best : std::clamp (target, lo, hi);
    }

    /** Scale pitch classes (relative to root) a walking line may use over a chord. */
    std::vector<int> walkingScale (const Chord& c)
    {
        switch (c.family())
        {
            case Family::dominant:
                if (c.hasTension (Tension::flat9) || c.hasTension (Tension::flat13))
                    return { 0, 1, 4, 5, 7, 8, 10 };
                return { 0, 2, 4, 5, 7, 9, 10 };
            case Family::major:          return c.hasTension (Tension::sharp11) ? std::vector<int> { 0, 2, 4, 6, 7, 9, 11 }
                                                                                : std::vector<int> { 0, 2, 4, 5, 7, 9, 11 };
            case Family::minor:          return c.seventh == Seventh::major ? std::vector<int> { 0, 2, 3, 5, 7, 9, 11 }
                                                                            : std::vector<int> { 0, 2, 3, 5, 7, 9, 10 };
            case Family::halfDiminished: return { 0, 1, 3, 5, 6, 8, 10 };
            case Family::diminished:     return { 0, 2, 3, 5, 6, 8, 9, 11 };
            case Family::augmented:      return { 0, 2, 4, 6, 8, 10 };
            default:                     return { 0, 2, 4, 5, 7, 9, 10 };
        }
    }

    struct Hit
    {
        double pos;
        char degree;      // bass: R 3 5 6 7 O L(5th below) b(b7)
        float vel;
        double len;
        bool anticipate = false;
    };

    class Generator
    {
    public:
        Generator (const std::vector<ChartChord>& chart, Genre g, std::uint32_t seed, int passes)
            : genre (g), rng (seed)
        {
            double chartLen = 0.0;
            for (const auto& c : chart)
                chartLen += std::max (1, c.beats);
            const double padded = std::ceil (chartLen / 4.0) * 4.0;

            const int chartBars = (int) (padded / 4.0);
            if (passes <= 0)
                passes = chartBars <= 4 ? 4 : (chartBars <= 16 ? 2 : 1);

            double t = 0.0;
            for (int p = 0; p < passes; ++p)
            {
                for (std::size_t i = 0; i < chart.size(); ++i)
                {
                    double len = std::max (1, chart[i].beats);
                    if (i + 1 == chart.size())
                        len += padded - chartLen;
                    Chord c = chart[i].chord;
                    segs.push_back ({ c, t, len });
                    t += len;
                }
            }

            total = t;
            bars = (int) std::lround (total / 4.0);
            seq.lengthBeats = total;
            seq.chartBeats = padded;
            seq.genre = genre;
        }

        Sequence run()
        {
            if (segs.empty())
                return seq;

            bass();
            drums();
            piano();
            guitar();
            click();

            std::stable_sort (seq.events.begin(), seq.events.end(),
                              [] (const NoteEvent& a, const NoteEvent& b) { return a.start < b.start; });
            return std::move (seq);
        }

    private:
        // ---------------------------------------------------------------- timeline
        std::size_t segIndexAt (double beat) const
        {
            beat = std::fmod (beat, total);
            if (beat < 0.0) beat += total;
            auto it = std::upper_bound (segs.begin(), segs.end(), beat + 1e-9,
                                        [] (double b, const Segment& s) { return b < s.start; });
            return (std::size_t) std::max<std::ptrdiff_t> (0, (it - segs.begin()) - 1);
        }

        const Segment& segAt (double beat) const { return segs[segIndexAt (beat)]; }
        const Segment& nextSeg (std::size_t i) const { return segs[(i + 1) % segs.size()]; }

        bool swingFeel() const { return genre == Genre::swing || genre == Genre::shuffle; }

        double feel (double beat) const
        {
            if (! swingFeel())
                return beat;
            const double whole = std::floor (beat);
            if (std::abs ((beat - whole) - 0.5) < 1e-6)
                return whole + 2.0 / 3.0;
            return beat;
        }

        void add (Instrument inst, int note, float vel, double start, double len, std::uint8_t flags = 0, bool humanize = true)
        {
            if (humanize)
            {
                if (start > 0.02)
                    start += rng.range (-0.01f, 0.01f);
                vel *= rng.range (0.9f, 1.07f);
            }
            start = std::clamp (start, 0.0, total - 1e-4);
            NoteEvent e;
            e.start = start;
            e.length = std::max (0.04, len);
            e.instrument = inst;
            e.note = (std::uint8_t) std::clamp (note, 0, 127);
            e.velocity = std::clamp (vel, 0.05f, 1.0f);
            e.flags = flags;
            seq.events.push_back (e);
        }

        bool phraseEnd (int bar) const { return bar % 4 == 3; }

        // ---------------------------------------------------------------- bass
        int bassDegree (const Chord& c, char d) const
        {
            const auto t = tonesOf (c);
            switch (d)
            {
                case '3': return t.third < 0 ? 7 : t.third;
                case '5': return t.fifth;
                case '6': return 9;
                case '7': return t.seventh >= 0 ? t.seventh : 12;
                case 'b': return 10;
                case 'O': return 12;
                case 'L': return t.fifth - 12;
                default:  return 0;
            }
        }

        void bass()
        {
            if (genre == Genre::swing)
            {
                walkingBass();
                return;
            }

            for (int b = 0; b < bars; ++b)
            {
                std::vector<Hit> hits;
                switch (genre)
                {
                    case Genre::bossa:
                        hits = { { 0.0, 'R', 0.85f, 1.45 }, { 1.5, 'L', 0.6f, 0.5 }, { 2.0, 'L', 0.8f, 1.45 }, { 3.5, 'R', 0.55f, 0.45 } };
                        break;
                    case Genre::rock:
                        for (int k = 0; k < 8; ++k)
                            hits.push_back ({ k * 0.5, 'R', k % 2 == 0 ? 0.85f : 0.68f, 0.42 });
                        if (phraseEnd (b))
                        {
                            hits[6].degree = '5';
                            hits[7].degree = 'O';
                        }
                        break;
                    case Genre::funk:
                        if (rng.chance (0.5f))
                            hits = { { 0.0, 'R', 0.95f, 0.35 }, { 0.75, 'R', 0.5f, 0.15 }, { 1.5, 'O', 0.8f, 0.15 },
                                     { 2.5, 'R', 0.75f, 0.3 }, { 2.75, '7', 0.6f, 0.2 }, { 3.0, '5', 0.7f, 0.3 }, { 3.5, 'O', 0.8f, 0.15 } };
                        else
                            hits = { { 0.0, 'R', 0.95f, 0.4 }, { 0.5, 'R', 0.35f, 0.12 }, { 1.25, '5', 0.7f, 0.2 }, { 1.75, 'O', 0.75f, 0.15 },
                                     { 2.5, 'R', 0.8f, 0.3 }, { 3.0, '7', 0.65f, 0.2 }, { 3.25, 'O', 0.4f, 0.12 }, { 3.5, '5', 0.7f, 0.25 } };
                        break;
                    case Genre::ballad:
                        hits = { { 0.0, 'R', 0.8f, 1.9 }, { 2.0, '5', 0.62f, 1.4 } };
                        break;
                    case Genre::shuffle:
                    {
                        const char line[8] = { 'R', 'R', '3', '3', '5', '5', '6', '6' };
                        for (int k = 0; k < 8; ++k)
                            hits.push_back ({ k * 0.5, line[k], k % 2 == 0 ? 0.8f : 0.6f, 0.4 });
                        break;
                    }
                    case Genre::swing:
                        break;
                }

                const double barStart = b * 4.0;
                for (const auto& h : hits)
                {
                    const double pos = barStart + h.pos;
                    const auto idx = segIndexAt (pos);
                    const auto& s = segs[idx];
                    const bool segStart = std::abs (pos - s.start) < 1e-6;
                    const char deg = segStart ? 'R' : h.degree;
                    const int root = segRoot (idx);
                    add (Instrument::bass, root + bassDegree (s.chord, deg), h.vel, feel (pos), h.len);
                }

                // Ballad: chromatic approach into a new chord on the next downbeat
                if (genre == Genre::ballad)
                {
                    const auto idx = segIndexAt (barStart + 3.5);
                    const auto& nx = nextSeg (idx);
                    if (std::abs (std::fmod (nx.start, total) - std::fmod (barStart + 4.0, total)) < 1e-6
                        && ! nx.chord.sameHarmony (segs[idx].chord))
                    {
                        const int target = nearestNote (nx.chord.root, lastBass, 33, 45);
                        add (Instrument::bass, target + (rng.chance (0.5f) ? 1 : -1), 0.55f, barStart + 3.5, 0.45);
                    }
                }
            }
        }

        int segRoot (std::size_t idx)
        {
            if (auto it = rootCache.find (idx); it != rootCache.end())
                return it->second;
            const int r = nearestNote (segs[idx].chord.root, lastBass, 33, 45);
            rootCache[idx] = r;
            lastBass = r;
            return r;
        }

        void walkingBass()
        {
            constexpr int lo = 28, hi = 50;
            int last = 40;

            // Split long chords into walk units of at most 4 beats
            struct Unit { std::size_t seg; double start; int beats; bool lastOfSeg; };
            std::vector<Unit> units;
            for (std::size_t i = 0; i < segs.size(); ++i)
            {
                double t = segs[i].start;
                double remaining = segs[i].length;
                while (remaining > 0.5)
                {
                    const int n = (int) std::min (4.0, std::round (remaining));
                    units.push_back ({ i, t, std::max (1, n), remaining <= 4.0 });
                    t += n;
                    remaining -= n;
                }
            }

            for (std::size_t u = 0; u < units.size(); ++u)
            {
                const auto& unit = units[u];
                const auto& chord = segs[unit.seg].chord;
                const auto& nextChord = segs[(unit.seg + 1) % segs.size()].chord;

                // Start: root (or the 5th/3rd when re-walking the same chord)
                int first = nearestNote (chord.root, std::clamp (last, 33, 45), lo, hi);
                if (unit.start > segs[unit.seg].start + 1e-6)
                {
                    const auto t = tonesOf (chord);
                    first = nearestNote (chord.root + (rng.chance (0.5f) ? t.fifth : (t.third < 0 ? 7 : t.third)), last, lo, hi);
                }

                // Target for the next downbeat
                int target;
                if (unit.lastOfSeg)
                    target = nearestNote (nextChord.root, first, 31, 47);
                else
                    target = nearestNote (chord.root, first + (rng.chance (0.5f) ? 7 : -5), 31, 47);

                int approach;
                const float r = rng.next01();
                if (r < 0.4f)      approach = target + 1;
                else if (r < 0.8f) approach = target - 1;
                else               approach = nearestNote (mod12 (target + 7), first, lo, hi); // dominant approach
                approach = std::clamp (approach, lo, hi);

                std::vector<int> line { first };
                if (unit.beats >= 3)
                {
                    const auto scale = absolute (walkingScale (chord), chord.root);
                    Chord basic = chord;
                    basic.tensions = 0;
                    const auto tones = basic.pitchClasses();

                    for (int k = 1; k <= unit.beats - 2; ++k)
                    {
                        const double ideal = first + (approach - first) * (double) k / (double) (unit.beats - 1);
                        int best = line.back();
                        double bestCost = 1e9;
                        for (int n = lo; n <= hi; ++n)
                        {
                            const bool inScale = std::find (scale.begin(), scale.end(), mod12 (n)) != scale.end();
                            const bool chordTone = tones.contains (n);
                            if (! inScale || (k == 2 && ! chordTone))
                                continue;
                            double cost = std::abs (n - ideal) + (n == line.back() ? 4.0 : 0.0) + rng.range (0.0f, 0.6f);
                            if (chordTone) cost -= 0.4;
                            if (cost < bestCost)
                            {
                                bestCost = cost;
                                best = n;
                            }
                        }
                        line.push_back (best);
                    }
                }
                if (unit.beats >= 2)
                    line.push_back (approach);

                for (std::size_t k = 0; k < line.size(); ++k)
                    add (Instrument::bass, line[k], k == 0 ? 0.85f : 0.72f, unit.start + (double) k, 0.92);

                // Occasional swung "skip" note before the next downbeat
                if (unit.beats == 4 && rng.chance (0.12f))
                    add (Instrument::bass, approach, 0.4f, feel (unit.start + 3.5), 0.3);

                last = line.back();
            }
        }

        // ---------------------------------------------------------------- drums
        void drum (std::uint8_t note, double pos, float vel, double len = 0.25)
        {
            add (Instrument::drums, note, vel, feel (pos), len);
        }

        void drums()
        {
            for (int b = 0; b < bars; ++b)
            {
                const double s = b * 4.0;
                const bool fill = phraseEnd (b);
                const bool crash = b % 8 == 0;

                switch (genre)
                {
                    case Genre::swing:
                    {
                        // Ride "spang-a-lang"
                        const double ride[] = { 0, 1, 1.5, 2, 3, 3.5 };
                        const float rideVel[] = { 0.62f, 0.78f, 0.5f, 0.62f, 0.78f, 0.5f };
                        for (int k = 0; k < 6; ++k)
                            if (! (fill && ride[k] >= 3.0))
                                drum (Drum::ride, s + ride[k], rideVel[k]);
                        drum (Drum::hatPedal, s + 1, 0.55f);
                        drum (Drum::hatPedal, s + 3, 0.55f);
                        for (int k = 0; k < 4; ++k)
                            drum (Drum::kick, s + k, 0.16f); // feathering

                        // Snare comping
                        const double spots[] = { 0.5, 1.5, 2.5, 3.5, 2.0 };
                        const int count = rng.pick (3);
                        for (int k = 0; k < count; ++k)
                            drum (Drum::snare, s + spots[rng.pick (5)], rng.range (0.22f, 0.45f));

                        if (fill)
                        {
                            drum (Drum::snare, s + 2.5, 0.5f);
                            drum (Drum::snare, s + 3.0, 0.6f);
                            drum (Drum::snare, s + 3.5, 0.7f);
                            drum (Drum::kick, s + 3.5, 0.55f);
                        }
                        if (crash && b > 0)
                        {
                            drum (Drum::crash, s, 0.55f, 2.0);
                            drum (Drum::kick, s, 0.55f);
                        }
                        break;
                    }

                    case Genre::bossa:
                    {
                        // Bossa clave on cross-stick (2-bar pattern)
                        if (b % 2 == 0) { drum (Drum::crossStick, s + 0, 0.6f); drum (Drum::crossStick, s + 1.5, 0.6f); drum (Drum::crossStick, s + 3, 0.6f); }
                        else            { drum (Drum::crossStick, s + 1, 0.6f); drum (Drum::crossStick, s + 2.5, 0.6f); }
                        for (int k = 0; k < 8; ++k)
                            drum (Drum::hatClosed, s + k * 0.5, k % 2 == 0 ? 0.38f : 0.26f, 0.2);
                        drum (Drum::kick, s + 0, 0.7f);
                        drum (Drum::kick, s + 1.5, 0.42f);
                        drum (Drum::kick, s + 2, 0.7f);
                        drum (Drum::kick, s + 3.5, 0.42f);
                        if (crash && b > 0)
                            drum (Drum::ride, s, 0.5f, 1.0);
                        break;
                    }

                    case Genre::rock:
                    {
                        const double hatEnd = fill ? 2.0 : 4.0;
                        for (int k = 0; k < 8 && k * 0.5 < hatEnd; ++k)
                            drum (Drum::hatClosed, s + k * 0.5, k % 2 == 0 ? 0.7f : 0.48f, 0.2);
                        drum (Drum::kick, s + 0, 0.95f);
                        drum (Drum::kick, s + 2, 0.85f);
                        if (rng.chance (0.6f)) drum (Drum::kick, s + 2.5, 0.7f);
                        drum (Drum::snare, s + 1, 0.92f);
                        if (! fill)
                            drum (Drum::snare, s + 3, 0.92f);
                        if (fill)
                        {
                            for (int k = 0; k < 4; ++k)
                                drum (Drum::snare, s + 2.0 + k * 0.25, 0.55f + 0.08f * (float) k);
                            drum (Drum::tomHigh, s + 3.0, 0.8f);
                            drum (Drum::tomHigh, s + 3.25, 0.75f);
                            drum (Drum::tomMid, s + 3.5, 0.8f);
                            drum (Drum::tomLow, s + 3.75, 0.85f);
                        }
                        if (crash)
                            drum (Drum::crash, s, 0.8f, 2.0);
                        break;
                    }

                    case Genre::funk:
                    {
                        for (int k = 0; k < 16; ++k)
                        {
                            const double p = k * 0.25;
                            if (fill && p >= 3.0)
                                break;
                            const bool openHat = (k == 14) && rng.chance (0.5f);
                            drum (openHat ? Drum::hatOpen : Drum::hatClosed, s + p,
                                  k % 4 == 0 ? 0.62f : (k % 2 == 0 ? 0.48f : 0.3f), openHat ? 0.4 : 0.15);
                        }
                        drum (Drum::kick, s + 0, 0.95f);
                        drum (Drum::kick, s + 0.75, 0.7f);
                        drum (Drum::kick, s + 2.5, 0.85f);
                        if (rng.chance (0.5f)) drum (Drum::kick, s + 3.25, 0.6f);
                        drum (Drum::snare, s + 1, 0.95f);
                        drum (Drum::snare, s + 3, 0.95f);
                        for (double g : { 1.75, 2.25, 3.75 })
                            if (rng.chance (0.7f))
                                drum (Drum::snare, s + g, rng.range (0.16f, 0.26f));
                        if (fill)
                            for (int k = 0; k < 4; ++k)
                                drum (Drum::snare, s + 3.0 + k * 0.25, 0.5f + 0.12f * (float) k);
                        if (crash)
                            drum (Drum::crash, s, 0.7f, 2.0);
                        break;
                    }

                    case Genre::ballad:
                    {
                        for (int k = 0; k < 8; ++k)
                            drum (Drum::hatClosed, s + k * 0.5, k % 2 == 0 ? 0.3f : 0.2f, 0.2);
                        drum (Drum::kick, s + 0, 0.65f);
                        drum (Drum::kick, s + 2.5, 0.45f);
                        drum (Drum::crossStick, s + 1, 0.5f);
                        drum (Drum::crossStick, s + 3, 0.5f);
                        if (fill)
                        {
                            drum (Drum::tomMid, s + 3.5, 0.4f);
                            drum (Drum::tomLow, s + 3.75, 0.45f);
                        }
                        if (crash)
                            drum (Drum::crash, s, 0.4f, 2.0);
                        break;
                    }

                    case Genre::shuffle:
                    {
                        for (int k = 0; k < 8; ++k)
                            if (! (fill && k >= 6))
                                drum (Drum::hatClosed, s + k * 0.5, k % 2 == 0 ? 0.6f : 0.35f, 0.2);
                        drum (Drum::kick, s + 0, 0.9f);
                        drum (Drum::kick, s + 2, 0.85f);
                        if (rng.chance (0.4f)) drum (Drum::kick, s + 1.5, 0.55f);
                        drum (Drum::snare, s + 1, 0.9f);
                        drum (Drum::snare, s + 3, 0.9f);
                        if (fill)
                        {
                            add (Instrument::drums, Drum::snare, 0.6f, s + 3.0 + 1.0 / 3.0, 0.2);
                            add (Instrument::drums, Drum::snare, 0.7f, s + 3.0 + 2.0 / 3.0, 0.2);
                        }
                        if (crash)
                            drum (Drum::crash, s, 0.7f, 2.0);
                        break;
                    }
                }
            }
        }

        // ---------------------------------------------------------------- comping (piano/guitar)
        using VoicingFn = std::function<std::vector<int> (const Chord&)>;

        void comp (Instrument inst, double pos, double len, float vel, const VoicingFn& voicing,
                   int low, int high, std::vector<int>& prev, bool rootBottom = false,
                   bool anticipate = false, double strum = 0.0, std::uint8_t flags = 0)
        {
            const auto& s = segAt (pos + (anticipate ? 0.5 : 0.0));
            const auto notes = voiceLead (absolute (voicing (s.chord), s.chord.root), low, high, prev, rootBottom);
            prev = notes;

            const double start = feel (pos);
            for (std::size_t i = 0; i < notes.size(); ++i)
                add (inst, notes[i], vel, start + strum * (double) i, len - strum * (double) i, flags);
        }

        /** Makes sure every chord change inside a bar gets at least one hit. */
        void coverChanges (int bar, std::vector<Hit>& hits, double len)
        {
            const double s = bar * 4.0;
            for (const auto& seg : segs)
            {
                const double local = seg.start - s;
                if (local <= 1e-6 || local >= 4.0)
                    continue;
                const bool covered = std::any_of (hits.begin(), hits.end(), [&] (const Hit& h)
                                                  { return h.pos >= local - 0.51 && h.pos < local + 1.0; });
                if (! covered)
                    hits.push_back ({ local, 'R', 0.5f, len });
            }
            std::sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.pos < b.pos; });
        }

        void piano()
        {
            std::vector<int> prev, prevLeft;
            const VoicingFn jazz = jazzVoicing, triad = triadVoicing, funky = funkVoicing;

            for (int b = 0; b < bars; ++b)
            {
                const double s = b * 4.0;
                std::vector<Hit> hits;

                switch (genre)
                {
                    case Genre::swing:
                    {
                        static const std::vector<std::vector<std::pair<double, double>>> cells {
                            { { 0.0, 0.7 }, { 1.5, 0.5 } },          // Charleston
                            { { 1.5, 0.5 }, { 3.5, 0.9 } },          // and-of-2 + anticipation
                            { { 0.0, 1.4 }, { 2.5, 0.5 } },
                            { { 1.0, 0.6 }, { 3.0, 0.6 } },
                            { { 0.5, 0.5 }, { 2.0, 0.8 } },
                            { { 2.5, 0.5 }, { 3.5, 0.9 } },
                        };
                        for (const auto& [p, l] : cells[(std::size_t) rng.pick ((int) cells.size())])
                            hits.push_back ({ p, 'R', rng.range (0.42f, 0.58f), l, p >= 3.5 });
                        coverChanges (b, hits, 0.6);
                        for (const auto& h : hits)
                            comp (Instrument::piano, s + h.pos, h.len, h.vel, jazz, 50, 74, prev, false, h.anticipate, 0.004);
                        break;
                    }

                    case Genre::bossa:
                    {
                        if (b % 2 == 0) hits = { { 0.0, 'R', 0.45f, 0.7 }, { 2.5, 'R', 0.4f, 0.45 } };
                        else            hits = { { 1.0, 'R', 0.42f, 0.6 }, { 3.5, 'R', 0.45f, 0.9, true } };
                        coverChanges (b, hits, 0.6);
                        for (const auto& h : hits)
                            comp (Instrument::piano, s + h.pos, h.len, h.vel, jazz, 52, 74, prev, false, h.anticipate);
                        break;
                    }

                    case Genre::rock:
                    {
                        for (int k = 0; k < 4; ++k)
                            comp (Instrument::piano, s + k, 0.45, k % 2 == 0 ? 0.6f : 0.5f, triad, 57, 76, prev);
                        for (double p : { 0.0, 2.0 })
                        {
                            const auto& seg = segAt (s + p);
                            const int r = nearestNote (seg.chord.root, 42, 36, 48);
                            add (Instrument::piano, r, 0.5f, s + p, 1.9);
                            add (Instrument::piano, r + 12, 0.42f, s + p, 1.9);
                        }
                        break;
                    }

                    case Genre::funk:
                    {
                        if (rng.chance (0.5f)) hits = { { 0.75, 'R', 0.6f, 0.18 }, { 1.5, 'R', 0.55f, 0.18 }, { 2.75, 'R', 0.6f, 0.18 }, { 3.5, 'R', 0.55f, 0.25 } };
                        else                   hits = { { 0.5, 'R', 0.55f, 0.15 }, { 1.25, 'R', 0.6f, 0.2 }, { 2.5, 'R', 0.55f, 0.2 }, { 3.25, 'R', 0.5f, 0.15 }, { 3.75, 'R', 0.55f, 0.2 } };
                        coverChanges (b, hits, 0.2);
                        for (const auto& h : hits)
                            comp (Instrument::piano, s + h.pos, h.len, h.vel, funky, 58, 77, prev);
                        break;
                    }

                    case Genre::ballad:
                    {
                        hits = { { 0.0, 'R', 0.4f, 1.95 }, { 2.0, 'R', 0.32f, 1.95 } };
                        coverChanges (b, hits, 1.9);
                        for (const auto& h : hits)
                            comp (Instrument::piano, s + h.pos, h.len, h.vel, jazz, 55, 76, prev, false, false, 0.02);
                        for (const auto& seg : segs)
                        {
                            if (seg.start < s - 1e-6 || seg.start >= s + 4.0 - 1e-6)
                                continue;
                            add (Instrument::piano, nearestNote (seg.chord.root, 41, 36, 48), 0.45f, seg.start,
                                 std::min (seg.length, s + 4.0 - seg.start) * 0.98);
                        }
                        break;
                    }

                    case Genre::shuffle:
                    {
                        hits = { { 0.5, 'R', 0.35f, 0.25 }, { 1.0, 'R', 0.5f, 0.35 }, { 2.5, 'R', 0.35f, 0.25 }, { 3.0, 'R', 0.5f, 0.35 } };
                        coverChanges (b, hits, 0.35);
                        for (const auto& h : hits)
                            comp (Instrument::piano, s + h.pos, h.len, h.vel, jazz, 55, 74, prev);
                        break;
                    }
                }
            }
        }

        void guitar()
        {
            std::vector<int> prev;
            const VoicingFn shell = shellVoicing, jazz = jazzVoicing, funky = funkVoicing;

            for (int b = 0; b < bars; ++b)
            {
                const double s = b * 4.0;

                switch (genre)
                {
                    case Genre::swing:
                        // Freddie Green: four short quarter-note shells
                        for (int k = 0; k < 4; ++k)
                            comp (Instrument::guitar, s + k, 0.32, k % 2 == 1 ? 0.55f : 0.48f, shell, 40, 62, prev, true, false, 0.012);
                        break;

                    case Genre::bossa:
                    {
                        // Thumb: root on 1, fifth on 3
                        for (double p : { 0.0, 2.0 })
                        {
                            const auto& seg = segAt (s + p);
                            const bool atStart = std::abs (seg.start - (s + p)) < 1e-6;
                            const int r = nearestNote (seg.chord.root, 45, 40, 52);
                            const int note = (p == 0.0 || atStart) ? r : r + tonesOf (seg.chord).fifth - 12;
                            add (Instrument::guitar, note < 40 ? note + 12 : note, 0.55f, s + p, 0.9);
                        }
                        // Fingers: syncopated plucks
                        std::vector<Hit> hits;
                        if (b % 2 == 0) hits = { { 0.0, 'R', 0.45f, 0.35 }, { 1.5, 'R', 0.42f, 0.35 }, { 3.0, 'R', 0.45f, 0.35 } };
                        else            hits = { { 0.5, 'R', 0.42f, 0.35 }, { 2.0, 'R', 0.45f, 0.35 }, { 3.5, 'R', 0.42f, 0.4, true } };
                        coverChanges (b, hits, 0.35);
                        for (const auto& h : hits)
                            comp (Instrument::guitar, s + h.pos, h.len, h.vel, jazz, 52, 69, prev, false, h.anticipate, 0.006);
                        break;
                    }

                    case Genre::rock:
                    {
                        for (int k = 0; k < 8; ++k)
                        {
                            const double p = s + k * 0.5;
                            const auto& seg = segAt (p);
                            const int r = nearestNote (seg.chord.root, 43, 40, 50);
                            const bool open = phraseEnd (b) && k >= 6;
                            const float v = open ? 0.85f : (k % 2 == 0 ? 0.7f : 0.58f);
                            const double len = open ? 0.9 : 0.22;
                            for (int iv : { 0, 7, 12 })
                                add (Instrument::guitar, r + iv, v, p + 0.004 * (iv / 7), len);
                        }
                        break;
                    }

                    case Genre::funk:
                    {
                        static const double accentsA[] = { 0.75, 1.5, 2.75, 3.5 };
                        static const double accentsB[] = { 0.5, 1.25, 2.5, 3.25 };
                        const double* accents = rng.chance (0.5f) ? accentsA : accentsB;
                        for (int k = 0; k < 16; ++k)
                        {
                            const double p = k * 0.25;
                            const bool accent = std::any_of (accents, accents + 4, [&] (double a) { return std::abs (a - p) < 1e-6; });
                            const double strum = (k % 2 == 0) ? 0.005 : -0.0; // down/up
                            if (accent)
                                comp (Instrument::guitar, s + p, 0.14, 0.68f, funky, 62, 77, prev, false, false, strum);
                            else
                            {
                                auto keep = prev;
                                comp (Instrument::guitar, s + p, 0.06, 0.26f, funky, 62, 77, keep, false, false, 0.003, NoteFlag::muted);
                            }
                        }
                        break;
                    }

                    case Genre::ballad:
                    {
                        // Arpeggio over an open voicing, re-voiced at each chord change
                        const VoicingFn open = [] (const Chord& c)
                        {
                            const auto t = tonesOf (c);
                            return std::vector<int> { 0, 7, t.third < 0 ? 12 : t.third, t.seventh >= 0 ? t.seventh : 12, t.ninth };
                        };
                        static const int order[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };
                        std::vector<int> notes;
                        const Segment* current = nullptr;
                        for (int k = 0; k < 8; ++k)
                        {
                            const double p = s + k * 0.5;
                            const auto& seg = segAt (p);
                            if (current != &seg)
                            {
                                current = &seg;
                                notes = voiceLead (absolute (open (seg.chord), seg.chord.root), 40, 74, prev, true);
                                prev = notes;
                            }
                            const int idx = std::min (order[k], (int) notes.size() - 1);
                            add (Instrument::guitar, notes[(std::size_t) idx], 0.42f, p, 1.4);
                        }
                        break;
                    }

                    case Genre::shuffle:
                    {
                        static const int top[8] = { 7, 7, 9, 9, 10, 10, 9, 9 };
                        for (int k = 0; k < 8; ++k)
                        {
                            const double p = s + k * 0.5;
                            const auto& seg = segAt (p);
                            const int r = nearestNote (seg.chord.root, 43, 40, 47);
                            int upper = top[k];
                            if (upper == 10 && seg.chord.family() != Family::dominant)
                                upper = 9;
                            const float v = k % 2 == 0 ? 0.62f : 0.5f;
                            add (Instrument::guitar, r, v, feel (p), 0.35);
                            add (Instrument::guitar, r + upper, v, feel (p) + 0.006, 0.35);
                        }
                        break;
                    }
                }
            }
        }

        void click()
        {
            for (int beat = 0; beat < (int) std::lround (total); ++beat)
                add (Instrument::click, beat % 4 == 0 ? 1 : 0, beat % 4 == 0 ? 0.9f : 0.6f, (double) beat, 0.1, 0, false);
        }

        Genre genre;
        Rng rng;
        std::vector<Segment> segs;
        double total = 0.0;
        int bars = 0;
        int lastBass = 40;
        std::map<std::size_t, int> rootCache;
        Sequence seq;
    };
} // namespace

Sequence generateSequence (const std::vector<ChartChord>& chart, Genre genre, std::uint32_t seed, int passes)
{
    if (chart.empty())
        return {};
    return Generator (chart, genre, seed, passes).run();
}

} // namespace chordsearch::backing
