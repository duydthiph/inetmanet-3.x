//
// Copyright (C) 2016 OpenSim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see http://www.gnu.org/licenses/.
//

#ifndef __INET_IRX_H
#define __INET_IRX_H

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/contract/packetlevel/IRadio.h"

namespace inet {

class MACAddress;

namespace ieee80211 {

class Ieee80211Frame;
class IContention;
class IStatistics;

using namespace inet::physicallayer;  //TODO Khmm

/**
 * Abstract interface for Rx processes. The Rx process checks received frames for
 * errors, manages the NAV, and notifies Tx processes about the channel state
 * (free or busy). The channel is free only if it is free according to both
 * the physical (CCA) and the virtual (NAV-based) carrier sense algorithms.
 * Correctly received frames are sent up to UpperMac (see IUpperMac), corrupted
 * frames are discarded. Tx processes are also notified about corrupted and
 * correctly received frames. so they can switch between using DIFS/AIFS and EIFS
 * according to the channel access procedure.
 */
class INET_API IRx
{
    public:
        virtual ~IRx() {}

        virtual IStatistics *getStatistics() = 0;

        virtual bool isReceptionInProgress() const = 0;

        // from Contention
        virtual bool isMediumFree() const = 0;
        virtual void frameTransmitted(simtime_t durationField) = 0;

        // from Coordination functions
        virtual void registerContention(IContention *contention) = 0;

        // events
        virtual void receptionStateChanged(IRadio::ReceptionState state) = 0;
        virtual void transmissionStateChanged(IRadio::TransmissionState state) = 0;
        virtual void receivedSignalPartChanged(IRadioSignal::SignalPart part) = 0;
        virtual bool lowerFrameReceived(Ieee80211Frame *frame) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif

