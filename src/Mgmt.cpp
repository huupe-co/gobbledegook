// Copyright 2017-2019 Paul Nettle
//
// This file is part of Gobbledegook.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file in the root of the source tree.

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// >>
// >>>  INSIDE THIS FILE
// >>
//
// This file contains various functions for interacting with Bluetooth Management interface, which provides adapter
// configuration.
//
// >>
// >>>  DISCUSSION
// >>
//
// We only cover the basics here. If there are configuration features you need that aren't supported (such as
// configuring BR/EDR), then this would be a good place for them.
//
// Note that this class relies on the `HciAdapter`, which is a very primitive implementation. Use with caution.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include <string.h>

#include "Logger.h"
#include "Mgmt.h"
#include "Utils.h"

namespace ggk {

// Construct the Mgmt device
//
// Set `controllerIndex` to the zero-based index of the device as recognized by the OS. If this parameter is omitted,
// the index of the first device (0) will be used.
Mgmt::Mgmt(uint16_t controllerIndex) :
    controllerIndex(controllerIndex) {
    HciAdapter::getInstance().sync(controllerIndex);
}

// Set the adapter name and short name
//
// The inputs `name` and `shortName` may be truncated prior to setting them on the adapter. To ensure that `name` and
// `shortName` conform to length specifications prior to calling this method, see the constants
// `kMaxAdvertisingNameLength` and `kMaxAdvertisingShortNameLength`. In addition, the static methods `truncateName()`
// and `truncateShortName()` may be helpful.
//
// Returns true on success, otherwise false
bool Mgmt::setName(std::string name, std::string shortName) {
    // Ensure their lengths are okay
    name = truncateName(name);
    shortName = truncateShortName(shortName);

    struct SRequest : HciAdapter::HciHeader {
        char name[249];
        char shortName[11];
    } __attribute__((packed));

    SRequest request;
    request.code = Mgmt::ESetLocalNameCommand;
    request.controllerId = controllerIndex;
    request.dataSize = sizeof(SRequest) - sizeof(HciAdapter::HciHeader);

    memset(request.name, 0, sizeof(request.name));
    snprintf(request.name, sizeof(request.name), "%s", name.c_str());

    memset(request.shortName, 0, sizeof(request.shortName));
    snprintf(request.shortName, sizeof(request.shortName), "%s", shortName.c_str());

    if (!HciAdapter::getInstance().sendCommand(request)) {
        Logger::warn(SSTR << "  + Failed to set name");
        return false;
    }

    return true;
}

// Sets discoverable mode
// 0x00 disables discoverable
// 0x01 enables general discoverable
// 0x02 enables limited discoverable
// Timeout is the time in seconds. For 0x02, the timeout value is required.
bool Mgmt::setDiscoverable(uint8_t disc, uint16_t timeout) {
    struct SRequest : HciAdapter::HciHeader {
        uint8_t disc;
        uint16_t timeout;
    } __attribute__((packed));

    SRequest request;
    request.code = Mgmt::ESetDiscoverableCommand;
    request.controllerId = controllerIndex;
    request.dataSize = sizeof(SRequest) - sizeof(HciAdapter::HciHeader);
    request.disc = disc;
    request.timeout = timeout;

    if (!HciAdapter::getInstance().sendCommand(request)) {
        Logger::warn(SSTR << "  + Failed to set discoverable");
        return false;
    }

    return true;
}

// Set a setting state to 'newState'
//
// Many settings are set the same way, this is just a convenience routine to handle them all
//
// Returns true on success, otherwise false
bool Mgmt::setState(uint16_t commandCode, uint16_t controllerId, uint8_t newState) {
    struct SRequest : HciAdapter::HciHeader {
        uint8_t state;
    } __attribute__((packed));

    SRequest request;
    request.code = commandCode;
    request.controllerId = controllerId;
    request.dataSize = sizeof(SRequest) - sizeof(HciAdapter::HciHeader);
    request.state = newState;

    if (!HciAdapter::getInstance().sendCommand(request)) {
        Logger::warn(
            SSTR << "  + Failed to set " << HciAdapter::kCommandCodeNames[commandCode]
                 << " state to: " << static_cast<int>(newState)
        );
        return false;
    }

    return true;
}

// Set the powered state to `newState` (true = powered on, false = powered off)
//
// Returns true on success, otherwise false
bool Mgmt::setPowered(bool newState) {
    return setState(Mgmt::ESetPoweredCommand, controllerIndex, newState ? 1 : 0);
}

// Set the BR/EDR state to `newState` (true = enabled, false = disabled)
//
// Returns true on success, otherwise false
bool Mgmt::setBredr(bool newState) {
    return setState(Mgmt::ESetBREDRCommand, controllerIndex, newState ? 1 : 0);
}

// Set the Secure Connection state (0 = disabled, 1 = enabled, 2 = secure connections only mode)
//
// Returns true on success, otherwise false
bool Mgmt::setSecureConnections(uint8_t newState) {
    return setState(Mgmt::ESetSecureConnectionsCommand, controllerIndex, newState);
}

// Set the bondable state to `newState` (true = enabled, false = disabled)
//
// Returns true on success, otherwise false
bool Mgmt::setBondable(bool newState) {
    return setState(Mgmt::ESetBondableCommand, controllerIndex, newState ? 1 : 0);
}

// Set the connectable state to `newState` (true = enabled, false = disabled)
//
// Returns true on success, otherwise false
bool Mgmt::setConnectable(bool newState) {
    return setState(Mgmt::ESetConnectableCommand, controllerIndex, newState ? 1 : 0);
}

// Set the LE state to `newState` (true = enabled, false = disabled)
//
// Returns true on success, otherwise false
bool Mgmt::setLE(bool newState) {
    return setState(Mgmt::ESetLowEnergyCommand, controllerIndex, newState ? 1 : 0);
}

// Set the advertising state to `newState` (0 = disabled, 1 = enabled (with consideration towards the connectable
// setting), 2 = enabled in connectable mode).
//
// Returns true on success, otherwise false
bool Mgmt::setAdvertising(uint8_t newState) {
    return setState(Mgmt::ESetAdvertisingCommand, controllerIndex, newState);
}

// Start advertising with custom data
// Advertisement packet will contain: flags, shortName, uuid
bool Mgmt::addAdvertising(std::string shortName, const uint16_t *uuid) {
    static std::string kShortName = shortName;
    static const uint16_t kUuid = *uuid;
    if (uuid == nullptr) {
        shortName = kShortName;
        uuid = &kUuid;
    }
    Logger::debug(SSTR << "addAdvertising()" << shortName << ": " << *uuid);
    constexpr size_t ADVERTISING_SHORTNAME_MAX_LEN = 8;
    constexpr size_t ADVERTISING_UUID_LEN = 2;
    constexpr size_t ADVERTISING_MAX_DATALEN = 2 + ADVERTISING_SHORTNAME_MAX_LEN + 2 + ADVERTISING_UUID_LEN;

    struct SRequest : HciAdapter::HciHeader {
        uint8_t instance;
        uint32_t flags;
        uint16_t duration;
        uint16_t timeout;
        uint8_t advDataLen;
        uint8_t scanRspLen;
        uint8_t data[ADVERTISING_MAX_DATALEN];
    } __attribute__((packed));

    size_t shortNameEffectiveLen = std::min(ADVERTISING_SHORTNAME_MAX_LEN, shortName.length());

    SRequest request;
    request.code = Mgmt::EAddAdvertisingCommand;
    request.controllerId = controllerIndex;
    request.dataSize =
        sizeof(SRequest) - sizeof(HciAdapter::HciHeader) - (ADVERTISING_SHORTNAME_MAX_LEN - shortNameEffectiveLen);

    request.instance = 1u;
    request.flags = 3u; // Connectable && Discoverable, see Bluez/lib/mgmt.h
    request.duration = 0;
    request.timeout = 0;

    request.advDataLen = ADVERTISING_MAX_DATALEN - (ADVERTISING_SHORTNAME_MAX_LEN - shortNameEffectiveLen);
    request.scanRspLen = 0;

    request.data[0] = static_cast<uint8_t>(ADVERTISING_UUID_LEN + 1);
    request.data[1] = 0x03; // Incomplete UUID list
    memcpy(&request.data[2], uuid, ADVERTISING_UUID_LEN);

    request.data[2 + ADVERTISING_UUID_LEN] = static_cast<uint8_t>(1 + shortNameEffectiveLen);
    request.data[3 + ADVERTISING_UUID_LEN] = 0x08; // Set short name
    memset(&request.data[4 + ADVERTISING_UUID_LEN], 0, ADVERTISING_SHORTNAME_MAX_LEN);
    memcpy(&request.data[4 + ADVERTISING_UUID_LEN], shortName.c_str(), shortNameEffectiveLen);

    if (!HciAdapter::getInstance().sendCommand(request)) {
        Logger::warn(SSTR << "  + Failed to start advertising with UUID");
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Utilitarian
// ---------------------------------------------------------------------------------------------------------------------------------

// Truncates the string `name` to the maximum allowed length for an adapter name. If `name` needs no truncation, a copy
// of `name` is returned.
std::string Mgmt::truncateName(const std::string &name) {
    if (name.length() <= kMaxAdvertisingNameLength) {
        return name;
    }

    return name.substr(0, kMaxAdvertisingNameLength);
}

// Truncates the string `name` to the maximum allowed length for an adapter short-name. If `name` needs no truncation, a
// copy of `name` is returned.
std::string Mgmt::truncateShortName(const std::string &name) {
    if (name.length() <= kMaxAdvertisingShortNameLength) {
        return name;
    }

    return name.substr(0, kMaxAdvertisingShortNameLength);
}

}; // namespace ggk
