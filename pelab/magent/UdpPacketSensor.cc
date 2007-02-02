#include "UdpPacketSensor.h"

using namespace std;

UdpPacketSensor::UdpPacketSensor()
  :lastSeenSeqNum(-1),
  packetLoss(0),
  totalPacketLoss(0),
  libpcapSendLoss(0),
  statReorderedPackets(0),
  ackFake(false)
{

}

UdpPacketSensor::~UdpPacketSensor()
{
  // Empty the list used to store the packets.
  sentPacketList.clear();

}

int UdpPacketSensor::getPacketLoss() const
{
	return packetLoss;

}

std::vector<UdpPacketInfo> UdpPacketSensor::getAckedPackets() const
{
	return ackedPackets;
}

bool UdpPacketSensor::isAckFake() const
{
	return ackFake;
}


void UdpPacketSensor::localSend(PacketInfo *packet)
{
	int overheadLen = 14 + 4 + 8 + packet->ip->ip_hl*4;

	if( ( ntohs(packet->udp->len ) - 8) < global::udpMinSendPacketSize )
	{
		logWrite(ERROR, "UDP packet data sent to UdpPacketSensor::localSend"
			       " was less than the "
			" required minimum %d bytes", global::udpMinSendPacketSize);
		sendValid = false;
		ackValid = false;
		return;
	}

	sendValid = true;
	ackValid = false;

	// CHANGE:
	unsigned short int seqNum = ntohs(*(unsigned short int *)(packet->payload + 1));
	unsigned short int packetSize = (*(unsigned short int *)(packet->payload + 1 + global::USHORT_INT_SIZE)) + overheadLen;
	UdpPacketInfo tmpPacketInfo;

	if(lastSeenSeqNum != -1)
	{
		// We missed some packets because of loss in libpcap buffer.
		// Add fake packets to the sent list, their sizes and time stamps
		// are unknown - but they can be gathered from the ACK packets.
		if(seqNum > (lastSeenSeqNum + 1))
		{
			for(int i = 1;i < seqNum - lastSeenSeqNum ; i++)
			{
				tmpPacketInfo.seqNum = lastSeenSeqNum + i;
				tmpPacketInfo.isFake = true;

				sentPacketList.push_back(tmpPacketInfo);
			}
			libpcapSendLoss += (seqNum - lastSeenSeqNum - 1);
		}
	}

	lastSeenSeqNum = seqNum;

	tmpPacketInfo.seqNum = seqNum;
	tmpPacketInfo.packetSize = packetSize;
	tmpPacketInfo.timeStamp = packet->packetTime.toMicroseconds();
	tmpPacketInfo.isFake = false;

	sentPacketList.push_back(tmpPacketInfo);
}

