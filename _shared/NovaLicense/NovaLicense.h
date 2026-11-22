#pragma once

// Shared seat-limited license activation module, used the same way by
// every Nova plugin. See /Users/internationalnova/Nova/nova-store's
// .claude/plans (Phase 2) for the full design.
//
// Flow:
//   1. On editor construction, call isUnlocked(). If false, show an unlock
//      screen (a native function like previewChord/exportMidiFile in
//      Nova Chord's PluginEditor is the established pattern).
//   2. User pastes a license key -> call activate(key, callback).
//   3. activate() does a blocking HTTPS POST to NOVA_LICENSE_SERVER_URL
//      "/api/activate" on a background thread, then calls `callback` back
//      on the message thread with the result.
//   4. On Success, a local cache file is written so future launches call
//      isUnlocked() with NO network round-trip at all.
//
// Security note: the local cache is lightly obfuscated (SHA256-derived
// XOR keystream, keyed by machine ID) to deter casual tampering, not to
// resist a determined reverse-engineer - true of essentially any
// client-side check, since the binary itself can always be patched. The
// real protection is server-side (the seat limit lives in the database,
// not on the user's machine).

#include <functional>

#include <juce_core/juce_core.h>

#ifndef NOVA_LICENSE_SERVER_URL
 #define NOVA_LICENSE_SERVER_URL "http://localhost:3000"
#endif

class NovaLicense
{
public:
    enum class ActivationResult
    {
        Success,
        InvalidKey,
        Revoked,
        SeatLimitReached,
        NetworkError,
        InvalidResponse
    };

    // appId should be short and stable per-plugin (e.g. "nova-sub") -
    // used only to namespace the local cache file, not sent to the server
    // as part of the activation check (the license key itself already
    // ties to a specific plugin server-side).
    explicit NovaLicense (juce::String appId);
    ~NovaLicense();

    // Checks the local cache only - no network call. Call this once at
    // editor/processor startup to decide whether to show the unlock
    // screen.
    bool isUnlocked() const;

    using ActivationCallback = std::function<void (ActivationResult)>;

    // Runs the network call on a background thread; `callback` is invoked
    // back on the message thread. Safe to call from the message thread.
    void activate (const juce::String& licenseKey, ActivationCallback callback);

private:
    juce::String appId;

    juce::File getCacheFile() const;
    juce::String getMachineId() const;
    juce::String getObfuscationKey (const juce::String& machineId) const;

    bool writeCache (const juce::String& licenseKey, const juce::String& machineId) const;
    bool readCache (juce::String& outLicenseKey, juce::String& outMachineId) const;

    struct ActivationThread;
    std::unique_ptr<ActivationThread> activeThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaLicense)
};
