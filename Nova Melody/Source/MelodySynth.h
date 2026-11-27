#pragma once

// Nova Melody's built-in preview sounds: small, clean and cheap. Polyphonic for
// Pluck / Keys / Lead, mono with glide for Sub Bass and 808 (so 808 slides work).
// Audio thread only: noteOn/noteOff/render, no allocation after prepare().

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

namespace NovaMelody
{
    enum Sound { SoftPluck, Keys, LeadSound, SubBass, Sound808, kNumSounds };
    inline const char* soundName (int s)
    {
        static const char* n[kNumSounds] = { "Soft Pluck", "Keys", "Lead", "Sub Bass", "808" };
        return n[juce::jlimit (0, kNumSounds - 1, s)];
    }

    class MelodySynth
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate; reset(); }
        void reset() { for (auto& v : voices) v = {}; monoActive = false; }
        void setSound (int s) { if (s != sound) { allOff(); sound = juce::jlimit (0, kNumSounds - 1, s); } }
        bool isMono() const noexcept { return sound == SubBass || sound == Sound808; }

        void noteOn (int pitch, float vel, bool glide)
        {
            if (isMono())
            {
                auto& v = voices[0];
                const bool slide = (glide || (sound == Sound808 && v.active && ! v.releasing)) && v.active;
                v.targetHz = hz (pitch);
                if (! slide || ! v.active) { v.hz = v.targetHz; v.phase = 0; v.env = 0; v.t = 0; v.drop = sound == Sound808 ? 1.0 : 0.0; }
                v.glideCoef = slide ? 1.0 - std::exp (-1.0 / (0.07 * sr)) : 1.0;
                v.pitch = pitch; v.vel = vel; v.active = true; v.releasing = false;
                return;
            }
            Voice* free = nullptr;
            for (auto& v : voices) if (! v.active) { free = &v; break; }
            if (free == nullptr)
            {
                free = &voices[0];
                for (auto& v : voices) if (v.t > free->t) free = &v;    // steal the oldest
            }
            *free = {};
            free->pitch = pitch; free->hz = free->targetHz = hz (pitch); free->vel = vel; free->active = true;
        }

        void noteOff (int pitch)
        {
            for (auto& v : voices) if (v.active && v.pitch == pitch) v.releasing = true;
        }

        void allOff() { for (auto& v : voices) if (v.active) v.releasing = true; }

        /// adds into the buffer
        void render (float* L, float* R, int n, float gain)
        {
            for (auto& v : voices)
            {
                if (! v.active) continue;
                for (int i = 0; i < n; ++i)
                {
                    const float s = next (v) * gain;
                    L[i] += s; if (R != L) R[i] += s;
                    if (! v.active) break;
                }
            }
        }

    private:
        struct Voice
        {
            bool active = false, releasing = false;
            int pitch = 60;
            double hz = 440, targetHz = 440, glideCoef = 1, phase = 0, phase2 = 0, t = 0, drop = 0;
            float vel = 0.8f, env = 0, lp = 0, lp2 = 0;
        };

        static double hz (int pitch) { return 440.0 * std::pow (2.0, (pitch - 69) / 12.0); }

        float next (Voice& v)
        {
            const double dt = 1.0 / sr;
            v.t += dt;
            v.hz += (v.targetHz - v.hz) * v.glideCoef;
            double f = v.hz;
            if (sound == Sound808) { f *= 1.0 + 1.2 * v.drop; v.drop *= std::exp (-dt / 0.012); }
            if (sound == LeadSound) f *= 1.0 + 0.004 * std::sin (2 * juce::MathConstants<double>::pi * 5.2 * v.t) * std::min (1.0, v.t / 0.3);
            v.phase += f * dt; if (v.phase >= 1) v.phase -= 1;
            v.phase2 += f * 1.003 * dt; if (v.phase2 >= 1) v.phase2 -= 1;
            const double p = v.phase;

            // envelope (attack, decay to sustain, release)
            float a = 0.004f, d = 0.3f, sus = 0.f, r = 0.08f;
            switch (sound)
            {
                case SoftPluck: a = 0.002f; d = 0.45f; sus = 0.f;  r = 0.12f; break;
                case Keys:      a = 0.003f; d = 1.4f;  sus = 0.15f; r = 0.25f; break;
                case LeadSound: a = 0.012f; d = 0.3f;  sus = 0.75f; r = 0.10f; break;
                case SubBass:   a = 0.005f; d = 0.4f;  sus = 0.85f; r = 0.06f; break;
                case Sound808:  a = 0.001f; d = 2.2f;  sus = 0.0f;  r = 0.15f; break;
                default: break;
            }
            if (v.releasing) v.env -= v.env * (float) (dt / r) * 3.f;
            else if (v.t < a) v.env = (float) (v.t / a);
            else v.env += (sus - v.env) * (float) (dt / d) * 2.f;
            if (v.releasing && v.env < 1.0e-4f) { v.active = false; return 0.f; }
            if (! v.releasing && sus == 0.f && v.t > a && v.env < 1.0e-4f) { v.active = false; return 0.f; }

            const double tw = 2 * juce::MathConstants<double>::pi;
            float s = 0.f;
            switch (sound)
            {
                case SoftPluck:
                {
                    const float saw = (float) (2.0 * p - 1.0), sq = p < 0.5 ? 1.f : -1.f;
                    const float raw = 0.6f * saw + 0.3f * sq;
                    const float cut = juce::jlimit (0.02f, 0.9f, 0.05f + 0.6f * v.env);      // brightness follows the pluck
                    v.lp += cut * (raw - v.lp); v.lp2 += cut * (v.lp - v.lp2);
                    s = v.lp2 * 1.6f;
                    break;
                }
                case Keys:
                    s = (float) (std::sin (tw * p) + 0.35 * std::sin (tw * 2 * p) * std::exp (-v.t * 3) + 0.12 * std::sin (tw * 3 * p) * std::exp (-v.t * 6)) * 0.42f;
                    break;
                case LeadSound:
                {
                    const float raw = (float) ((2.0 * p - 1.0) + (2.0 * v.phase2 - 1.0)) * 0.5f;
                    v.lp += 0.18f * (raw - v.lp);
                    s = v.lp * 0.5f;
                    break;
                }
                case SubBass:
                    s = (float) std::tanh (1.4 * std::sin (tw * p)) * 0.55f;
                    break;
                case Sound808:
                    s = (float) std::tanh (2.2 * std::sin (tw * p)) * 0.5f;
                    break;
                default: break;
            }
            return s * v.env * (0.35f + 0.65f * v.vel);
        }

        double sr = 48000;
        int sound = SoftPluck;
        bool monoActive = false;
        std::array<Voice, 12> voices;
    };
}
