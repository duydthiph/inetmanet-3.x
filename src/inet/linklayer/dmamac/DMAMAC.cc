//**************************************************************************
// * file:        DMAMAC main file
// *
// * author:      A. Ajith Kumar S.
// * copyright:   (c) A. Ajith Kumar S. 
// * homepage:    www.hib.no/ansatte/aaks
// * email:       aji3003 @ gmail.com
// **************************************************************************
// * part of:     A dual mode adaptive MAC (DMAMAC) protocol for WSAN.
// * Refined on:  25-Apr-2015
// **************************************************************************
// *This file is part of DMAMAC (DMAMAC Protocol Implementation on MiXiM-OMNeT).
// *
// *DMAMAC is free software: you can redistribute it and/or modify
// *it under the terms of the GNU General Public License as published by
// *the Free Software Foundation, either version 3 of the License, or
// *(at your option) any later version.
// *
// *DMAMAC is distributed in the hope that it will be useful,
// *but WITHOUT ANY WARRANTY; without even the implied warranty of
// *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// *GNU General Public License for more details.
// *
// *You should have received a copy of the GNU General Public License
// *along with DMAMAC.  If not, see <http://www.gnu.org/licenses/>./
// **************************************************************************

#include <cstdlib>
#include "inet/linklayer/dmamac/DMAMAC.h"
#include "inet/common/INETUtils.h"
#include "inet/common/INETMath.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/FindModule.h"
#include "inet/linklayer/common/SimpleLinkLayerControlInfo.h"
#include "inet/physicallayer/contract/packetlevel/RadioControlInfo_m.h"


namespace inet {

Define_Module(DMAMAC);

/* @brief To set the nodeId for the current node  */
#define myId ((myMacAddr.getInt() & 0xFFFF)-1)

/* @brief Got this from BaseLayer file, to catch the signal for hostState change */
// const simsignal_t DMAMAC::catHostStateSignal = simsignal_t(MIXIM_SIGNAL_HOSTSTATE_NAME);

/* @brief Initialize the MAC protocol using omnetpp.ini variables and initializing other necessary variables  */

void DMAMAC::initializeMACAddress()
{
    const char *addrstr = par("address");

    if (!strcmp(addrstr, "auto")) {
        // assign automatic address
        myMacAddr = MACAddress::generateAutoAddress();

        // change module parameter from "auto" to concrete address
        par("address").setStringValue(myMacAddr.str().c_str());
    }
    else {
        myMacAddr.setAddress(addrstr);
    }
}

InterfaceEntry *DMAMAC::createInterfaceEntry()
{
    InterfaceEntry *e = new InterfaceEntry(this);

    // data rate
    e->setDatarate(bitrate);

    // generate a link-layer address to be used as interface token for IPv6
    e->setMACAddress(myMacAddr);
    e->setInterfaceToken(myMacAddr.formInterfaceIdentifier());

    // capabilities
    e->setMtu(par("mtu").longValue());
    e->setMulticast(false);
    e->setBroadcast(true);

    return e;
}

void DMAMAC::initialize(int stage)
{
    MACProtocolBase::initialize(stage);

    if(stage == INITSTAGE_LOCAL)
    {
        EV << "¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤MAC Initialization in process¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤ " << endl;

        /* @brief For Droppedpacket code used */

        /* @brief Signal handling for node death indication as HostState */
        // catHostStateSignal.initialize();

        /* @brief Getting parameters from ini file and setting MAC variables DMA-MAC  */
        /*@{*/
        queueLength         = par("queueLength");
        slotDuration        = par("slotDuration");
        bitrate             = par("bitrate");
        headerLength        = par("headerLength");
        numSlotsTransient   = par("numSlotsTransient");
        numSlotsSteady      = par("numSlotsSteady");
        txPower             = par("txPower");
        ackTimeoutValue     = par("ackTimeout");
        dataTimeoutValue    = par("dataTimeout");
        alertTimeoutValue   = par("alertTimeout");                  
        xmlFileSteady       = par("xmlFileSteadyN");
        xmlFileTransient    = par("xmlFileTransientN");
        stateProbability    = par("stateProbability");
        alertProbability    = par("alertProbability");
        stats               = par("stats");                         // @Statistics recording switch ON/OFF
        alertDelayMax       = par("alertDelayMax");                 // Tune the max delay in sending alert packets
        maxRadioSwitchDelay = par("maxRadioSwitchDelay");
        sinkAddress         = MACAddress( par("sinkAddress").longValue());
        isActuator          = par("isActuator");                    
        maxNodes            = par("maxNodes");
        maxChildren         = par("maxChildren");
        hasSensorChild      = par("hasSensorChild");                                     
        double temp         = (par("macTypeInput").doubleValue());                       
        if(temp == 0)
            macType = DMAMAC::HYBRID;
        else
            macType = DMAMAC::TDMA;
        /*@}*/

        /* @brief @Statistics collection */
        /*@{*/
        nbTxData            = 0;
        nbTxDataFailures    = 0;
        nbTxAcks            = 0;
        nbTxAlert           = 0;
        nbTxSlots           = 0;
        nbTxNotifications   = 0;
        nbRxData            = 0;
        nbRxAlert    = 0;
        nbRxNotifications   = 0;
        //nbMissedAcks        = 0;
        nbRxAcks            = 0;
        nbSleepSlots        = 0;
        nbCollisions        = 0;
        nbDroppedDataPackets= 0;
        nbTransient         = 0;
        nbSteady            = 0;
        nbSteadyToTransient = 0;
        nbTransientToSteady = 0;
        nbFailedSwitch      = 0;
        nbMidSwitch         = 0;
        nbSkippedAlert      = 0;
        nbForwardedAlert    = 0;      
        nbDiscardedAlerts   = 0;      
        randomNumber        = 0;
        nbTimeouts          = 0;
        nbAlertRxSlots      = 0;        
        /*@}*/

        /* @brief Other initializations */
        /*@{*/
        maxNumSlots             = numSlotsSteady;
        nextSlot                = 0;
        txAttempts              = 0;                        // Number of retransmission attempts
        currentMacState         = STARTUP;
        previousMacMode         = STEADY;
        currentMacMode          = TRANSIENT;
        changeMacMode           = false;
        sendAlertMessage        = false;
        alertMessageFromDown    = false;                    // Indicates alert received from children
        checkForSuperframeChange= false;
        forChildNode            = false;
        /*@}*/

        /* @brief How long does it take to send/receive a control packet */
        controlDuration = ((double)headerLength + (double)numSlots + 16) / (double)bitrate;
        EV_DETAIL << "Control packets take : " << controlDuration << " seconds to transmit\n";

        cModule *radioModule = getModuleFromPar<cModule>(par("radioModule"), this);
        radioModule->subscribe(IRadio::radioModeChangedSignal, this);
        radioModule->subscribe(IRadio::transmissionStateChangedSignal, this);
        radioModule->subscribe(IRadio::receptionStateChangedSignal, this);
        radio = check_and_cast<IRadio *>(radioModule);


        initializeMACAddress();
        registerInterface();

        EV << "My Mac address is" << myMacAddr << endl;

        /* @brief copies all xml data for slots in steady and transient and neighbor data */
        slotInitialize();
    }
    else if(stage == INITSTAGE_LINK_LAYER) {

        EV << " QueueLength  = " << queueLength
        << " slotDuration  = "  << slotDuration
        << " controlDuration= " << controlDuration
        << " numSlots       = " << numSlots
        << " bitrate        = " << bitrate
        << " stateProbability = " << stateProbability
        << " alertProbability = " << alertProbability
        << " macType = "  << macType
        << " Is Actuator = " << isActuator << endl;

        /* @brief initialize the timers with enumerated values  */
        /*@{*/
        startup             = new cMessage("DMAMAC_STARTUP");
        setSleep            = new cMessage("DMAMAC_SLEEP");
        waitData            = new cMessage("DMAMAC_WAIT_DATA");
        waitAck             = new cMessage("DMAMAC_WAIT_ACK");
        waitAlert           = new cMessage("DMAMAC_WAIT_ALERT");
        waitNotification    = new cMessage("DMAMAC_WAIT_NOTIFICATION");
        sendData            = new cMessage("DMAMAC_SEND_DATA");
        sendAck             = new cMessage("DMAMAC_SEND_ACK");
        scheduleAlert       = new cMessage("DMAMAC_SCHEDULE_ALERT");
        sendAlert           = new cMessage("DMAMAC_SEND_ALERT");
        sendNotification    = new cMessage("DMAMAC_SEND_NOTIFICATION");
        ackReceived         = new cMessage("DMAMAC_ACK_RECEIVED");
        ackTimeout          = new cMessage("DMAMAC_ACK_TIMEOUT");
        dataTimeout         = new cMessage("DMAMAC_DATA_TIMEOUT");
        alertTimeout        = new cMessage("DMAMAC_ALERT_TIMEOUT");

        /* @brief msgKind for identification in integer */
        startup->setKind(DMAMAC_STARTUP);
        setSleep->setKind(DMAMAC_SLEEP);
        waitData->setKind(DMAMAC_WAIT_DATA);
        waitAck->setKind(DMAMAC_WAIT_ACK);
        waitAlert->setKind(DMAMAC_WAIT_ALERT);
        waitNotification->setKind(DMAMAC_WAIT_NOTIFICATION);
        sendData->setKind(DMAMAC_SEND_DATA);
        sendAck->setKind(DMAMAC_SEND_ACK);
        scheduleAlert->setKind(DMAMAC_SCHEDULE_ALERT);
        sendAlert->setKind(DMAMAC_SEND_ALERT);
        sendNotification->setKind(DMAMAC_SEND_NOTIFICATION);
        ackReceived->setKind(DMAMAC_ACK_RECEIVED);
        ackTimeout->setKind(DMAMAC_ACK_TIMEOUT);
        dataTimeout->setKind(DMAMAC_DATA_TIMEOUT);
        alertTimeout->setKind(DMAMAC_ALERT_TIMEOUT);
        /*@}*/

        /* @brief My slot is same as myID assigned used for slot scheduling  */
        mySlot = myId;

        /* @brief Schedule a self-message to start superFrame  */
        scheduleAt(0.0, startup);


        sendUppperLayer = par("sendUppperLayer"); // if false the module deletes the packet, other case, it sends the packet to the upper layer.

        // initialize the random generator.
        if (par("initialSeed").longValue() != -1)
        {
            randomGenerator = new CRandomMother(par("initialSeed").longValue());
            setNextSequenceChannel();
        }
        else
        {
            setChannel(par("initialChannel"));
            randomGenerator = nullptr;
        }
    }
}


/* @brief Module destructor */

DMAMAC::~DMAMAC() {

    /* @brief Clearing Self Messages */
    /*@{*/
    cancelAndDelete(startup);
    cancelAndDelete(setSleep);
    cancelAndDelete(waitData);
    cancelAndDelete(waitAck);
    cancelAndDelete(waitAlert);
    cancelAndDelete(waitNotification);
    cancelAndDelete(sendData);
    cancelAndDelete(sendAck);
    cancelAndDelete(scheduleAlert);
    cancelAndDelete(sendAlert);
    cancelAndDelete(sendNotification);
    cancelAndDelete(ackReceived);
    cancelAndDelete(ackTimeout);
    cancelAndDelete(dataTimeout);
    cancelAndDelete(alertTimeout); 
    /*@}*/

    MacPktQueue::iterator it1;
        for(it1 = macPktQueue.begin(); it1 != macPktQueue.end(); ++it1) {
           delete (*it1);
        }
        macPktQueue.clear();
}

/* @brief Recording statistical data fro analysis */
void DMAMAC::finish() {

    /* @brief Record statistics for plotting before finish */
    if (stats)
    {
        recordScalar("nbTxData", nbTxData);
        recordScalar("nbTxActuatorData", nbTxActuatorData);
        recordScalar("nbTxDataFailures", nbTxDataFailures);
        recordScalar("nbTxAcks", nbTxAcks);
        recordScalar("nbTxAlert", nbTxAlert);
        recordScalar("nbTxSlots", nbTxSlots);
        recordScalar("nbTxNotifications", nbTxNotifications);
        recordScalar("nbRxData", nbRxData);
        recordScalar("nbRxActuatorData", nbRxActuatorData);
        recordScalar("nbRxAcks", nbRxAcks);
        recordScalar("nbRxAlert", nbRxAlert);
        recordScalar("nbRxNotifications", nbRxNotifications);
        //recordScalar("nbMissedAcks", nbMissedAcks);
        recordScalar("nbSleepSlots", nbSleepSlots);
        recordScalar("nbCollisions",nbCollisions);
        recordScalar("nbDroppedDataPackets", nbDroppedDataPackets);
        recordScalar("nbTransient", nbTransient);
        recordScalar("nbSteady", nbSteady);
        recordScalar("nbSteadyToTransient", nbSteadyToTransient);
        recordScalar("nbTransientToSteady", nbTransientToSteady);
        recordScalar("nbFailedSwitch", nbFailedSwitch);
        recordScalar("nbMidSwitch", nbMidSwitch);
        recordScalar("nbSkippedAlert", nbSkippedAlert);
        recordScalar("nbForwardedAlert", nbForwardedAlert);
        recordScalar("nbDiscardedAlerts", nbDiscardedAlerts);
        recordScalar("nbTimeouts", nbTimeouts); 
        recordScalar("nbAlertRxSlots", nbAlertRxSlots); 
    }
}

/* @brief
 * Handles packets from the upper layer and starts the process
 * to send them down.
 */
void DMAMAC::handleUpperPacket(cPacket* msg){

    EV_DEBUG << "Packet from Network layer" << endl;
    if (sendUppperLayer) {

        /* Creating Mac packets here itself to have periodic style in our method
         * Since application packet generated period had certain issues
         * Folliwing code segement can be used for application layer generated packets
         * @brief Casting upper layer message to mac packet format
         * */
        DMAMACPkt *mac = static_cast<DMAMACPkt *>(encapsMsg(static_cast<cPacket*>(msg)));
        destAddr = mac->getDestAddr();
        // @brief Sensor data goes to sink and is of type DMAMAC_DATA, setting it here 20 May
        if (mac->getDestAddr() != sinkAddress)
        {
            throw cRuntimeError("Destination address is not sink address Dest: %s, Sink Addr %s", mac->getDestAddr().str().c_str(), sinkAddress.str().c_str());
        }


        mac->setKind(DMAMAC_DATA);
        mac->setMySlot(mySlot);

        // @brief Check if packet queue is full s
        if (macPktQueue.size() < queueLength) {
            macPktQueue.push_back(mac);
            EV_DEBUG << " Data packet put in MAC queue with queueSize: "
                            << macPktQueue.size() << endl;

        } else {
            // @brief Queue is full, message has to be deleted DMA-MAC
            EV_DEBUG << "New packet arrived, but queue is FULL, so new packet is deleted\n";
            // @brief Network layer unable to handle MAC_ERROR for now so just deleting
            delete mac;
        }
    }
    else
        delete msg;
}

/*  @brief Handles the messages sent to self (mainly slotting messages)  */
void DMAMAC::handleSelfMessage(cMessage* msg)
{
    EV << "Self-Message Arrived with type : " << msg->getKind() << "Current mode of operation is :" << currentMacMode << endl;

    /* @brief To check if collision(alert packet dropped) has resulted in a switch failure */
    if(checkForSuperframeChange && !changeMacMode && currentSlot == 0)
    {
        EV << "Switch failed due to collision" << endl;
        nbFailedSwitch++;
        checkForSuperframeChange = false;
    }
    /* @brief To set checkForSuperframeChange as false when superframe actually changes */
    if(checkForSuperframeChange && changeMacMode && currentSlot == 0)
        checkForSuperframeChange = false;

    /* @brief To change superframe when in slot 0 or at multiples of numSlotsTransient 
	 * between transient parts, if required for emergency transient switch.
	 */
    if (currentSlot == 0 || (currentSlot % numSlotsTransient) == 0)
    {
        /* @brief If we need to change to new superframe operational mode */
        if (changeMacMode)
        {
           EV << " MAC to change operational mode : " << currentMacMode << " to : " << previousMacMode << endl;
           if (currentMacMode == TRANSIENT)
           {
               previousMacMode = TRANSIENT;
               currentMacMode = STEADY;
           }
           else
           {
              previousMacMode = STEADY;
              currentMacMode = TRANSIENT;
           }
           EV << "The currentSlot is on switch is " << currentSlot << endl;
           changeSuperFrame(currentMacMode);
        }
    }

	/* @brief Collecting statistcs and creating packets after every superframe */
    if (currentSlot == 0)
    {
       EV << "¤¤¤¤¤¤¤¤New Superframe is starting¤¤¤¤¤¤¤¤" << endl;
       setNextSequenceChannel();
       macPeriod = DATA;
       /* @Statistics */
       if (currentMacMode == TRANSIENT)
           nbTransient++;
       else
           nbSteady++;

       /* @brief Creating the data packet for this round */
       if(!isActuator && macPktQueue.empty())
       {
              DMAMACPkt* mac = new DMAMACPkt();
              destAddr = MACAddress(sinkAddress);
              mac->setDestAddr(destAddr);
              mac->setKind(DMAMAC_DATA);
              mac->setByteLength(44);        
              macPktQueue.push_back(mac);
              //mac->setBitLength(headerLength);
         }
    }

    /* @brief To mark Alert period for mark */
    if (currentSlot >= numSlotsTransient)
        macPeriod = ALERT;

    EV << "Current <MAC> period = " << macPeriod << endl;
    /* @brief Printing number of slots for check  */
    EV << "nbSlots = " << numSlots << ", currentSlot = " << currentSlot << ", mySlot = " << mySlot << endl;
    EV << "In this slot transmitting node is : " << transmitSlot[currentSlot] << endl;
    EV << "Current RadioState : " << radio->getRadioModeName(radio->getRadioMode())  << endl;

    bool recIsIdle = (radio->getReceptionState() == IRadio::RECEPTION_STATE_IDLE);

    switch (msg->getKind())
    {
        /* @brief SETUP phase enters to start the MAC protocol operation */
        case DMAMAC_STARTUP:

                currentMacState = STARTUP;
                currentSlot = 0;
                /* @brief Starting with notification (TRANSIENT state) */
                scheduleAt(simTime(), waitNotification);
                break;

        /* @brief Sleep state definition  */
        case DMAMAC_SLEEP:

                currentMacState = MAC_SLEEP;
                /* Finds the next slot after getting up */
                findDistantNextSlot();
                break;

        /* @brief Waiting for data packet state definition */
        case DMAMAC_WAIT_DATA:

                currentMacState = WAIT_DATA;
                EV << "My data receive slot " << endl;

				/* @brief Data tiemout is scheduled */
                EV << "Scheduling timeout with value : " << dataTimeoutValue << endl;   
                scheduleAt(simTime() + dataTimeoutValue, dataTimeout);                  

                /* @brief Checking if the radio is already in receive mode */
                if (radio->getRadioMode() != IRadio::RADIO_MODE_RECEIVER)
                {
                    radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
                    EV << "Switching Radio to RX" << endl;
                }
                else
                    EV << "Radio already in RX" << endl;

				/* @brief Find next slot and increment current slot counter after sending self message */
                findImmediateNextSlot(currentSlot, slotDuration);
                currentSlot++;
                currentSlot %= numSlots;
                break;

        /* @brief Waiting for ACK packet state definition */
        case DMAMAC_WAIT_ACK:

                currentMacState = WAIT_ACK;
                EV << "Switching radio to RX, waiting for ACK" << endl;
                radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);

                /* @brief Wait only until ACK timeout */
                scheduleAt(simTime() + ackTimeoutValue, ackTimeout);
                break;

        /* @brief Waiting for ALERT packet state definition */
        case DMAMAC_WAIT_ALERT:

                currentMacState = WAIT_ALERT;
                EV << "Switching radio to RX, waiting for Alert" << endl;

                /* @brief If no sensor children no wait alert.*/
                if(!hasSensorChild)
                {
                    EV << " No children are <Sensors> so no waiting" << endl;
                }
                else
                {
                    nbAlertRxSlots++;

					/* @brief The only place where DMAMAC types Hybrid and TDMA are different is in the wait alert and send alert */
                    if(macType == DMAMAC::TDMA) // 1 = TDMA, 0 = CSMA.
                    {
                        EV << "Scheduling alert timeout with value : " << alertTimeoutValue << endl; 
                        scheduleAt(simTime() + alertTimeoutValue, alertTimeout);
                    }

                    /* @brief Checking if the radio is already in receive mode, preventing RX->RX */
                    if (radio->getRadioMode() != IRadio::RADIO_MODE_RECEIVER)
                    {
                        radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
                        EV << "Switching Radio to RX mode" << endl;
                    }
                    else
                        EV << "Radio already in RX mode" << endl;
                }

                findImmediateNextSlot(currentSlot, slotDuration);
                currentSlot++;
                currentSlot %= numSlots;
                break;

        /* @brief Waiting for ACK packet state definition */
        case DMAMAC_WAIT_NOTIFICATION:

                currentMacState = WAIT_NOTIFICATION;
                EV << "Switching radio to RX, waiting for Notification packet from sink" << endl;

                /* @brief Checking if the radio is already in receive mode, preventing RX->to RX */
                if (radio->getRadioMode() != IRadio::RADIO_MODE_RECEIVER)
                {
                    radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
                    EV << "Switching Radio to RX mode" << endl;
                }
                else
                    EV << "Radio already in RX mode" << endl;


                findImmediateNextSlot(currentSlot, slotDuration);
                currentSlot++;
                currentSlot %= numSlots;
                break;

        /* @brief Handling receiving of ACK, more of a representative message */
        case DMAMAC_ACK_RECEIVED:

                EV << "ACK Received, setting radio to sleep" << endl;
                if (radio->getRadioMode() == IRadio::RADIO_MODE_RECEIVER)
                    radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
                break;

        /* @brief Handling ACK timeout possibility (ACK packet lost)  */
        case DMAMAC_ACK_TIMEOUT:

                /* @brief Calculating number of transmission failures  */
                nbTxDataFailures++;
                EV << "Data <failed>" << endl;

                if (radio->getRadioMode() == IRadio::RADIO_MODE_RECEIVER)
                    radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);

                /* @brief Checking if we have re-transmission slots left */
                if (transmitSlot[currentSlot + 1] == mySlot)
                    EV << "ACK timeout received re-sending DATA" << endl;
                else
                {
                    EV << "Maximum re-transmissions attempt done, DATA transmission <failed>. Deleting packet from que" << endl;
                    EV_DEBUG << " Deleting packet from DMAMAC queue";
                    delete macPktQueue.front();     // DATA Packet deleted in case re-transmissions are done
                    macPktQueue.pop_front();
                    EV << "My Packet queue size" << macPktQueue.size() << endl;
                }
                break;

        /* @brief Handling data timeout (required when in re-transmission slot nothing is sent) */
        case DMAMAC_DATA_TIMEOUT:

                EV << "No data transmission detected stopping RX" << endl;
                if (radio->getRadioMode() == IRadio::RADIO_MODE_RECEIVER)
                {
                    radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
                    EV << "Switching Radio to SLEEP" << endl;
                }
                nbTimeouts++;
                break;

        /* @brief Handling alert timeout (required when in alert slot nothing is being received) */
        case DMAMAC_ALERT_TIMEOUT:

               EV << "No alert transmission detected stopping alert RX, macType : " << macType << endl;
               if (radio->getRadioMode() == IRadio::RADIO_MODE_RECEIVER)
               {
                   radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
                  EV << "Switching Radio to SLEEP" << endl;
               }
               break;

        /* @brief Sending data definition */
        case DMAMAC_SEND_DATA:

                currentMacState = SEND_DATA;
                nbTxSlots++;

                /* @brief Checking if the current slot is also the node's transmit slot  */
                assert(mySlot == transmitSlot[currentSlot]);

                /* @brief Checking if packets are available to send in the mac queue  */
                if(macPktQueue.empty())
                    EV << "No Packet to Send exiting" << endl;
                else
                {
                    /* @brief Setting the radio state to transmit mode */
                    if(radio->getRadioMode() != IRadio::RADIO_MODE_TRANSMITTER)
                        radio->setRadioMode(IRadio::RADIO_MODE_TRANSMITTER);
                    EV_DEBUG << "Radio switch to TX command Sent.\n";
                }

                findImmediateNextSlot(currentSlot, slotDuration);
                currentSlot++;
                currentSlot %= numSlots;
                break;

        /* @brief Sending ACK packets when DATA is received */
        case DMAMAC_SEND_ACK:

                currentMacState = SEND_ACK;
                radio->setRadioMode(IRadio::RADIO_MODE_TRANSMITTER);
                break;

        /* @brief Scheduling the alert with a random Delay for Hybrid and without delay fro TDMA */
        case DMAMAC_SCHEDULE_ALERT:

                currentMacState = SCHEDULE_ALERT;
                /* @brief Radio set to sleep to prevent missing messages due to time required to switch 
				 */
                if(macType == DMAMAC::HYBRID || macType == DMAMAC::TDMA) // 0 = Hybrid
                {
                    if((radio->getRadioMode() == IRadio::RADIO_MODE_RECEIVER) || (radio->getRadioMode() == IRadio::RADIO_MODE_TRANSMITTER))
                        radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
                }

                randomNumber = uniform(0,1000,0);   
                EV << "Generated randomNumber is : " << randomNumber << endl;                

                /* @brief Checking if we need to send Alert message at all */
                /*@{*/
                if (alertMessageFromDown)
                {
                   EV << "Forwarding Alert packets from children " << endl;
                   sendAlertMessage = true;
                }
                /* @brief if we have alert message generated randomly */
                else if (randomNumber < alertProbability) 
                {
                   EV << "Threshold crossed need to send Alert message, number generated :" << randomNumber << endl;
                   sendAlertMessage = true;
                }
                /*@}*/

                /* @brief if we have alert message to be sent it is scheduled here with random Delay
 				 * random delay is only used for Hybrid version, for TDMA it it sent without delay
				 */
                if(macType == DMAMAC::HYBRID) // 0 = Hybrid
                {
                    if(sendAlertMessage)
                    {
                        /* @brief Scheduling the first Alert message */
                        EV << "Alert Message being scheduled !alert " << endl;
                        double alertDelay = ((double)(intrand(alertDelayMax)))/ 10000;
                        EV_DEBUG << "Delay generated !alert, alertDelay: " << alertDelay << endl;
                        
                        scheduleAt(simTime() + alertDelay, sendAlert);											
                        sendAlertMessage = false;
                    }
                    else
                    {
                        /* @brief If we have no alert to send check if radio is in RX, if yes set to SLEEP */
                        EV << "No alert to send sleeping <NoAlert>" << endl;
                        if(radio->getRadioMode() != IRadio::RADIO_MODE_SLEEP)
                            radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
                    }
                }
                else if (macType == DMAMAC::TDMA) // 1 = TDMA
                {
                    if(sendAlertMessage)
                    {
                        scheduleAt(simTime(), sendAlert);
                        sendAlertMessage = false;
                    }
                    else
                    {
                        EV << "No alert to send sleeping <NoAlert>" << endl;
                        if(radio->getRadioMode() != IRadio::RADIO_MODE_SLEEP)
                            radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
                    }
                }

                /* @brief Changed to in any case find next slot */
                findImmediateNextSlot(currentSlot, slotDuration);
                currentSlot++;
                currentSlot %= numSlots;
                break;

        /* @brief Sending alert packets after set delay */
        case DMAMAC_SEND_ALERT:
                currentMacState = SEND_ALERT;
                EV << "Sending Alert message " << endl;
                if (macType == DMAMAC::HYBRID) // 0 = Hybrid
                {
                    /*@{ Hybrid part*/
                    /* @brief Send only if channel is idle otherwise ignore since there is alert message being sent already */
                    if(recIsIdle)
                    {
                        radio->setRadioMode(IRadio::RADIO_MODE_TRANSMITTER);
                    }
                    else
                    {
                       EV << "Channel busy indicating alert message is being sent: thus we skip " << endl;
                       nbSkippedAlert++;
                    }
                    /*@}*/
                }
                else if (macType == DMAMAC::TDMA)  // 1 = TDMA
                {
                    /* Start sending alert */
                    if (radio->getRadioMode() != IRadio::RADIO_MODE_TRANSMITTER)
                        radio->setRadioMode(IRadio::RADIO_MODE_TRANSMITTER);
                }
                break;

        default:{
            EV << "WARNING: unknown timer callback at Self-Message" << msg->getKind() << endl;
        }
    }
}

/* @brief
 * Handles and forwards different control messages we get from the physical layer.
 * This comes from basic MAC tutorial for MiXiM
 */
void DMAMAC::receiveSignal(cComponent *source, simsignal_t signalID, long value, cObject *details)
{

    Enter_Method_Silent();
    if (signalID == IRadio::radioModeChangedSignal) {
            IRadio::RadioMode radioMode = (IRadio::RadioMode)value;
            if (radioMode == IRadio::RADIO_MODE_TRANSMITTER) {
                handleRadioSwitchedToTX();
            }
            else if (radioMode == IRadio::RADIO_MODE_RECEIVER) {
                handleRadioSwitchedToRX();
            }
    }
    else if (signalID == IRadio::transmissionStateChangedSignal) {
        IRadio::TransmissionState newRadioTransmissionState = (IRadio::TransmissionState)value;
        if (transmissionState == IRadio::TRANSMISSION_STATE_TRANSMITTING && newRadioTransmissionState == IRadio::TRANSMISSION_STATE_IDLE) {
            EV_DEBUG << "Packet transmission completed" << endl;
            /* @brief Should not wait for ACK for the control packet sent by node 0 */
            if(currentMacState == SEND_DATA)
            {
                EV_DEBUG << "Packet transmission completed awaiting ACK" << endl;
                scheduleAt(simTime(), waitAck);
            }
            /* @brief Setting radio to sleep to save energy after ACK is sent */
            if(currentMacState == SEND_ACK && radio->getRadioMode() == IRadio::RADIO_MODE_TRANSMITTER)
                radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);

            /* @brief Setting radio to sleep to save energy after Alert is sent */
            if(currentMacState == SEND_ALERT && radio->getRadioMode() == IRadio::RADIO_MODE_TRANSMITTER)
                radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
        }
        transmissionState = newRadioTransmissionState;
    }
    else if (signalID == IRadio::receptionStateChangedSignal) {
        IRadio::ReceptionState newRadioReceptionState = (IRadio::ReceptionState)value;
        if (newRadioReceptionState == IRadio::RECEPTION_STATE_RECEIVING) {
            EV_DEBUG << "Message being received" << endl;
            /* @brief Data received thus need to cancel data timeout event */
            if(dataTimeout->isScheduled() && currentMacState == WAIT_DATA)
            {
                cancelEvent(dataTimeout);
                EV << "Receiving Data, hence scheduled data timeout is cancelled" << endl;
            }
            /* @brief ACK received thus need to cancel ACK timeout event */
            else if(ackTimeout->isScheduled() && currentMacState == WAIT_ACK)
            {
                cancelEvent(ackTimeout);
                EV << "Receiving ACK, hence scheduled ACK timeout is cancelled" << endl;
            }
            /* @brief Alert received thus need to cancel Alert timeout event */
            else if(alertTimeout->isScheduled() && currentMacState == WAIT_ALERT)
            {
                cancelEvent(alertTimeout);
                EV << "Receiving alert, hence scheduled alert timeout is cancelled" << endl;
            }
        }
    }
}

/* @brief
 * Called after the radio is switched to TX mode. Encapsulates
 * and sends down the packet to the physical layer.
 * This comes from basic MAC tutorial for MiXiM
 */
void DMAMAC::handleRadioSwitchedToTX() {
    /* @brief Radio is set to Transmit state thus next job is to send the packet to physical layer  */
    EV << "¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤ TX handler module ¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤" << endl;

        if(currentMacState == SEND_DATA)
        {
            EV << "Sending Data packet down " << endl;
            DMAMACPkt* data = macPktQueue.front()->dup();

            /* @brief Other fields are set, setting slot field 
			 * Both for self data and forward data
			 */
            data->setMySlot(mySlot);
            attachSignal(data);
            EV_INFO << "Sending down data packet\n";
            sendDown(data);

            /* @statistics */
            nbTxData++;
        }
        else if (currentMacState == SEND_ACK)
        {
            EV_INFO << "Creating and sending ACK packet down " << endl;
            /* @brief ACK packet has to be created */
            DMAMACPkt* ack = new DMAMACPkt();
            ack->setDestAddr(lastDataPktSrcAddr);
            ack->setSrcAddr(myMacAddr);
            ack->setKind(DMAMAC_ACK);
            ack->setMySlot(mySlot);
            ack->setByteLength(11);
            attachSignal(ack);
            EV_INFO << "Sending ACK packet\n";
            sendDown(ack);

            /* @Statistics */
            nbTxAcks++;
            EV_INFO << "#TxAcks : " << nbTxAcks << endl;
        }
        else if (currentMacState == SEND_ALERT)
        {
		   /* @brief Forwarding alert packets received from children */
           if(alertMessageFromDown)
            {
               EV_INFO << "Forwarding Alert from Down " << endl;
                AlertPkt* alert = new AlertPkt();
                destAddr = MACAddress(parent);
                alert->setDestAddr(destAddr);
                alert->setSrcAddr(myMacAddr);
                alert->setKind(DMAMAC_ALERT);
                alert->setByteLength(11);
                attachSignal(alert);
                EV_INFO << "Forwarding Alert packet\n";
                sendDown(alert);

                /* @Statistics */
                nbForwardedAlert++;
            }
			/* @brief Sending its own alert packet */
            else
            {
                EV_INFO << "Creating and sending Alert packet down " << endl;
                /* @brief Alert packet has to be created */
                AlertPkt* alert = new AlertPkt();
                destAddr = MACAddress(parent);
                alert->setDestAddr(destAddr);
                alert->setSrcAddr(myMacAddr);
                alert->setKind(DMAMAC_ALERT);
                alert->setByteLength(11);
                attachSignal(alert);
                EV_INFO << "Sending #new Alert packet\n";
                sendDown(alert);

                /* @Statistics */
                nbTxAlert++;
            }
        }
}

/* @brief
 * Encapsulates the packet from the upper layer and
 * creates and attaches signal to it.
 */
MACFrameBase* DMAMAC::encapsMsg(cPacket* msg) {

    DMAMACPkt *pkt = new DMAMACPkt(msg->getName(), msg->getKind());
    pkt->setBitLength(headerLength);

    /* @brief Copy dest address from the Control Info (if attached to the network message by the network layer) */
    IMACProtocolControlInfo *const cInfo = check_and_cast<IMACProtocolControlInfo *>(msg->removeControlInfo());

    EV_DEBUG << "CInfo removed, mac addr=" << cInfo->getDestinationAddress() << endl;
    pkt->setDestAddr(cInfo->getDestinationAddress());

    /* @brief Delete the control info */
    delete cInfo;

    /* @brief Set the src address to own mac address (nic module getId()) */
    pkt->setSrcAddr(myMacAddr);

    /* @brief Encapsulate the MAC packet */
    pkt->encapsulate(check_and_cast<cPacket *>(msg));
    EV_DEBUG <<"pkt encapsulated\n";

    return pkt;
}

/* @brief
 * Called when the radio switched back to RX.
 * Sets the MAC state to RX
 * This comes from basic MAC tutorial for MiXiM
 */
void DMAMAC::handleRadioSwitchedToRX() {

    EV << "Radio switched to Receiving state" << endl;

    /* @brief
     * No operations defined currently MAC just waits for packet from
     * physical layer which will be handled by handleLowerMsg function
     */
}

/* @brief
 * Handles received Mac packets from Physical layer. Asserts if the packet
 * was received correct and checks if it was meant for us. 
 */
void DMAMAC::handleLowerPacket(cPacket* msg) {

    if (msg->hasBitError()) {
        EV << "Received " << msg << " contains bit errors or collision, dropping it\n";
        if (currentMacMode == STEADY)
            checkForSuperframeChange = true;
        delete msg;
        return;
    }

    if (dynamic_cast<DMAMACSyncPkt*>(msg))
    {
        // TODO: sync packet, syncronize channels and time slots
        DMAMACSyncPkt * syncPk = dynamic_cast<DMAMACSyncPkt*>(msg);
        currentSlot = syncPk->getTimeSlot();
        numSlots = syncPk->getTdmaNumSlots();
        return;
    }

    if(currentMacState == WAIT_DATA)
    {
        DMAMACPkt *mac  = dynamic_cast<DMAMACPkt *>(msg);
        if (mac == nullptr)
        {
            delete msg;
            return;
        }
        const MACAddress& dest = mac->getDestAddr();

        EV << " DATA Packet received with length :" << mac->getByteLength() << " destined for: " << mac->getDestAddr() << endl;

        /* @brief Checking done for actuator data packets if it is for one of the children */
        if( mac->getKind() == DMAMAC_ACTUATOR_DATA)
        {
            for(int i=0;i<4;i++)
            {
                for(int j=0;j<10;j++)
                {
                    MACAddress childNode =  MACAddress(downStream[i].reachableAddress[j]);
                    if (dest == childNode)
                       forChildNode = true;
                }
            }
            /* @brief Error prevention clause to make space for actuator packets even if there is no space */
            if(macPktQueue.size() == queueLength)
            {
                EV_DEBUG << " Deleting packet from DMAMAC queue";
                delete macPktQueue.front();     // DATA Packet deleted
                macPktQueue.pop_front();
            }
        }

        /* @brief Check if the packet is for me or sink (TDMA So it has to be me in general so just for testing) */
        if(dest == myMacAddr || dest == sinkAddress || forChildNode)
        {
            lastDataPktSrcAddr = mac->getSrcAddr();
            /* @brief not sending forwarding packets up to application layer */
            if (dest == myMacAddr && sendUppperLayer)
                sendUp(decapsMsg(mac));
            else {

                if (macPktQueue.size() < queueLength) {
                    macPktQueue.push_back(mac);
                    EV << " DATA packet from child node put in queue : "
                              << macPktQueue.size() << endl;
                }
                else
                {
                    EV << " No space for forwarding packets queue size :"
                              << macPktQueue.size() << endl;
                    delete mac;
                }
            }


            /* @brief Packet received for myself thus sending an ACK */
            scheduleAt(simTime(), sendAck);

            /* @brief Resetting forChildNode value to false */
            if (forChildNode)
            {
                forChildNode = false;
                /* @statistics */
                nbRxActuatorData++;
            }
            //if(dest == myMacAddr)
                nbRxData++;
        }
        else
        {
            EV << "DATA Packet not for me, deleting";
            delete mac;
        }
    }
    else if (currentMacState == WAIT_ACK)
    {
        DMAMACPkt *mac  = dynamic_cast<DMAMACPkt *>(msg);
        if (mac == nullptr)
        {
            delete msg;
            return;
        }

        EV << "ACK Packet received with length : " << mac->getByteLength() << ", DATA Transmission successful"  << endl;

        EV_DEBUG << " Deleting packet from DMAMAC queue";
        delete macPktQueue.front();     // DATA Packet deleted
        macPktQueue.pop_front();
        delete mac;                     // ACK Packet deleted

        /* @brief Returns self message that just prints an extra ACK received message preserved for future use */
        scheduleAt(simTime(), ackReceived);

        /* @Statistics */
        nbRxAcks++;
    }
    else if (currentMacState == WAIT_NOTIFICATION)
    {
        DMAMACSinkPkt *notification  = dynamic_cast<DMAMACSinkPkt *>(msg);
        if (notification == nullptr)
        {
            delete msg;
            return;
        }

        changeMacMode = notification->getChangeMacMode();
        EV << " NOTIFICATION packet received with length : " << notification->getByteLength() << " and changeMacMode = " << changeMacMode << endl;

        if (changeMacMode == true)
        {
            EV << " Sink has asked to switch states " << endl;
            if(currentMacMode == STEADY)
            {
                EV << "Initiating switch procedure (STEADY to TRANSIENT) immediately " << endl;
                /* @brief if alert is successful delete saved alert */
                if(alertMessageFromDown && currentMacMode == STEADY)
                    alertMessageFromDown = false;

                /* @brief After last notification of the steady superframe, new Superframe again has notification.
                 * Otherwise in between there is always sleep after notification 
                 * Because before we come here next slot is already found and scheduled
				 * Thus we need to cancel the next event based on the result here.
                 */
                EV << "Current slot value at this time :" << currentSlot << endl;
                if(currentSlot == 0)
                {
					/* @brief This part only re-instates wait notification (Have not tried with ommitting this one) Might not be required */
                    EV << " Notification message arrival time " << waitNotification->getArrivalTime() << endl;
                    const simtime_t nextDiscreteEvent = waitNotification->getArrivalTime();
                    cancelEvent(waitNotification);
                    scheduleAt(nextDiscreteEvent,waitNotification);
                }
                else
                {
					/* @brief Schedules sleep instead of waitNotification */
                    EV << " Sleep message arrival time " << setSleep->getArrivalTime() << endl;
                    scheduleAt(setSleep->getArrivalTime(),waitNotification);
                    cancelEvent(setSleep);
                }

                /* €brief For the case of emergency switch resetting currentSlot counter to 0 */
                if (currentSlot != 0)
                    EV << " Switch asked by !Sink based on Alert" << endl;
            }
            else
                EV << "Initiating switch : TRANSIENT to STEADY in the next Superframe " << endl;
        }

        /* @Statistics */
        nbRxNotifications++;
        delete notification;

        /* @brief Notification received hence sleeping briefly until next work */
        if (radio->getRadioMode() == IRadio::RADIO_MODE_RECEIVER)
            radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
    }
    else if (currentMacState == WAIT_ALERT)
    {
        AlertPkt *alert  = dynamic_cast<AlertPkt *>(msg);
        if (alert == nullptr)
        {
            delete msg;
            return;
        }


        EV << "I have received an Alert packet from fellow sensor " << endl;
        EV << "Alert Packet length is : " << alert->getByteLength() << " detailed info " << alert->detailedInfo() << endl;

        EV << " Alert message sent to : " << alert->getDestAddr() << " my address is " << myMacAddr << endl;

        const MACAddress& dest = alert->getDestAddr();

        if(!alertMessageFromDown && dest == myMacAddr)
        {
            EV << "Alert message received from down will be forwarded" << endl;
            alertMessageFromDown = true;
            /* @Statistics */
            nbRxAlert++;
        }
        else
        {
            EV << "Alert for different node" << endl;
            nbDiscardedAlerts++;
        }

        delete alert;

        /* If in TDMA or Hybrid mode then stop RX process once alert is received. */
        if (radio->getRadioMode() == IRadio::RADIO_MODE_RECEIVER)
            radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
    }
}

/* @brief
 * Finding the distant slot generally after sleep for some slots.
 */
void DMAMAC::findDistantNextSlot()
{
    EV << "Current slot before calculating next wakeup :" << currentSlot << endl;
    int i,x; i = 1;
    x = (currentSlot + i) % numSlots;
    /* @brief if Actuator, only check if node has to receive otherwise check about transmit as well. */
    if(isActuator)
    {
        while(transmitSlot[x] != mySlot && receiveSlot[x] != mySlot && receiveSlot[x] != BROADCAST_RECEIVE && receiveSlot[x] != alertLevel)
        {
          i++;
          x = (currentSlot + i) % numSlots;
        }
    }
    else
    {
        while(transmitSlot[x] != mySlot && receiveSlot[x] != mySlot && receiveSlot[x] != BROADCAST_RECEIVE && transmitSlot[x] != alertLevel && receiveSlot[x] != alertLevel)
        {
          i++;
          x = (currentSlot + i) % numSlots;
        }
    }

    /* @brief
     * Here you could decide if you want to let the node sleep,
     * if the next transmit or receive slot is to appear soon
     * We set it to 1 for the time being.
	 * This means atleast one sleep slot
     *    if ( i > 1)
      {
     */
    /* @brief if not sleeping already then set sleep */
    if (radio->getRadioMode() != IRadio::RADIO_MODE_SLEEP)
        radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
    else
        EV << "Radio already in sleep just making wakeup calculations " << endl;

	/* @brief Time to next event */
    nextEvent = slotDuration*(i);
    currentSlot +=i;

    /* @Statistics */
    nbSleepSlots +=i;

    currentSlot %= numSlots;

    EV << "Time for RADIO Sleep for " << nextEvent << " Seconds" << endl;
    EV << "Waking up in slot :" << currentSlot << endl;

    /* @brief
    * Checking if next active slot found is a transmit slot or receive slot. Then scheduling
    * the next event for the time when the node will enter into transmit or receive.
    */
    if(transmitSlot[(currentSlot) % numSlots] == mySlot)
    {
        if(currentSlot < numSlotsTransient)
        {
            EV << "My next slot after sleep is transmit slot" << endl;
            scheduleAt(simTime() + nextEvent, sendData);
        }
        else if(currentSlot >= numSlotsTransient)
        {
            EV << "My next slot after sleep is alert Transmit Slot" << endl;
            scheduleAt(simTime() + nextEvent, scheduleAlert);
        }
    }
    else if (transmitSlot[(currentSlot) % numSlots] == alertLevel && !isActuator)
    {
        EV << "My next slot after sleep is alert Transmit Slot" << endl;
        scheduleAt(simTime() + nextEvent, scheduleAlert);
    }
    else if (receiveSlot[(currentSlot) % numSlots] == alertLevel)
    {
        EV << "My next slot after sleep is alert receive Slot" << endl;
        scheduleAt(simTime() + nextEvent, waitAlert);
    }
    else if (receiveSlot[(currentSlot) % numSlots] == BROADCAST_RECEIVE)
    {
        EV << "My next slot after sleep is notification slot" << endl;
        scheduleAt(simTime() + nextEvent, waitNotification);
    }
    else if (receiveSlot[(currentSlot) % numSlots] == mySlot)
    {
        if(currentSlot < numSlotsTransient)
        {
            EV << "My next slot after sleep is receive slot" << endl;
            scheduleAt(simTime() + nextEvent, waitData);
        }
        else if(currentSlot >= numSlotsTransient)
        {
            EV << "My next slot after sleep is alert receive Slot" << endl;
            scheduleAt(simTime() + nextEvent, waitAlert);
        }
    }
    else
            EV << " Undefined MAC state <ERROR>" << endl;

}

