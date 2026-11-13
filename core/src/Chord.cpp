#include "chordsearch/Chord.h"

#include <array>
#include <bit>
#include <vector>

namespace chordsearch
{

namespace Tension
{
    int interval (std::uint8_t flag) noexcept
    {
        switch (flag)
        {
            case flat9:    return 1;
            case nine:     return 2;
            case sharp9:   return 3;
            case eleven:   return 5;
            case sharp11:  return 6;
            case flat13:   return 8;
            case thirteen: return 9;
            default:       return 0;
        }
    }

    static constexpr std::array<std::pair<std::uint8_t, const char*>, 7> ordered {{
        { flat9, "b9" }, { nine, "9" }, { sharp9, "#9" }, { eleven, "11" },
        { sharp11, "#11" }, { flat13, "b13" }, { thirteen, "13" } }};

    std::string toString (std::uint8_t flags)
    {
        std::string s;
        for (const auto& [flag, text] : ordered)
        {
            if ((flags & flag) == 0)
                continue;
            if (! s.empty())
                s += ", ";
            s += text;
        }
        return s;
    }
} // namespace Tension

Family Chord::family() const noexcept
{
    switch (third)
    {
        case Third::major:
            if (seventh == Seventh::minor)  return Family::dominant;
            if (fifth == Fifth::sharp)      return Family::augmented;
            return Family::major;

        case Third::minor:
            if (fifth == Fifth::flat)
                return seventh == Seventh::minor ? Family::halfDiminished : Family::diminished;
            return Family::minor;

        case Third::sus2:
        case Third::sus4:
            return seventh == Seventh::minor ? Family::dominant : Family::suspended;

        case Third::none:
        default:
            return Family::power;
    }
}

PitchClassSet Chord::intervals() const noexcept
{
    PitchClassSet s;
    s.add (0);

    switch (third)
    {
        case Third::minor: s.add (3); break;
        case Third::major: s.add (4); break;
        case Third::sus2:  s.add (2); break;
        case Third::sus4:  s.add (5); break;
        case Third::none:  break;
    }

    switch (fifth)
    {
        case Fifth::perfect: s.add (7); break;
        case Fifth::flat:    s.add (6); break;
        case Fifth::sharp:   s.add (8); break;
        case Fifth::none:    break;
    }

    switch (seventh)
    {
        case Seventh::sixth:      s.add (9); break;
        case Seventh::minor:      s.add (10); break;
        case Seventh::major:      s.add (11); break;
        case Seventh::diminished: s.add (9); break;
        case Seventh::none:       break;
    }

    for (const auto& [flag, text] : Tension::ordered)
        if (hasTension (flag))
            s.add (Tension::interval (flag));

    return s;
}

std::string Chord::suffix() const
{
    std::uint8_t rest = tensions;
    std::vector<std::string> extras;
    std::string s;

    auto has  = [&] (std::uint8_t t) { return (rest & t) != 0; };
    auto take = [&] (std::uint8_t t)
    {
        const bool h = has (t);
        rest = std::uint8_t (rest & ~t);
        return h;
    };

    switch (third)
    {
        case Third::major:
        {
            if (fifth == Fifth::sharp)
            {
                if (seventh == Seventh::minor)      s = take (Tension::nine) ? "9#5" : "7#5";
                else if (seventh == Seventh::major) s = "maj7#5";
                else                                s = "+";
            }
            else if (seventh == Seventh::major)
            {
                if (has (Tension::thirteen) && has (Tension::nine)) { take (Tension::thirteen); take (Tension::nine); s = "maj13"; }
                else if (take (Tension::nine)) s = "maj9";
                else                           s = "maj7";
                if (fifth == Fifth::flat) extras.push_back ("b5");
            }
            else if (seventh == Seventh::minor)
            {
                const int altCount = std::popcount (unsigned (rest & Tension::altered)) + (fifth == Fifth::flat ? 1 : 0);
                const bool alteredNine = has (Tension::flat9) || has (Tension::sharp9);

                if (! has (Tension::nine) && ! has (Tension::thirteen) && alteredNine && altCount >= 3)
                {
                    rest = std::uint8_t (rest & ~Tension::altered);
                    s = "7alt";
                }
                else
                {
                    if (has (Tension::thirteen))   { take (Tension::thirteen); take (Tension::nine); s = "13"; }
                    else if (take (Tension::nine)) s = "9";
                    else                           s = "7";
                    if (fifth == Fifth::flat) extras.push_back ("b5");
                }
            }
            else if (seventh == Seventh::sixth)
            {
                s = take (Tension::nine) ? "6/9" : "6";
                if (fifth == Fifth::flat) extras.push_back ("b5");
            }
            else
            {
                s = take (Tension::nine) ? "add9" : "";
                if (fifth == Fifth::flat) extras.push_back ("b5");
            }
            break;
        }

        case Third::minor:
        {
            if (fifth == Fifth::flat)
            {
                if (seventh == Seventh::minor)           s = "m7b5";
                else if (seventh == Seventh::diminished) s = "dim7";
                else if (seventh == Seventh::major)      s = "dim(maj7)";
                else                                     s = "dim";
            }
            else
            {
                switch (seventh)
                {
                    case Seventh::minor:
                        if (has (Tension::eleven))        { take (Tension::eleven); take (Tension::nine); s = "m11"; }
                        else if (has (Tension::thirteen)) { take (Tension::thirteen); take (Tension::nine); s = "m13"; }
                        else if (take (Tension::nine))    s = "m9";
                        else                              s = "m7";
                        break;
                    case Seventh::major:  s = take (Tension::nine) ? "m(maj9)" : "m(maj7)"; break;
                    case Seventh::sixth:  s = take (Tension::nine) ? "m6/9" : "m6"; break;
                    default:              s = take (Tension::nine) ? "m(add9)" : "m"; break;
                }
                if (fifth == Fifth::sharp) extras.push_back ("#5");
            }
            break;
        }

        case Third::sus4:
        case Third::sus2:
        {
            const char* susName = third == Third::sus4 ? "sus4" : "sus2";
            if (seventh == Seventh::minor)
            {
                if (has (Tension::thirteen))   { take (Tension::thirteen); take (Tension::nine); s = std::string ("13") + susName; }
                else if (take (Tension::nine)) s = std::string ("9") + susName;
                else                           s = std::string ("7") + susName;
            }
            else if (seventh == Seventh::major) s = std::string ("maj7") + susName;
            else if (seventh == Seventh::sixth) s = std::string ("6") + susName;
            else                                s = susName;

            if (fifth == Fifth::flat)  extras.push_back ("b5");
            if (fifth == Fifth::sharp) extras.push_back ("#5");
            break;
        }

        case Third::none:
        default:
        {
            if (seventh == Seventh::none && fifth == Fifth::perfect && rest == 0)
                return "5";
            if (seventh == Seventh::minor)      s = "7(no3)";
            else if (seventh == Seventh::major) s = "maj7(no3)";
            else                                s = "(no3)";
            if (fifth == Fifth::flat)  extras.push_back ("b5");
            if (fifth == Fifth::sharp) extras.push_back ("#5");
            break;
        }
    }

    for (const auto& [flag, text] : Tension::ordered)
        if ((rest & flag) != 0)
            extras.push_back (text);

    if (! extras.empty())
    {
        s += "(";
        for (std::size_t i = 0; i < extras.size(); ++i)
            s += (i > 0 ? "," : "") + extras[i];
        s += ")";
    }

    return s;
}

std::string Chord::name (Accidentals acc) const
{
    auto n = pitchClassName (root, acc) + suffix();
    if (bass != root)
        n += "/" + pitchClassName (bass, acc);
    return n;
}

int Chord::degreeOf (int pc) const noexcept
{
    switch (mod12 (pc - root))
    {
        case 0:  return 1;
        case 1:  return 2;
        case 2:  return 2;
        case 3:  return third == Third::major ? 2 : 3;        // #9 vs b3
        case 4:  return 3;
        case 5:  return 4;
        case 6:  return fifth == Fifth::flat ? 5 : 4;          // b5 vs #11
        case 7:  return 5;
        case 8:  return fifth == Fifth::sharp ? 5 : 6;         // #5 vs b13
        case 9:  return seventh == Seventh::diminished ? 7 : 6; // bb7 vs 6/13
        case 10: return 7;
        default: return 7;
    }
}

std::string Chord::spell (int pc, Accidentals rootAcc) const
{
    return spellDegree (root, rootAcc, mod12 (pc - root), degreeOf (pc));
}

std::optional<Chord> Chord::fromSuffix (int root, std::string_view suffix)
{
    struct Preset
    {
        std::string_view name;
        Third third;
        Fifth fifth;
        Seventh seventh;
        std::uint8_t tensions;
    };

    using T = Third;
    using F = Fifth;
    using S = Seventh;
    namespace X = Tension;

    static constexpr std::array presets {
        Preset { "",        T::major, F::perfect, S::none,       0 },
        Preset { "m",       T::minor, F::perfect, S::none,       0 },
        Preset { "dim",     T::minor, F::flat,    S::none,       0 },
        Preset { "+",       T::major, F::sharp,   S::none,       0 },
        Preset { "sus4",    T::sus4,  F::perfect, S::none,       0 },
        Preset { "sus2",    T::sus2,  F::perfect, S::none,       0 },
        Preset { "5",       T::none,  F::perfect, S::none,       0 },
        Preset { "6",       T::major, F::perfect, S::sixth,      0 },
        Preset { "6/9",     T::major, F::perfect, S::sixth,      X::nine },
        Preset { "m6",      T::minor, F::perfect, S::sixth,      0 },
        Preset { "maj7",    T::major, F::perfect, S::major,      0 },
        Preset { "maj9",    T::major, F::perfect, S::major,      X::nine },
        Preset { "maj7#11", T::major, F::perfect, S::major,      X::sharp11 },
        Preset { "maj7#5",  T::major, F::sharp,   S::major,      0 },
        Preset { "7",       T::major, F::perfect, S::minor,      0 },
        Preset { "9",       T::major, F::perfect, S::minor,      X::nine },
        Preset { "13",      T::major, F::perfect, S::minor,      X::nine | X::thirteen },
        Preset { "7b9",     T::major, F::perfect, S::minor,      X::flat9 },
        Preset { "7#9",     T::major, F::perfect, S::minor,      X::sharp9 },
        Preset { "7#11",    T::major, F::perfect, S::minor,      X::sharp11 },
        Preset { "7b13",    T::major, F::perfect, S::minor,      X::flat13 },
        Preset { "7alt",    T::major, F::none,    S::minor,      X::flat9 | X::sharp9 | X::sharp11 | X::flat13 },
        Preset { "7#5",     T::major, F::sharp,   S::minor,      0 },
        Preset { "7sus4",   T::sus4,  F::perfect, S::minor,      0 },
        Preset { "9sus4",   T::sus4,  F::perfect, S::minor,      X::nine },
        Preset { "m7",      T::minor, F::perfect, S::minor,      0 },
        Preset { "m9",      T::minor, F::perfect, S::minor,      X::nine },
        Preset { "m11",     T::minor, F::perfect, S::minor,      X::nine | X::eleven },
        Preset { "m(maj7)", T::minor, F::perfect, S::major,      0 },
        Preset { "m7b5",    T::minor, F::flat,    S::minor,      0 },
        Preset { "dim7",    T::minor, F::flat,    S::diminished, 0 },
    };

    for (const auto& p : presets)
    {
        if (p.name != suffix)
            continue;

        Chord c;
        c.root = mod12 (root);
        c.bass = c.root;
        c.third = p.third;
        c.fifth = p.fifth;
        c.seventh = p.seventh;
        c.tensions = p.tensions;
        return c;
    }
    return std::nullopt;
}

} // namespace chordsearch
