#pragma once

// MasteringPlan <-> XML, the exact format Nova AI Master saves in its state.
// Copied from Nova AI Master/Source/PluginProcessor.cpp (keep the two in step) so
// hosts like Nova Studio's Mix Assistant can hand the plugin a finished plan.

#include <juce_core/juce_core.h>
#include "Decision/NovaMasteringDecisionEngine.h"

namespace NovaMasteringPlanXml
{

    inline std::unique_ptr<juce::XmlElement> planToXml (const NovaMasteringPlan& plan)
    {
        auto xml = std::make_unique<juce::XmlElement> ("MasteringPlan");

        auto* eq = xml->createNewChildElement ("CorrectiveEQ");
        eq->setAttribute ("active", plan.correctiveEQ.active);
        eq->setAttribute ("lowShelfFreq", (double) plan.correctiveEQ.lowShelfFreq);
        eq->setAttribute ("lowShelfDb", (double) plan.correctiveEQ.lowShelfDb);
        eq->setAttribute ("lowMidFreq", (double) plan.correctiveEQ.lowMidFreq);
        eq->setAttribute ("lowMidDb", (double) plan.correctiveEQ.lowMidDb);
        eq->setAttribute ("highMidFreq", (double) plan.correctiveEQ.highMidFreq);
        eq->setAttribute ("highMidDb", (double) plan.correctiveEQ.highMidDb);
        eq->setAttribute ("highShelfFreq", (double) plan.correctiveEQ.highShelfFreq);
        eq->setAttribute ("highShelfDb", (double) plan.correctiveEQ.highShelfDb);

        auto* res = xml->createNewChildElement ("ResonanceControl");
        res->setAttribute ("active", plan.resonanceControl.active);
        res->setAttribute ("amount", (double) plan.resonanceControl.amount);
        res->setAttribute ("timed", plan.resonanceControl.timed);

        auto* comp = xml->createNewChildElement ("Compression");
        comp->setAttribute ("active", plan.compression.active);
        comp->setAttribute ("thresholdDb", (double) plan.compression.thresholdDb);
        comp->setAttribute ("ratio", (double) plan.compression.ratio);
        comp->setAttribute ("attackMs", (double) plan.compression.attackMs);
        comp->setAttribute ("releaseMs", (double) plan.compression.releaseMs);
        comp->setAttribute ("makeupDb", (double) plan.compression.makeupDb);

        auto* sat = xml->createNewChildElement ("Saturation");
        sat->setAttribute ("active", plan.saturation.active);
        sat->setAttribute ("drive", (double) plan.saturation.drive);
        sat->setAttribute ("mix", (double) plan.saturation.mix);

        auto* stereo = xml->createNewChildElement ("Stereo");
        stereo->setAttribute ("active", plan.stereo.active);
        stereo->setAttribute ("widthAmount", (double) plan.stereo.widthAmount);
        stereo->setAttribute ("bassMonoHz", (double) plan.stereo.bassMonoHz);

        auto* clip = xml->createNewChildElement ("Clipping");
        clip->setAttribute ("active", plan.clipping.active);
        clip->setAttribute ("ceilingDb", (double) plan.clipping.ceilingDb);

        auto* lim = xml->createNewChildElement ("Limiting");
        lim->setAttribute ("active", plan.limiting.active);
        lim->setAttribute ("thresholdDb", (double) plan.limiting.thresholdDb);
        lim->setAttribute ("truePeakCeilDb", (double) plan.limiting.truePeakCeilDb);
        lim->setAttribute ("releaseMs", (double) plan.limiting.releaseMs);
        lim->setAttribute ("brickwall", plan.limiting.brickwall);

        auto* out = xml->createNewChildElement ("Output");
        out->setAttribute ("targetLufs", (double) plan.output.targetLufs);
        out->setAttribute ("gainDb", (double) plan.output.gainDb);
        out->setAttribute ("applyGain", plan.output.applyGain);

        return xml;
    }