/* @brief Finding immediate next Slot */
void DMAMAC::findImmediateNextSlot(int currentSlotLocal,double nextSlot)
{
    EV << "Finding immediate next slot" << endl;
    if (transmitSlot[(currentSlotLocal + 1) % numSlots] == mySlot)
    {
        if(currentSlotLocal < numSlotsTransient)
        {
            EV << "Immediate next Slot is my Send Slot, getting ready to transmit" << endl;
            scheduleAt(simTime() + nextSlot, sendData);
        }
        else if(currentSlotLocal >= numSlotsTransient)
        {
            EV << "Immediate next Slot is my alert Transmit Slot, getting ready to send" << endl;
            scheduleAt(simTime() + nextSlot, scheduleAlert);
        }
    }
    else if (transmitSlot[(currentSlotLocal + 1) % numSlots] == alertLevel && !isActuator)
    {
        EV << "Immediate next Slot is my alert Transmit Slot, getting ready to send" << endl;
        scheduleAt(simTime() + nextSlot, scheduleAlert);
    }
    else if (receiveSlot[(currentSlotLocal + 1) % numSlots] == mySlot)
    {
        if(currentSlotLocal < numSlotsTransient)
        {
            EV << "Immediate next Slot is my Receive Slot, getting ready to receive" << endl;
            scheduleAt(simTime() + nextSlot, waitData);
        }
        else if(currentSlotLocal >= numSlotsTransient)
        {
            EV << "Immediate next Slot is my alert Receive Slot, getting ready to receive" << endl;
            scheduleAt(simTime() + nextSlot, waitAlert);
        }
    }
    else if (receiveSlot[(currentSlotLocal + 1) % numSlots] == alertLevel)
    {
        EV << "Immediate next Slot is my alert Receive Slot, getting ready to receive" << endl;
        scheduleAt(simTime() + nextSlot, waitAlert);
    }
    else if (receiveSlot[(currentSlotLocal + 1) % numSlots] == BROADCAST_RECEIVE)
    {
        EV << "Immediate next Slot is Notification Slot, getting ready to receive" << endl;
        scheduleAt(simTime() + nextSlot, waitNotification);
    }
    else
    {
        EV << "Immediate next Slot is Sleep slot.\n";
        scheduleAt(simTime() + nextSlot, setSleep);
    }
}

/* @brief Used by encapsulation function */
void DMAMAC::attachSignal(MACFrameBase* macPkt)
{
    simtime_t duration = macPkt->getBitLength() / bitrate;
    macPkt->setDuration(duration);
}

/* @brief To change main superframe to the new mac mode superframe */
void DMAMAC::changeSuperFrame(macMode mode)
{
    /* @brief Switching superframe slots to new mode */

    EV << "Switching superframe slots to new mode " << endl;

    if (mode == TRANSIENT)
    {
        for(int i=0; i < numSlotsTransient; i ++)
        {
            transmitSlot[i] = transmitSlotTransient[i];
            receiveSlot[i] = receiveSlotTransient[i];
        }
        numSlots = numSlotsTransient;
        EV << "In mode : " << mode << " , and number of slots : " << numSlots << endl;
    }
    else if (mode == STEADY)
    {
        for(int i=0; i < numSlotsSteady; i ++)
        {
            transmitSlot[i] = transmitSlotSteady[i];
            receiveSlot[i] = receiveSlotSteady[i];
        }
        numSlots = numSlotsSteady;
        EV << "In mode : " << mode << " , and number of slots : " << numSlots << endl;
    }

    /* @brief Set change mac mode back to false */
    changeMacMode = false;

    EV << "Transmit Slots and Receive Slots" << endl;
    for(int i=0; i < numSlots; i ++)
    {
        EV_DEBUG << " Send Slot "<< i << " = " << transmitSlot[i] << endl;
    }
    for(int i=0; i < numSlots; i ++)
    {
        EV_DEBUG << " Receive Slot "<< i << " = " << receiveSlot[i] << endl;
    }
    /* @brief Current Slot is set to Zero with start of new superframe */
    currentSlot=0;

    /* @brief period definition */
    macPeriod = DATA;
}

/* @brief Extracting the static slot schedule here from the xml files */
void DMAMAC::slotInitialize()
{
    EV << " Starting in transient state operation (just to be safe) " << endl;

    currentMacMode = TRANSIENT;

    /* @brief Intializing slotting arrays */

    /* @brief Steady superframe copied */
    xmlBuffer = xmlFileSteady->getFirstChildWithTag("transmitSlots");
    nListBuffer = xmlBuffer->getChildren(); //Gets all children (slot values)

    /* @brief Steady superframe listed continuously as values listing the slot indicator */
    xmlListIterator = nListBuffer.begin();
    for(int i=0;xmlListIterator!=nListBuffer.end();xmlListIterator++,i++)
    {
       xmlBuffer = (*xmlListIterator);
       EV_DEBUG << " XML stuff " << xmlBuffer->getNodeValue() << endl;
       transmitSlotSteady[i] = atoi(xmlBuffer->getNodeValue());
    }

    xmlBuffer = xmlFileSteady->getFirstChildWithTag("receiveSlots");
    nListBuffer = xmlBuffer->getChildren();
    xmlListIterator = nListBuffer.begin();

    for(int i=0;xmlListIterator!=nListBuffer.end();xmlListIterator++,i++)
    {
       xmlBuffer = (*xmlListIterator);
       EV_DEBUG << " XML stuff " << xmlBuffer->getNodeValue() << endl;
       receiveSlotSteady[i] = atoi(xmlBuffer->getNodeValue());
    }

    /* @brief Transient superframe copied */
    xmlBuffer = xmlFileTransient->getFirstChildWithTag("transmitSlots");
    nListBuffer = xmlBuffer->getChildren();
    xmlListIterator = nListBuffer.begin();

    for(int i=0;xmlListIterator!=nListBuffer.end();xmlListIterator++,i++)
    {
       xmlBuffer = (*xmlListIterator);
       EV_DEBUG << " XML stuff " << xmlBuffer->getNodeValue() << endl;
       transmitSlotTransient[i] = atoi(xmlBuffer->getNodeValue());
    }

    xmlBuffer = xmlFileTransient->getFirstChildWithTag("receiveSlots");
    nListBuffer = xmlBuffer->getChildren();
    xmlListIterator = nListBuffer.begin();

    for(int i=0;xmlListIterator!=nListBuffer.end();xmlListIterator++,i++)
    {
       xmlBuffer = (*xmlListIterator);
       EV_DEBUG << " XML stuff " << xmlBuffer->getNodeValue() << endl;
       receiveSlotTransient[i] = atoi(xmlBuffer->getNodeValue());
    }

    /* @brief Initializing to prevent random values */
    for(int i=0; i < maxNumSlots; i ++)
    {
        transmitSlot[i] = -1;
        receiveSlot[i] = -1;
    }

    /* @brief Starting with transient superframe */
    for(int i=0; i < maxNumSlots; i ++)
    {
       transmitSlot[i] = transmitSlotTransient[i];
       receiveSlot[i] = receiveSlotTransient[i];
    }

    numSlots = numSlotsTransient;

    EV_DEBUG << "Transmit Slots and Receive Slots" << endl;
    for(int i=0; i < maxNumSlots; i ++)
    {
        EV_DEBUG << " Send Slot "<< i << " = " << transmitSlot[i] << endl;
    }
    for(int i=0; i < maxNumSlots; i ++)
    {
        EV_DEBUG << " Receive Slot "<< i << " = " << receiveSlot[i] << endl;
    }

    /* @brief Extracting neighbor data from the input xml file */
    //if (myId != sinkAddress)

    cXMLElement *xmlBuffer1,*xmlBuffer2;

    cXMLElement* rootElement = par("neighborData").xmlValue();

    char id[maxNodes];
    sprintf(id, "%d", myId);
    EV << " My ID is : " << myId << endl;


    xmlBuffer = rootElement->getElementById(id);

    alertLevel = atoi(xmlBuffer->getFirstChildWithTag("level")->getNodeValue());
    EV << " Node is at alertLevel " << alertLevel << endl;

    parent = long(atoi(xmlBuffer->getFirstChildWithTag("parent")->getNodeValue()));
    EV << "My Parent is " << parent << endl;
    int reachableNodes[maxNodes],nextHopEntry;
    int i=0,j=0;

    for(j=0;j<maxNodes;j++)
       reachableNodes[j]=-1;

    /* @brief Initialization */
    for(int i=0;i<maxChildren;i++)
    {
        for(int j=0;j<maxNodes;j++)
        {
            downStream[i].nextHop = -1;
            downStream[i].reachableAddress[j] = -1;
        }
    }

    cXMLElementList::iterator xmlListIterator1;
    cXMLElementList::iterator xmlListIterator2;
    cXMLElementList nListBuffer1,nListBuffer2;

    nListBuffer1 = xmlBuffer->getElementsByTagName("nextHop");

    j=0;
    for(xmlListIterator1 = nListBuffer1.begin();xmlListIterator1!=nListBuffer1.end();xmlListIterator1++)
    {
      xmlBuffer1 = (*xmlListIterator1);
      EV<< " nList buffer " << xmlBuffer1->getAttribute("address") << endl;
      nextHopEntry = atoi(xmlBuffer1->getAttribute("address"));
      EV << "next hop entry is " << nextHopEntry << endl;
      if(xmlBuffer1->hasChildren())
      {
          nListBuffer2 = xmlBuffer1->getChildren();
          for(xmlListIterator2 = nListBuffer2.begin();xmlListIterator2!=nListBuffer2.end();xmlListIterator2++)
          {
              xmlBuffer2 = (*xmlListIterator2);
              EV << " XML stuff " << xmlBuffer2->getNodeValue() << endl;
              reachableNodes[i] = atoi(xmlBuffer2->getNodeValue());
              i++;
          }
          downStream[j].nextHop = nextHopEntry;
          for(int x=0;x<i;x++)
              downStream[j].reachableAddress[x] = reachableNodes[x];
          /* @brief Resetting reachableNodes to input next values */
          for(int x=0;x<maxNodes;x++)
              reachableNodes[x] = -1;
          i=0;
          j++;
      }
      else
      {
          EV << "Children are leaf nodes" << endl;
          reachableNodes[0] = nextHopEntry;
          downStream[j].nextHop = nextHopEntry;
          for(int x=0;x<maxNodes;x++)
              downStream[j].reachableAddress[x] = reachableNodes[x];
          j++;
      }
    }
    /* @brief All nodes can have max 3 children so we list only 3 for their next hops. */
    for(int j=0;j<maxChildren;j++)
    {
        EV << " The downstream possibilities at :" << downStream[j].nextHop << " are :";
        for(int x=0;x<maxNodes;x++)
        {
          if(downStream[j].reachableAddress[x] != -1)
              EV << "Node: " << downStream[j].reachableAddress[x];
        }
        EV <<  endl;
    }
}


