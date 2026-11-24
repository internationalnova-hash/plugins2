#include "NovaLicense.h"

#include <juce_product_unlocking/juce_product_unlocking.h>
#include <juce_cryptography/juce_cryptography.h>

namespace
{
    const juce::String kAllAccessId ("all-access");

    // XOR-obfuscates `data` in place against a repeating SHA256 keystream
    // derived from `key`. Symmetric (same function encrypts and decrypts).
    void xorObfuscate (juce::MemoryBlock& data, const juce::String& key)
    {
        const juce::SHA256 hash (key.toUTF8());
        const auto digestBlock = hash.getRawData(); // 32 bytes
        const auto* digest = (const uint8_t*) digestBlock.getData();
        const size_t digestSize = digestBlock.getSize();

        auto* bytes = (uint8_t*) data.getData();
        for (size_t i = 0; i < data.getSize(); ++i)
            bytes[i] = (uint8_t) (bytes[i] ^ digest[i % digestSize]);
    }

    // POST JSON to the store; returns the parsed reply (void var on network failure).
    juce::var postJson (const juce::String& path, const juce::var& payload, int& statusCode)
    {
        juce::URL url (juce::String (NOVA_LICENSE_SERVER_URL) + path);
        url = url.withPOSTData (juce::JSON::toString (payload, true));
        const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                                 .withExtraHeaders ("Content-Type: application/json")
                                 .withConnectionTimeoutMs (8000)
                                 .withStatusCode (&statusCode);
        juce::String body;
        if (auto stream = url.createInputStream (options))
            body = stream->readEntireStreamAsString();
        return statusCode > 0 ? juce::JSON::parse (body) : juce::var();
    }

    juce::Time parseTime (const juce::var& v)
    {
        const auto s = v.toString();
        return s.isEmpty() ? juce::Time() : juce::Time::fromISO8601 (s);
    }
}

//==============================================================================
struct NovaLicense::ActivationThread : public juce::Thread
{
    ActivationThread (NovaLicense& ownerIn, juce::String keyIn, ActivationCallback callbackIn)
        : juce::Thread ("NovaLicenseActivation"),
          owner (ownerIn), licenseKey (std::move (keyIn)), callback (std::move (callbackIn)) {}

    void run() override
    {
        const auto machineId = owner.getMachineId();
        juce::DynamicObject::Ptr payload = new juce::DynamicObject();
        payload->setProperty ("licenseKey", licenseKey);
        payload->setProperty ("machineId", machineId);
        payload->setProperty ("pluginId", owner.appId);

        int status = 0;
        const auto json = postJson ("/api/activate", juce::var (payload.get()), status);
        ActivationResult result = ActivationResult::NetworkError;

        if (status > 0)
        {
            if (auto* obj = json.getDynamicObject())
            {
                if ((bool) obj->getProperty ("ok"))
                {
                    CacheEntry e;
                    e.licenseKey = licenseKey;
                    e.machineId = machineId;
                    e.kind = obj->getProperty ("kind").toString().isNotEmpty() ? obj->getProperty ("kind").toString() : juce::String ("perpetual");
                    e.plugin = obj->getProperty ("plugin").toString();
                    e.expires = parseTime (obj->getProperty ("expiresAt"));
                    e.lastCheck = juce::Time::getCurrentTime();
                    // All Access goes in the shared cache, so every Nova plugin sees it
                    const auto id = e.plugin == kAllAccessId ? kAllAccessId : owner.appId;
                    result = owner.writeCache (id, e) ? ActivationResult::Success : ActivationResult::InvalidResponse;
                }
                else
                {
                    const auto reason = obj->getProperty ("reason").toString();
                    if (reason == "invalid_key")             result = ActivationResult::InvalidKey;
                    else if (reason == "revoked")            result = ActivationResult::Revoked;
                    else if (reason == "seat_limit_reached") result = ActivationResult::SeatLimitReached;
                    else if (reason == "expired")            result = ActivationResult::Expired;
                    else if (reason == "wrong_plugin")       result = ActivationResult::WrongPlugin;
                    else                                     result = ActivationResult::InvalidResponse;
                }
            }
            else
            {
                result = ActivationResult::InvalidResponse;
            }
        }

        juce::MessageManager::callAsync ([cb = callback, result] { if (cb) cb (result); });
    }

    NovaLicense& owner;
    juce::String licenseKey;
    ActivationCallback callback;
};

//==============================================================================
struct NovaLicense::CheckThread : public juce::Thread
{
    CheckThread (NovaLicense& ownerIn, CacheEntry entryIn, juce::String cacheIdIn, std::function<void()> changed)
        : juce::Thread ("NovaLicenseCheck"), owner (ownerIn), entry (std::move (entryIn)),
          cacheId (std::move (cacheIdIn)), onChanged (std::move (changed)) {}

    void run() override
    {
        juce::DynamicObject::Ptr payload = new juce::DynamicObject();
        payload->setProperty ("licenseKey", entry.licenseKey);
        payload->setProperty ("machineId", entry.machineId);
        payload->setProperty ("pluginId", owner.appId);

        int status = 0;
        const auto json = postJson ("/api/license/check", juce::var (payload.get()), status);
        auto* obj = json.getDynamicObject();
        if (status <= 0 || obj == nullptr || status >= 500) return;    // offline or server trouble: try again later

        const bool before = entryUnlocks (entry);
        if ((bool) obj->getProperty ("ok"))
        {
            entry.expires = parseTime (obj->getProperty ("expiresAt"));
            entry.lastCheck = juce::Time::getCurrentTime();
            owner.writeCache (cacheId, entry);
        }
        else
        {
            // Ended, revoked or no longer activated here: forget the key
            cacheFileFor (cacheId).deleteFile();
            entry.valid = false;
        }

        if (before != (entry.valid && entryUnlocks (entry)))
            juce::MessageManager::callAsync ([cb = onChanged] { if (cb) cb(); });
    }