    inline NovaMasteringPlan planFromXml (const juce::XmlElement& xml)
    {
        NovaMasteringPlan plan;

        if (auto* eq = xml.getChildByName ("CorrectiveEQ"))
        {
            plan.correctiveEQ.active        = eq->getBoolAttribute ("active", plan.correctiveEQ.active);
            plan.correctiveEQ.lowShelfFreq  = (float) eq->getDoubleAttribute ("lowShelfFreq", plan.correctiveEQ.lowShelfFreq);
            plan.correctiveEQ.lowShelfDb    = (float) eq->getDoubleAttribute ("lowShelfDb", plan.correctiveEQ.lowShelfDb);
            plan.correctiveEQ.lowMidFreq    = (float) eq->getDoubleAttribute ("lowMidFreq", plan.correctiveEQ.lowMidFreq);
            plan.correctiveEQ.lowMidDb      = (float) eq->getDoubleAttribute ("lowMidDb", plan.correctiveEQ.lowMidDb);
            plan.correctiveEQ.highMidFreq   = (float) eq->getDoubleAttribute ("highMidFreq", plan.correctiveEQ.highMidFreq);
            plan.correctiveEQ.highMidDb     = (float) eq->getDoubleAttribute ("highMidDb", plan.correctiveEQ.highMidDb);
            plan.correctiveEQ.highShelfFreq = (float) eq->getDoubleAttribute ("highShelfFreq", plan.correctiveEQ.highShelfFreq);
            plan.correctiveEQ.highShelfDb   = (float) eq->getDoubleAttribute ("highShelfDb", plan.correctiveEQ.highShelfDb);
        }

        if (auto* res = xml.getChildByName ("ResonanceControl"))
        {
            plan.resonanceControl.active = res->getBoolAttribute ("active", plan.resonanceControl.active);
            plan.resonanceControl.amount = (float) res->getDoubleAttribute ("amount", plan.resonanceControl.amount);
            plan.resonanceControl.timed  = res->getBoolAttribute ("timed", false);   // older saves: original behaviour
        }

        if (auto* comp = xml.getChildByName ("Compression"))
        {
            plan.compression.active      = comp->getBoolAttribute ("active", plan.compression.active);
            plan.compression.thresholdDb = (float) comp->getDoubleAttribute ("thresholdDb", plan.compression.thresholdDb);
            plan.compression.ratio       = (float) comp->getDoubleAttribute ("ratio", plan.compression.ratio);
            plan.compression.attackMs    = (float) comp->getDoubleAttribute ("attackMs", plan.compression.attackMs);
            plan.compression.releaseMs   = (float) comp->getDoubleAttribute ("releaseMs", plan.compression.releaseMs);
            plan.compression.makeupDb    = (float) comp->getDoubleAttribute ("makeupDb", plan.compression.makeupDb);
        }

        if (auto* sat = xml.getChildByName ("Saturation"))
        {
            plan.saturation.active = sat->getBoolAttribute ("active", plan.saturation.active);
            plan.saturation.drive  = (float) sat->getDoubleAttribute ("drive", plan.saturation.drive);
            plan.saturation.mix    = (float) sat->getDoubleAttribute ("mix", plan.saturation.mix);
        }

        if (auto* stereo = xml.getChildByName ("Stereo"))
        {
            plan.stereo.active      = stereo->getBoolAttribute ("active", plan.stereo.active);
            plan.stereo.widthAmount = (float) stereo->getDoubleAttribute ("widthAmount", plan.stereo.widthAmount);
            plan.stereo.bassMonoHz  = (float) stereo->getDoubleAttribute ("bassMonoHz", plan.stereo.bassMonoHz);
        }

        if (auto* clip = xml.getChildByName ("Clipping"))
        {
            plan.clipping.active    = clip->getBoolAttribute ("active", plan.clipping.active);
            plan.clipping.ceilingDb = (float) clip->getDoubleAttribute ("ceilingDb", plan.clipping.ceilingDb);
        }

        if (auto* lim = xml.getChildByName ("Limiting"))
        {
            plan.limiting.active         = lim->getBoolAttribute ("active", plan.limiting.active);
            plan.limiting.thresholdDb    = (float) lim->getDoubleAttribute ("thresholdDb", plan.limiting.thresholdDb);
            plan.limiting.truePeakCeilDb = (float) lim->getDoubleAttribute ("truePeakCeilDb", plan.limiting.truePeakCeilDb);
            plan.limiting.releaseMs      = (float) lim->getDoubleAttribute ("releaseMs", plan.limiting.releaseMs);
            plan.limiting.brickwall      = lim->getBoolAttribute ("brickwall", false);   // older saves: original limiter
        }

        if (auto* out = xml.getChildByName ("Output"))
        {
            plan.output.targetLufs = (float) out->getDoubleAttribute ("targetLufs", plan.output.targetLufs);
            plan.output.gainDb     = (float) out->getDoubleAttribute ("gainDb", plan.output.gainDb);
            plan.output.applyGain  = out->getBoolAttribute ("applyGain", false);         // older saves: gain never applied
        }

        return plan;
    }
}