cPacket *DMAMAC::decapsMsg(MACFrameBase *macPkt)
{
    cPacket *msg = macPkt->decapsulate();
    setUpControlInfo(msg, macPkt->getSrcAddr());
    return msg;
}

/**
 * Attaches a "control info" (MacToNetw) structure (object) to the message pMsg.
 */
cObject *DMAMAC::setUpControlInfo(cMessage *const pMsg, const MACAddress& pSrcAddr)
{
    SimpleLinkLayerControlInfo *const cCtrlInfo = new SimpleLinkLayerControlInfo();
    cCtrlInfo->setSrc(pSrcAddr);
    pMsg->setControlInfo(cCtrlInfo);
    return (cCtrlInfo);
}

void DMAMAC::setChannel(const int &channel) {
    if (channel < 11 || channel > 26)
        return;
    if (actualChannel == channel)
        return;

    EV << "Hop to channel :" << channel << endl;

    actualChannel = channel;
    ConfigureRadioCommand *configureCommand = new ConfigureRadioCommand();
    configureCommand->setBandwidth(Hz(channels[channel-11].bandwith));
    configureCommand->setCarrierFrequency(Hz(channels[channel-11].mean));

    cMessage *message = new cMessage("configureRadioMode", RADIO_C_CONFIGURE);
    message->setControlInfo(configureCommand);
    sendDown(message);
}

double DMAMAC::getCarrierChannel(const int &channel) {
    if (channel < 11 || channel > 26)
        return (-1);
    return (channels[channel-11].mean);
}

double DMAMAC::getBandwithChannel(const int &channel) {
    if (channel < 11 || channel > 26)
        return (-1);
    return (channels[channel-11].bandwith);
}

void DMAMAC::setNextSequenceChannel()
{
    if (randomGenerator == nullptr)
        return;
    int c = randomGenerator->iRandom(11,16);
    setChannel(c);
}

void DMAMAC::setChannelWithSeq(const uint32_t *v)
{
    if (randomGenerator == nullptr)
        return;
    int c = randomGenerator->iRandom(11,16,v);
    setChannel(c);
}

// Output random bits
uint32_t DMAMAC::CRandomMother::bRandom() {
  uint64_t sum;
  for (int i = 0; i < 5 ; i++)
      Prevx[i] = x[i];
  sum = (uint64_t)2111111111UL * (uint64_t)x[3] +
     (uint64_t)1492 * (uint64_t)(x[2]) +
     (uint64_t)1776 * (uint64_t)(x[1]) +
     (uint64_t)5115 * (uint64_t)(x[0]) +
     (uint64_t)x[4];
  x[3] = x[2];  x[2] = x[1];  x[1] = x[0];
  x[4] = (uint32_t)(sum >> 32);                  // Carry
  x[0] = (uint32_t)sum;                          // Low 32 bits of sum
  return (x[0]);
}

uint32_t DMAMAC::CRandomMother::bRandom(const uint32_t *v) {
  uint64_t sum;

  sum = (uint64_t)2111111111UL * (uint64_t)v[3] +
     (uint64_t)1492 * (uint64_t)(v[2]) +
     (uint64_t)1776 * (uint64_t)(v[1]) +
     (uint64_t)5115 * (uint64_t)(v[0]) +
     (uint64_t)v[4];                         // Low 32 bits of sum
  return ((uint32_t)sum);
}

// returns a random number between 0 and 1:
double DMAMAC::CRandomMother::random() {
   return ((double)bRandom() * (1./(65536.*65536.)));
}

// returns integer random number in desired interval:
int DMAMAC::CRandomMother::iRandom(int min, int max) {
   // Output random integer in the interval min <= x <= max
   // Relative error on frequencies < 2^-32
   if (max <= min) {
      if (max == min) return (min); else return (0x80000000);
   }
   // Assume 64 bit integers supported. Use multiply and shift method
   uint32_t interval;                  // Length of interval
   uint64_t longran;                   // Random bits * interval
   uint32_t iran;                      // Longran / 2^32

   interval = (uint32_t)(max - min + 1);
   longran  = (uint64_t)bRandom() * interval;
   iran = (uint32_t)(longran >> 32);
   // Convert back to signed and return result
   return ((int32_t)iran + min);
}

// returns integer random number in desired interval:
int DMAMAC::CRandomMother::iRandom(int min, int max, const uint32_t *v) {
    // Output random integer in the interval min <= x <= max
    // Relative error on frequencies < 2^-32
    if (max <= min) {
        if (max == min) return (min); else return (0x80000000);
    }
    // Assume 64 bit integers supported. Use multiply and shift method
    uint32_t interval;                  // Length of interval
    uint64_t longran;                   // Random bits * interval
    uint32_t iran;                      // Longran / 2^32
    interval = (uint32_t)(max - min + 1);
    longran  = (uint64_t)bRandom(v) * interval;
    iran = (uint32_t)(longran >> 32);
    // Convert back to signed and return result
    return ((int32_t)iran + min);
}

// this function initializes the random number generator:
void DMAMAC::CRandomMother::randomInit (int seed) {
  int i;
  uint32_t s = seed;
  // make random numbers and put them into the buffer
  for (i = 0; i < 5; i++) {
    s = s * 29943829 - 1;
    x[i] = s;
  }
  // randomize some more
  for (i=0; i<19; i++) bRandom();
}

void DMAMAC::CRandomMother::getRandSeq(uint32_t *v) {
    for (int i = 0 ; i < 5; i++)
        v[i] = x[i];
}

void DMAMAC::CRandomMother::getPrevSeq(uint32_t *v) {
    for (int i = 0 ; i < 5; i++)
        v[i] = Prevx[i];
}

void DMAMAC::CRandomMother::setRandSeq(const uint32_t *v) {
    for (int i = 0 ; i < 5; i++)
        x[i] = v[i];
}

}