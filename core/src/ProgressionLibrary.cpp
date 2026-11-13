#include "chordsearch/ProgressionLibrary.h"

#include <sstream>

namespace chordsearch
{

namespace
{
    struct Def
    {
        const char* name;
        const char* genre;
        Mode mode;
        float weight;
        const char* romans; // space separated
    };

    // Sources: common-practice functional harmony, jazz standard repertoire
    // (Real Book turnarounds and cycles), blues forms, and widely cited pop/J-pop progressions.
    constexpr Def defs[] = {
        // ---- Jazz (major)
        { "ii–V–I",                    "Jazz",      Mode::major, 1.00f, "ii7 V7 Imaj7" },
        { "I–vi–ii–V 턴어라운드",        "Jazz",      Mode::major, 0.95f, "Imaj7 vi7 ii7 V7 Imaj7" },
        { "iii–vi–ii–V",               "Jazz",      Mode::major, 0.85f, "iii7 vi7 ii7 V7 Imaj7" },
        { "I–VI7–ii–V (리듬 체인지)",    "Jazz",      Mode::major, 0.85f, "Imaj7 VI7 ii7 V7 iii7 VI7 ii7 V7 Imaj7" },
        { "iii–VI7–ii–V",              "Jazz",      Mode::major, 0.75f, "iii7 VI7 ii7 V7 Imaj7" },
        { "세컨더리 도미넌트 체인",       "Jazz",      Mode::major, 0.65f, "III7 VI7 II7 V7 Imaj7" },
        { "ii–subV–I (트라이톤 대리)",    "Jazz",      Mode::major, 0.75f, "ii7 bII7 Imaj7" },
        { "백도어 ii–V",                "Jazz",      Mode::major, 0.70f, "iv7 bVII7 Imaj7" },
        { "IV–iv–I (마이너 플라갈)",      "Jazz",      Mode::major, 0.70f, "IVmaj7 iv6 Imaj7" },
        { "I–I7–IV–iv",                "Jazz",      Mode::major, 0.60f, "Imaj7 I7 IVmaj7 iv6 Imaj7" },
        { "IV로 가는 ii–V",              "Jazz",      Mode::major, 0.60f, "Imaj7 v7 I7 IVmaj7" },
        { "경과 감화음 상행",             "Jazz",      Mode::major, 0.60f, "Imaj7 #i°7 ii7 #ii°7 iii7" },
        { "IV–#iv°7–I",                "Gospel",    Mode::major, 0.55f, "IVmaj7 #iv°7 Imaj7" },
        { "Lady Bird 턴어라운드",         "Jazz",      Mode::major, 0.45f, "Imaj7 bIII7 bVImaj7 bII7 Imaj7" },
        { "Tadd Dameron 턴어라운드",      "Jazz",      Mode::major, 0.45f, "Imaj7 bIIImaj7 bVImaj7 bIImaj7 Imaj7" },
        { "Autumn Leaves 사이클 (장조)",  "Jazz",      Mode::major, 0.80f, "ii7 V7 Imaj7 IVmaj7 viiø7 III7 vi7" },
        { "vi–ii–V–I",                 "Jazz",      Mode::major, 0.80f, "vi7 ii7 V7 Imaj7" },
        { "거짓 종지",                   "Classical", Mode::major, 0.60f, "ii7 V7 vi7" },
        { "서스 도미넌트 해결",           "Jazz",      Mode::major, 0.50f, "ii7 V7sus V7 Imaj7" },
        { "Coltrane 체인지 (단편)",       "Jazz",      Mode::major, 0.35f, "Imaj7 bIII7 bVImaj7 VII7 IIImaj7 V7 Imaj7" },

        // ---- Blues
        { "재즈 블루스",                  "Blues",     Mode::major, 0.80f, "I7 IV7 I7 v7 I7 IV7 #iv°7 I7 VI7 ii7 V7 I7 VI7 ii7 V7" },
        { "12마디 블루스",                "Blues",     Mode::major, 0.75f, "I7 IV7 I7 IV7 I7 V7 IV7 I7 V7" },

        // ---- Pop / Rock
        { "I–V–vi–IV (액시스)",          "Pop",       Mode::major, 1.00f, "I V vi IV I" },
        { "vi–IV–I–V",                 "Pop",       Mode::major, 0.90f, "vi IV I V vi" },
        { "I–vi–IV–V (50년대)",          "Pop",       Mode::major, 0.80f, "I vi IV V I" },
        { "캐논 진행",                    "Classical", Mode::major, 0.85f, "I V vi iii IV I IV V I" },
        { "I–IV–V–I",                  "Classical", Mode::major, 0.85f, "I IV V I" },
        { "IV–V–I",                    "Classical", Mode::major, 0.80f, "IV V I" },
        { "I–IV–vi–V",                 "Pop",       Mode::major, 0.55f, "I IV vi V" },
        { "I–iii–IV–V",                "Pop",       Mode::major, 0.50f, "I iii IV V" },
        { "믹솔리디안 bVII",              "Rock",      Mode::major, 0.50f, "I bVII IV I" },
        { "bVI–bVII–I",                "Rock",      Mode::major, 0.50f, "bVI bVII I" },
        { "IV–V–vi",                   "Pop",       Mode::major, 0.65f, "IV V vi" },

        // ---- J-Pop / Neo-Soul / K-Pop
        { "왕도 진행 (Royal Road)",       "J-Pop",     Mode::major, 0.90f, "IVmaj7 V7 iii7 vi7" },
        { "Just the Two of Us",        "Neo-Soul",  Mode::major, 0.75f, "IVmaj7 III7 vi7 v7 I7 IVmaj7" },
        { "네오소울 IV–iii–vi",           "Neo-Soul",  Mode::major, 0.60f, "IVmaj7 iii7 vi7 ii7 V7" },
        { "IV–iii–ii–I",               "Neo-Soul",  Mode::major, 0.50f, "IVmaj7 iii7 ii7 Imaj7" },
        { "vi–V–IV–V",                 "Pop",       Mode::major, 0.45f, "vi V IV V" },

        // ---- Minor
        { "단조 ii–V–i",                 "Jazz",      Mode::minor, 1.00f, "iiø7 V7 i7" },
        { "Autumn Leaves 사이클 (단조)",  "Jazz",      Mode::minor, 0.90f, "iv7 bVII7 bIIImaj7 bVImaj7 iiø7 V7 i7" },
        { "안달루시안 종지",               "Flamenco",  Mode::minor, 0.85f, "i bVII bVI V7" },
        { "i–iv–V–i",                  "Classical", Mode::minor, 0.80f, "i iv V7 i" },
        { "i–bVI–bIII–bVII",           "Pop",       Mode::minor, 0.80f, "i bVI bIII bVII i" },
        { "i–bVII–bVI–bVII",           "Pop",       Mode::minor, 0.60f, "i bVII bVI bVII i" },
        { "i–bVI–bVII–i",              "Rock",      Mode::minor, 0.60f, "i bVI bVII i" },
        { "단조 블루스",                  "Blues",     Mode::minor, 0.60f, "i7 iv7 i7 bVI7 V7 i7" },
        { "bVI–V7–i",                  "Classical", Mode::minor, 0.60f, "bVImaj7 V7 i7" },
        { "i–iiø–V–i",                 "Classical", Mode::minor, 0.75f, "i7 iiø7 V7 i7" },
        { "도리안 뱀프",                  "Funk",      Mode::minor, 0.55f, "i7 IV7 i7" },
        { "나폴리 6화음 종지",             "Classical", Mode::minor, 0.40f, "i bIImaj7 V7 i" },
        { "i–v–bVI–bIII (단조 캐논)",     "Pop",       Mode::minor, 0.45f, "i v bVI bIII iv i" },
        { "iv–V7–i",                   "Classical", Mode::minor, 0.65f, "iv7 V7 i7" },
    };

    std::vector<Progression> build()
    {
        std::vector<Progression> out;
        for (const auto& d : defs)
        {
            Progression p;
            p.name = d.name;
            p.genre = d.genre;
            p.mode = d.mode;
            p.weight = d.weight;
            p.display = d.romans;

            std::istringstream in (d.romans);
            std::string tok;
            while (in >> tok)
            {
                auto t = parseRoman (tok);
                if (! t)
                    continue; // malformed entry: skipped (covered by unit test)
                if (! p.tokens.empty() && p.tokens.back().degree == t->degree
                    && p.tokens.back().chord.family() == t->chord.family())
                    continue; // collapse repeats
                p.tokens.push_back (*t);
            }
            out.push_back (std::move (p));
        }
        return out;
    }
} // namespace

const std::vector<Progression>& progressionLibrary()
{
    static const std::vector<Progression> lib = build();
    return lib;
}

} // namespace chordsearch
