#include "NotesCore.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>
#include <cstdlib>
using namespace NovaNotes;
int main (int, char** argv) {
  juce::AudioFormatManager fm; fm.registerBasicFormats();
  std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (juce::String (argv[1]))));
  const int len = (int) r->lengthInSamples; juce::AudioBuffer<float> b ((int) r->numChannels, len); r->read (&b, 0, len, 0, true, true);
  std::printf ("sr %.0f ch %d len %.2fs\n", r->sampleRate, (int) r->numChannels, len / r->sampleRate);
  auto t = trackPitch (b.getReadPointer (0), len, r->sampleRate); auto ns = segmentNotes (t);
  static const char* N[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
  for (auto& n : ns) { int m = (int) std::lround (n.center); std::printf ("%5.2fs %4.2fs %s%d %+3.0fc\n", n.start*t.hop/r->sampleRate, (n.end-n.start)*t.hop/r->sampleRate, N[m%12], m/12-1, (n.center-m)*100); }
  auto k = detectKey (ns, t); if (argv[2]) { k.tonic = std::atoi (argv[2]); k.minor = false; } std::printf ("key %s %s %.2f\n", N[k.tonic], k.minor?"minor":"major", k.confidence);
  auto ai = ns; aiAssist (t, ai, {}, AssistStyle::tight, k);
  if (argv[2] && argv[3]) {
    const int len2 = len; std::vector<float> x (b.getReadPointer (0), b.getReadPointer (0) + len2), y ((size_t) len2);
    auto ep = findEpochs (x.data(), len2, t);
    render (x.data(), len2, t, ep, ai, y.data());
    auto t2 = trackPitch (y.data(), len2, r->sampleRate);
    if (std::getenv ("NN_DEBUG_AT")) {
      const double at = std::atof (std::getenv ("NN_DEBUG_AT"));
      for (auto& n : ai) if (std::abs (n.start * t.hop / r->sampleRate - at) < 0.01) {
        std::vector<float> sl, fa; decompose (t, n, sl, fa);
        std::printf ("note c=%.2f p=%.2f dr=%.2f\n", n.center, n.pitch, n.drift);
        for (int f = n.start - 4; f < n.end + 4; ++f) std::printf ("  f%d orig %.2f rend %.2f %s slow %.2f\n", f, t.midi[(size_t) f], t2.midi[(size_t) f], (f>=n.start&&f<n.end)?"*":" ", (f>=n.start&&f<n.end)? sl[(size_t)(f-n.start)] : 0.f);
      }
    }
    int inKey = 0, cnt = 0; float worst = 0, sum = 0; const int mask = scaleMask (k);
    for (auto& n : ai) { if (n.end - n.start < 8) continue; const float got = measureCenter (t2, n.start, n.end); const float e = std::abs (got - n.pitch) * 100; sum += e; worst = std::max (worst, e); ++cnt; if (e > 15) std::printf ("   note %.2fs len %.2fs target %.2f got %.2f err %.0fc\n", n.start*t.hop/r->sampleRate, (n.end-n.start)*t.hop/r->sampleRate, n.pitch, got, e); if (mask & (1 << ((int) std::lround (got) % 12))) ++inKey; }
    std::printf ("rendered: %d/%d notes land in key, mean error vs target %.1fc, worst %.1fc\n", inKey, cnt, sum / cnt, worst);
    for (int pass = 0; pass < 2; ++pass) {
      juce::File f (juce::String (argv[3]) + (pass ? "_tuned.wav" : "_original.wav")); f.deleteFile();
      juce::WavAudioFormat wav; std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), r->sampleRate, 1, 24, {}, 0));
      const float* ch[] = { pass ? y.data() : x.data() }; w->writeFromFloatArrays (ch, 1, len2);
    }
  }
  for (size_t i = 0; i < ns.size(); ++i) { int m = (int) std::lround (ns[i].center), q = (int) std::lround (ai[i].pitch); std::printf ("  %s%d%+3.0fc -> %s%d%+3.0fc\n", N[m%12], m/12-1, (ns[i].center-m)*100, N[q%12], q/12-1, (ai[i].pitch-q)*100); }
}
