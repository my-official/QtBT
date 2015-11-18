#ifndef PROTOCOL_H
#define PROTOCOL_H

namespace torrent
{

	//static const int PendingRequestTimeout = 60 * 1000;
	//static const int ClientTimeout = 120 * 1000;
	//static const int ConnectTimeout = 60 * 1000;
	//static const int KeepAliveInterval = 30 * 1000;
	//static const int RateControlTimerDelay = 2000;
	//static const int MinimalHeaderSize = 48;
	//static const char ProtocolId[] = "BitTorrent protocol";
	//static const char ProtocolIdSize = 19;


	static const int PendingRequestTimeout = 60 * 1000;
	static const int ClientTimeout = 120 * 1000;
	static const int ConnectTimeout = 60 * 1000;
	static const int KeepAliveInterval = 40 * 1000;
	static const int RateControlTimerDelay = 2000;
	static const int MinimalHeaderSize = 48;
	static const quint8 ProtocolId[] = "BitTorrent protocol";
	static const quint8 ProtocolIdSize = 19;


	static const int SpeedRefreshInterval = 1000;
	
	


	static const quint32 TrackerDefaultInterval = 10 * 1000;
	static const quint32 PiecePartSize = 1024 * 32;
	//static const int MaxSeederPerTorrent = 50;
	//static const int MaxLeechersPerTorrent = 50;
	//static const int MaxPeersPerTorrent = MaxSeederPerTorrent + MaxLeechersPerTorrent;
    static const int MaxConnectionsPerTorrent = 100;

	//extern quint8 PeerId[20];
	static const quint8 PeerIdSize = 20;

	//inline static QString qPeerId()
	//{
	//	return QString::fromLatin1((char*)PeerId, PeerIdSize);
	//}




	enum class PacketType : quint8
	{
        ChokePacket = 0,
        UnchokePacket = 1,
        InterestedPacket = 2,
        NotInterestedPacket = 3,
        HavePacket = 4,
        BitFieldPacket = 5,
        RequestPacket = 6,
        PiecePacket = 7,
        CancelPacket = 8
    };




	// Reads a 32bit unsigned int from data in network order.
static inline quint32 fromNetworkData(const char *data)
{
    const unsigned char *udata = (const unsigned char *)data;
    return (quint32(udata[0]) << 24)
        | (quint32(udata[1]) << 16)
        | (quint32(udata[2]) << 8)
        | (quint32(udata[3]));
}

//// Writes a 32bit unsigned int from num to data in network order.
//static inline void toNetworkData(quint32 num, char *data)
//{
//    unsigned char *udata = (unsigned char *)data;
//    udata[3] = (num & 0xff);
//    udata[2] = (num & 0xff00) >> 8;
//    udata[1] = (num & 0xff0000) >> 16;
//    udata[0] = (num & 0xff000000) >> 24;
//}

static inline quint32 fromNetworkData(quint32 data)
{
	return ((data & 0xFF000000) >> 24) +
		((data & 0x00FF0000) >> 8) +
		((data & 0x0000FF00) << 8) +
		((data & 0x000000FF) << 24);
}

// Writes a 32bit unsigned int from num to data in network order.
static inline quint32 toNetworkData(quint32 data)
{
	return ((data & 0xFF000000) >> 24) +
		((data & 0x00FF0000) >> 8) +
		((data & 0x0000FF00) << 8) +
		((data & 0x000000FF) << 24);
}



//static inline void fromNetworkData(quint32& data)
//{
//	data = (data & 0xFF000000) >> 24 +
//		(data & 0x00FF0000) >> 8 +
//		(data & 0x0000FF00) << 8 +
//		(data & 0x000000FF) << 24;
//}

//// Writes a 32bit unsigned int from num to data in network order.
//static inline void toNetworkData(quint32& data)
//{
//	data = (data & 0xFF000000) >> 24 +
//		(data & 0x00FF0000) >> 8 +
//		(data & 0x0000FF00) << 8 +
//		(data & 0x000000FF) << 24;
//}


	static const quint32 MaxSimultaneousOpenedFiles = 100;


}

#endif
