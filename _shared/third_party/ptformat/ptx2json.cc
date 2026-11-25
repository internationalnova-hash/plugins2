// ptx2json: reads a Pro Tools session (.ptx/.ptf/.pts) with ptformat and prints
// it as JSON for PT Converter. Positions stay in the session's own sample rate.
//
//   ptx2json session.ptx
//
// {"ok":true,"version":12,"sampleRate":48000,
//  "files":[{"name":"Vox_01.wav","length":123}],
//  "tracks":[{"index":0,"name":"Vox","clips":[{"file":"Vox_01.wav","start":0,"offset":0,"length":123,"region":"Vox_01-01"}]}],
//  "midiTracks":[{"index":0,"name":"Inst 1","clips":[{"start":0,"length":9,"notes":[[60,100,0,480]]}]}]}

#include "ptformat/ptformat.h"
#include <cstdio>
#include <map>
#include <string>
#include <vector>

static std::string esc (const std::string& s)
{
    std::string o;
    for (unsigned char c : s)
    {
        switch (c)
        {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; snprintf (b, sizeof (b), "\\u%04x", c); o += b; }
                else o += (char) c;
        }
    }
    return o;
}

int main (int argc, char** argv)
{
    if (argc < 2) { printf ("{\"ok\":false,\"error\":\"usage: ptx2json session.ptx\"}\n"); return 2; }
    PTFFormat ptf;
    const int r = ptf.load (argv[1], 0);   // 0 = keep the session's own rate (Nova patch)
    if (r != 0)
    {
        const char* why = r == -1 ? "Cannot decrypt the session file"
                        : r == -2 ? "Cannot read the Pro Tools version"
                        : r == -3 ? "Unsupported Pro Tools version"
                        : r == -4 ? "Cannot parse the session" : "Cannot open the session";
        printf ("{\"ok\":false,\"code\":%d,\"error\":\"%s\"}\n", r, why);
        return 1;
    }

    printf ("{\"ok\":true,\"version\":%d,\"sampleRate\":%lld,", ptf.version(), (long long) ptf.sessionrate());

    printf ("\"files\":[");
    bool first = true;
    for (const auto& w : ptf.audiofiles())
    {
        printf ("%s{\"name\":\"%s\",\"length\":%lld}", first ? "" : ",", esc (w.filename).c_str(), (long long) w.length);
        first = false;
    }
    printf ("],");

    // audio tracks: one entry per (track, clip); group by track index keeping order
    std::vector<int> order;
    std::map<int, std::pair<std::string, std::vector<const PTFFormat::track_t*>>> byIndex;
    for (const auto& t : ptf.tracks())
    {
        if (byIndex.find (t.index) == byIndex.end()) order.push_back (t.index);
        auto& e = byIndex[t.index];
        e.first = t.name;
        e.second.push_back (&t);
    }
    printf ("\"tracks\":[");
    first = true;
    for (int idx : order)
    {
        const auto& e = byIndex[idx];
        printf ("%s{\"index\":%d,\"name\":\"%s\",\"clips\":[", first ? "" : ",", idx, esc (e.first).c_str());
        first = false;
        bool fc = true;
        for (const auto* t : e.second)
        {
            if (t->reg.wave.filename.empty()) continue;
            printf ("%s{\"file\":\"%s\",\"start\":%lld,\"offset\":%lld,\"length\":%lld,\"region\":\"%s\"}", fc ? "" : ",",
                    esc (t->reg.wave.filename).c_str(), (long long) t->reg.startpos, (long long) t->reg.sampleoffset,
                    (long long) t->reg.length, esc (t->reg.name).c_str());
            fc = false;
        }
        printf ("]}");
    }
    printf ("],");

    // MIDI tracks
    std::vector<int> morder;
    std::map<int, std::pair<std::string, std::vector<const PTFFormat::track_t*>>> mby;
    for (const auto& t : ptf.miditracks())
    {
        if (mby.find (t.index) == mby.end()) morder.push_back (t.index);
        auto& e = mby[t.index];
        e.first = t.name;
        e.second.push_back (&t);
    }
    printf ("\"midiTracks\":[");
    first = true;
    for (int idx : morder)
    {
        const auto& e = mby[idx];
        printf ("%s{\"index\":%d,\"name\":\"%s\",\"clips\":[", first ? "" : ",", idx, esc (e.first).c_str());
        first = false;
        bool fc = true;
        for (const auto* t : e.second)
        {
            printf ("%s{\"start\":%lld,\"length\":%lld,\"notes\":[", fc ? "" : ",", (long long) t->reg.startpos, (long long) t->reg.length);
            fc = false;
            bool fn = true;
            for (const auto& n : t->reg.midi)
            {
                printf ("%s[%d,%d,%llu,%llu]", fn ? "" : ",", n.note, n.velocity, (unsigned long long) n.pos, (unsigned long long) n.length);
                fn = false;
            }
            printf ("]}");
        }
        printf ("]}");
    }
    printf ("]}\n");
    return 0;
}