void UdpPacketSensor::localAck(PacketInfo *packet)
{
	if( ( ntohs(packet->udp->len ) - 8) < global::udpMinAckPacketSize )
	{
		logWrite(ERROR, "UDP packet data sent to UdpPacketSensor::localAck"
			       " was less than the "
			" minimum %d bytes ",global::udpMinAckPacketSize);
		ackValid = false;
		return;
	}

	unsigned short int seqNum = *(unsigned short int *)(packet->payload + 1);

	// Find the entry for the packet this ACK is acknowledging, and
	// remove it from the sent(&unacked) packet list.
	list<UdpPacketInfo >::iterator listIterator;

	listIterator = find_if(sentPacketList.begin(), sentPacketList.end(), bind2nd(equalSeqNum(), seqNum)); 

	if(listIterator == sentPacketList.end())
	{
		bool isReordered = handleReorderedAck(packet);

		if(isReordered == false)
		{
			logWrite(EXCEPTION, "Unknown UDP seq number %d  is being ACKed. "
				"We might have received "
				" a reordered ACK, which has already been ACKed using redundant ACKs .\n", seqNum);
			ackValid = false;
			return;
		}
		else
			ackValid = true;
	}
	else
		ackValid = true;

	// We received an ACK correctly(without reordering), but we dont have any record of ever
	// sending the original packet(Actually, we have a fake packet inserted into our send list)
       //	-- this indicates libpcap loss.
	if( (*listIterator).isFake == true)
		ackFake = true;
	else
		ackFake = false;


	// Remove the old state information.
	ackedPackets.clear();
	packetLoss = 0;

	int i;
	unsigned char numRedunAcksChar = 0; 
	int numRedunAcks = 0; 

	// Read how many redundant ACKs are being sent in this packet.
	memcpy(&numRedunAcksChar, &packet->payload[0], global::UCHAR_SIZE);

	numRedunAcks = static_cast<int>(numRedunAcksChar);

	// Store an iterator to the current seqNum being acknowledge, and delete it at the end.
	list<UdpPacketInfo >::iterator curPacketIterator = listIterator;

	// Look at the redundant ACKs first.
	UdpPacketInfo tmpPacketInfo;

	if(numRedunAcks > 0)
	{
		unsigned short int redunSeqNum;

		for(i = 0; i < numRedunAcks; i++)
		{
			redunSeqNum = *(unsigned short int *)(packet->payload + global::udpMinAckPacketSize + i*global::udpRedunAckSize);
			listIterator = sentPacketList.end();

			// Check whether the packet that this redundant ACK refers to exists
			// in our list of sent ( but unacked ) packets. It might not exist
			// if its ACK was not lost earlier.
			listIterator = find_if(sentPacketList.begin(), curPacketIterator, bind2nd(equalSeqNum(), redunSeqNum)); 

			// An unacked packet exists with this sequence number, delete it 
			// from the list and consider it acked.
			if(listIterator != curPacketIterator && listIterator != sentPacketList.end())
			{
				tmpPacketInfo.seqNum = (*listIterator).seqNum;
				tmpPacketInfo.packetSize = (*listIterator).packetSize;
				tmpPacketInfo.timeStamp = (*listIterator).timeStamp;
				tmpPacketInfo.isFake = (*listIterator).isFake;

				ackedPackets.push_back(tmpPacketInfo);

				sentPacketList.erase(listIterator);
			}
			else
			{
                               // Check whether this ACK corresponds to a packet reordered
                                // on the forward path.
                                list<UdpPacketInfo >::iterator reOrderedPacketIterator ;
                                reOrderedPacketIterator = find_if(unAckedPacketList.begin(), unAckedPacketList.end(), bind2nd(equalSeqNum(), redunSeqNum));

                                // An unacked packet exists with this sequence number, delete it
                                // from the list and consider it acked.
                                if(reOrderedPacketIterator != unAckedPacketList.end())
                                {
                                        tmpPacketInfo.seqNum = (*reOrderedPacketIterator).seqNum;
                                        tmpPacketInfo.packetSize = (*reOrderedPacketIterator).packetSize;
                                        tmpPacketInfo.timeStamp = (*reOrderedPacketIterator).timeStamp;
                                        tmpPacketInfo.isFake = (*reOrderedPacketIterator).isFake;

                                        ackedPackets.push_back(tmpPacketInfo);

                                        unAckedPacketList.erase(reOrderedPacketIterator);
                                        statReorderedPackets++;
                                        logWrite(SENSOR,"STAT:: Number of reordered packets = %d",statReorderedPackets);
				}

			}
		}
	}


	tmpPacketInfo.seqNum = (*curPacketIterator).seqNum;
	tmpPacketInfo.packetSize = (*curPacketIterator).packetSize;
	tmpPacketInfo.timeStamp = (*curPacketIterator).timeStamp;
	tmpPacketInfo.isFake = (*curPacketIterator).isFake;

	ackedPackets.push_back(tmpPacketInfo);

	// Check for packet loss - if we have any unacked packets with sequence
	// numbers less than the received ACK seq number, then the packets/or their ACKS
	// were lost - treat this as congestion on the forward path.

	// Find out how many packets were lost.
	struct UdpPacketCmp comparePacket;

	comparePacket.seqNum = seqNum;
	comparePacket.timeStamp = (*curPacketIterator).timeStamp;

	listIterator = find_if(sentPacketList.begin(), curPacketIterator, bind2nd(lessSeqNum(), &comparePacket)); 

	if( (listIterator != sentPacketList.end()) && (listIterator != curPacketIterator ))
	{
		logWrite(SENSOR, "STAT::Packet being ACKed = %d",seqNum);

		do{
			logWrite(SENSOR, "STAT::Lost packet seqnum = %d",(*listIterator).seqNum);

                        // This packet might have been lost - but store it as UnAcked
                        // to account for reordering on the forward path.
                        tmpPacketInfo.seqNum = (*listIterator).seqNum;
                        tmpPacketInfo.packetSize = (*listIterator).packetSize;
                        tmpPacketInfo.timeStamp = (*listIterator).timeStamp;
                        tmpPacketInfo.isFake = (*listIterator).isFake;

                        unAckedPacketList.push_back(tmpPacketInfo);

			sentPacketList.erase(listIterator);
			listIterator = sentPacketList.end();

			packetLoss++;
			totalPacketLoss++;

			listIterator = find_if(sentPacketList.begin(), curPacketIterator, bind2nd(lessSeqNum(), &comparePacket )); 

		}
		while( (listIterator != sentPacketList.end()) && (listIterator != curPacketIterator) );
		logWrite(SENSOR,"STAT::Total packet loss = %d",totalPacketLoss);
	}

	sentPacketList.erase(curPacketIterator);
}

bool UdpPacketSensor::handleReorderedAck(PacketInfo *packet)
{
        list<UdpPacketInfo >::iterator listIterator;
        unsigned short int seqNum = *(unsigned short int *)(packet->payload + 1);
        bool retVal = false;

        listIterator = find_if(unAckedPacketList.begin(), unAckedPacketList.end(), bind2nd(equalSeqNum(), seqNum));

        if(listIterator == unAckedPacketList.end())
                retVal = false;
        else
        {
                retVal = true;
                statReorderedPackets++;
                logWrite(SENSOR,"STAT:: Number of reordered packets = %d",statReorderedPackets);
        }

        return retVal;
}

list<UdpPacketInfo>& UdpPacketSensor::getUnAckedPacketList()
{
	        return unAckedPacketList;
}