    NovaLicense& owner;
    CacheEntry entry;
    juce::String cacheId;
    std::function<void()> onChanged;
};

//==============================================================================
NovaLicense::NovaLicense (juce::String appIdIn) : appId (std::move (appIdIn)) {}

NovaLicense::~NovaLicense()
{
    // Let a running network call finish (it slightly exceeds the 8 s timeout).
    if (activeThread != nullptr) activeThread->stopThread (9000);
    if (checkThread != nullptr)  checkThread->stopThread (9000);
}

juce::File NovaLicense::cacheFileFor (const juce::String& id)
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("International Nova")
        .getChildFile (id + ".novalicense");
}

juce::String NovaLicense::getMachineId() const
{
    return juce::OnlineUnlockStatus::MachineIDUtilities::getUniqueMachineID();
}

juce::String NovaLicense::obfuscationKey (const juce::String& id, const juce::String& machineId)
{
    // Only deters hand-editing; ties the cache to this machine so it can't be copied.
    return "NovaLicense::" + id + "::" + machineId;
}

bool NovaLicense::writeCache (const juce::String& id, const CacheEntry& e) const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty ("licenseKey", e.licenseKey);
    obj->setProperty ("machineId", e.machineId);
    obj->setProperty ("kind", e.kind);
    obj->setProperty ("plugin", e.plugin);
    obj->setProperty ("expiresAt", e.expires.toMilliseconds() > 0 ? e.expires.toISO8601 (true) : juce::String());
    obj->setProperty ("lastCheck", e.lastCheck.toISO8601 (true));
    obj->setProperty ("activatedAt", juce::Time::getCurrentTime().toISO8601 (true));

    const auto plainText = juce::JSON::toString (juce::var (obj.get()), true);
    juce::MemoryBlock block (plainText.toUTF8(), plainText.getNumBytesAsUTF8());
    xorObfuscate (block, obfuscationKey (id, e.machineId));

    const auto file = cacheFileFor (id);
    if (! file.getParentDirectory().createDirectory())
        return false;
    return file.replaceWithData (block.getData(), block.getSize());
}

NovaLicense::CacheEntry NovaLicense::readCache (const juce::String& id) const
{
    CacheEntry e;
    const auto file = cacheFileFor (id);
    if (! file.existsAsFile()) return e;

    juce::MemoryBlock block;
    if (! file.loadFileAsData (block) || block.getSize() == 0) return e;

    // Decrypts to valid JSON only on the machine that wrote it.
    const auto currentMachineId = getMachineId();
    xorObfuscate (block, obfuscationKey (id, currentMachineId));
    const auto json = juce::JSON::parse (juce::String::fromUTF8 ((const char*) block.getData(), (int) block.getSize()));
    auto* obj = json.getDynamicObject();
    if (obj == nullptr) return e;

    e.licenseKey = obj->getProperty ("licenseKey").toString();
    e.machineId = obj->getProperty ("machineId").toString();
    e.kind = obj->getProperty ("kind").toString();
    if (e.kind.isEmpty()) e.kind = "perpetual";                 // caches written before subscriptions existed
    e.plugin = obj->getProperty ("plugin").toString();
    e.expires = parseTime (obj->getProperty ("expiresAt"));
    e.lastCheck = parseTime (obj->getProperty ("lastCheck"));
    e.valid = e.licenseKey.isNotEmpty() && e.machineId == currentMachineId;
    return e;
}

bool NovaLicense::entryUnlocks (const CacheEntry& e)
{
    if (! e.valid) return false;
    if (e.kind != "subscription") return true;
    return juce::Time::getCurrentTime() < e.expires + juce::RelativeTime::days (kOfflineGraceDays);
}

NovaLicense::Status NovaLicense::getStatus() const
{
    Status s;
    const auto own = readCache (appId);
    if (entryUnlocks (own))
    {
        s.unlocked = true;
        s.licenseKey = own.licenseKey;
        if (own.kind == "subscription") { s.allAccess = true; s.expires = own.expires; }
        return s;
    }
    const auto shared = readCache (kAllAccessId);
    if (entryUnlocks (shared))
    {
        s.unlocked = true;
        s.allAccess = true;
        s.licenseKey = shared.licenseKey;
        s.expires = shared.expires;
    }
    return s;
}

bool NovaLicense::isUnlocked() const
{
    return getStatus().unlocked;
}

void NovaLicense::activate (const juce::String& licenseKey, ActivationCallback callback)
{
    if (activeThread != nullptr) activeThread->stopThread (9000);
    activeThread = std::make_unique<ActivationThread> (*this, licenseKey.trim().toUpperCase(), std::move (callback));
    activeThread->startThread();
}

void NovaLicense::refreshIfDue (std::function<void()> onChanged)
{
    if (checkThread != nullptr && checkThread->isThreadRunning()) return;

    for (const auto& id : { appId, kAllAccessId })
    {
        const auto e = readCache (id);
        if (! e.valid || e.kind != "subscription") continue;
        const auto now = juce::Time::getCurrentTime();
        const bool due = now - e.lastCheck > juce::RelativeTime::days (kRecheckDays)
                      || now > e.expires - juce::RelativeTime::days (2);
        if (! due) continue;

        checkThread = std::make_unique<CheckThread> (*this, e, id, std::move (onChanged));
        checkThread->startThread();
        return;
    }
}
