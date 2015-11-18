#include "peer.h"
#include "protocol.h"
#include "exception.h"
#include "torrent.h"
#include "server.h"

using namespace std;

namespace torrent
{

	bool PeerInfo::operator==(const PeerInfo& rhs) const
	{
		return m_Address == rhs.m_Address && m_Port == rhs.m_Port;
	}

	PeerInfo::PeerInfo(const QHostAddress& address, quint16 port, const QString& peerId) : m_Address(address), m_Port(port), m_TrackedPeerId(peerId), m_AvailabilityAmount(0), m_DownloadedBytes(0), m_UploadedBytes(0), m_DownloadingSpeed(0), m_UploadingSpeed(0)
	{

	}

	quint64 PeerInfo::GetDownloadingSpeed() const
	{
		return m_DownloadingSpeed;
	}

	quint64 PeerInfo::GetUploadingSpeed() const
	{
		return m_UploadingSpeed;
	}


	quint64 PeerInfo::GetDownloadedBytes() const
	{
		return m_DownloadedBytes;
	}

	quint64 PeerInfo::GetUploadedBytes() const
	{
		return m_UploadedBytes;
	}

	QHostAddress PeerInfo::GetHostAddress() const
	{
		return m_Address;
	}

	quint16 PeerInfo::GetPort() const
	{
		return m_Port;
	}

	QString PeerInfo::GetPeerId() const
	{
		return m_PeerId;
	}

	bool PeerInfo::HasPiece(quint32 index)
	{
		if (index >= (quint32)m_PeerPieces.size())
			THROW;
		return m_PeerPieces.testBit(index);
	}

	Peer::PeerState Peer::GetState()
	{
		return m_State;
	}

	void Peer::Choke()
	{
		SendChoke();
		m_AmChoking = true;
	}

	void Peer::Unchoke()
	{
		SendUnchoke();
		m_AmChoking = false;
	}

	void Peer::Interesting()
	{
		if (!m_AmInterested)
		{
			SendInterested();
			m_AmInterested = true;
		}		
	}

	void Peer::NotInteresting()	
	{
		if (m_AmInterested)
		{
			SendNotInterested();
			m_AmInterested = false;
		}
	}

	bool Peer::AmChoking()
	{
		return m_AmChoking;
	}
	bool Peer::AmInterested()
	{
		return m_AmInterested;
	}

	bool Peer::PeerChoking()
	{
		return m_PeerChoking;
	}

	bool Peer::PeerInterested()
	{
		return m_PeerInterested;
	}


	void Peer::RequestPiece(quint32 index, quint32 begin, quint32 length)
	{
		//CHECK(!m_OutcomingRequestList.contains(PiecePart(index, begin, length)));			
		SendRequest(index, begin, length);
		//m_OutcomingRequestList.append(PiecePart(index, begin, length));
		
	}

	void Peer::CancelPiece(quint32 index, quint32 begin, quint32 length)
	{
		if (!m_OutcomingRequestList.contains(PiecePart(index, begin, length)))
			THROW;
		if (!m_OutcomingRequestList.removeOne(PiecePart(index, begin, length)))
			THROW;
		SendCancel(index, begin, length);
	}

	void Peer::UploadPiece(quint32 index, quint32 begin, quint32 length)
	{
		PiecePart piece(index, begin, length);
		if (m_UploadingPieceParts.contains(piece))
			THROW;
		m_UploadingPieceParts.append(piece);
	}


	void Peer::Have(quint32 index)
	{
		if (m_State != torrent::Peer::PeerState::PS_RECVING)
			return;

		if (!SendHave(index))
			THROW;
	}

	/*bool Peer::Process(quint32 deltaTime)
	{
		switch (m_State)
		{
			case PeerState::PS_NULL:
			{
									   if (!Connection(deltaTime))
										   return false;
			}
			break;
			case PeerState::PS_CONNECTED:
			{
											if (!TickTimers(deltaTime))
												return false;

											if (!ReciveHandshake())
												return false;
			}
			break;
			case PeerState::PS_HANDSHAKED:
			{
											 if (m_Torrent->GetInfoHash() != m_Infohash)
												 return false;

											 if (!m_SentHandshake)
											 {
												 if (!SendHandshake(m_Torrent->GetInfoHash()))
													 return false;
												 if (!SendPeerID())
													 return false;
												 m_SentHandshake = true;
											 }


											 if (!TickTimers(deltaTime))
												 return false;

											 if (!RecivePeerID())
												 return false;
			}
			break;
			case PeerState::PS_RECV_ID:
			{
										  if (!m_PeerInfo->m_TrackedPeerId.isEmpty() && m_PeerInfo->m_TrackedPeerId != m_PeerInfo->m_PeerId)
											  return false;

										  if (m_PeerInfo->m_PeerId == m_Torrent->m_Server->GetPeerId())
											  return false;

										  m_State = PeerState::PS_RECVING;

										  if (!TickTimers(deltaTime))
											  return false;
										  if (m_Torrent->m_Pieces.count(true) > 0)
										  {
											  if (!SendBitField(m_Torrent->m_Pieces))
												  return false;
										  }
			}
			break;
			case PeerState::PS_RECVING:
			{
										  if (!TickTimers(deltaTime))
											  return false;

										  if (!SendPackets())
											  return false;

										  if (!RecivePackets())
											  return false;
			}
			break;
			default:
			THROW;
			break;
		}
		return true;
	}
*/
	bool Peer::operator==(const PeerInfo& rhs) const
	{
		return *m_PeerInfo == rhs;
	}

	bool Peer::operator==(const Peer& rhs) const
	{
		return *m_PeerInfo == *rhs.m_PeerInfo;
	}


	Peer::Peer(QTcpSocket* socket) : m_State(PeerState::PS_CONNECTED), m_Torrent(nullptr), m_PeerInfo(new PeerInfo(socket->peerAddress(), socket->peerPort())), m_AmChoking(true), m_AmInterested(false), m_PeerChoking(true), m_PeerInterested(false), m_OutcomingRequestIndex(0),	m_OutcomingRequestBegin(0),
		m_Socket(socket), m_NextPacketLength(0), m_LocalKeepAliveTimer(this), m_RemoteKeepAliveTimer(this), m_SpeedTimer(0), m_DownloadedBytes(0), m_UploadedBytes(0), m_PrevDownloadedBytes(0), m_PrevUploadedBytes(0), m_SentHandshake(false), m_RequestSizePower(0), m_MaxRequestSizePower(2*PiecePartSize), m_DownloadingDeltaSpeed(0)
	{
		connect(&m_LocalKeepAliveTimer,SIGNAL(timeout()), SLOT(OnTimeout()));
		m_LocalKeepAliveTimer.start(KeepAliveInterval);
		connect(&m_RemoteKeepAliveTimer,SIGNAL(timeout()), SLOT(OnTimeout()));
		m_RemoteKeepAliveTimer.start(KeepAliveInterval);

		connect(m_Socket.data(),SIGNAL(readReady()), SLOT(OnReadReady()));
		//connect(m_Socket.data(),SIGNAL(disconnect()), SIGNAL(NeedRemove()));
		connect(m_Socket.data(),SIGNAL(error(QAbstractSocket::SocketError)), SLOT(OnError(QAbstractSocket::SocketError)));
	}

	Peer::Peer(Torrent* torrent, QSharedPointer<PeerInfo> peerInfo) : m_State(PeerState::PS_NULL), m_Torrent(torrent), m_PeerInfo(peerInfo), m_AmChoking(true), m_AmInterested(false), m_PeerChoking(true), m_PeerInterested(false), m_OutcomingRequestIndex(0),	m_OutcomingRequestBegin(0),
		m_Socket(nullptr), m_NextPacketLength(0), m_LocalKeepAliveTimer(this), m_RemoteKeepAliveTimer(this), m_SpeedTimer(0), m_DownloadedBytes(0), m_UploadedBytes(0), m_PrevDownloadedBytes(0), m_PrevUploadedBytes(0), m_SentHandshake(false), m_RequestSizePower(0), m_MaxRequestSizePower(2*PiecePartSize), m_DownloadingDeltaSpeed(0)
	{
		m_PeerInfo->m_PeerPieces.resize(torrent->PieceCount());
		SetupConnection();
	}

	void Peer::OnReadReady()
	{
		CHECK(m_State != PeerState::PS_NULL);

		ResetTimers();		

		if (m_State == PeerState::PS_CONNECTED)
		{
			if (!ReciveHandshake())
				return;
		}

		if (m_State == PeerState::PS_HANDSHAKED)
		{

			if (m_Torrent->GetInfoHash() != m_Infohash)
			{
				emit NeedRemove();
				return;
			}

			if (!m_SentHandshake)
			{
				if (!SendHandshake(m_Torrent->GetInfoHash()))
				{
					emit NeedRemove();
					return;
				}
				if (!SendPeerID())
				{
					emit NeedRemove();
					return;
				}
				m_SentHandshake = true;
			}

			if (!RecivePeerID())
				return;

			emit PeerIdRecived();

			if (!m_PeerInfo->m_TrackedPeerId.isEmpty() && m_PeerInfo->m_TrackedPeerId != m_PeerInfo->m_PeerId)
			{
				emit NeedRemove();
				return;
			}

			if (m_PeerInfo->m_PeerId == m_Torrent->m_Server->GetPeerId())
			{
				emit NeedRemove();
				return;
			}

			m_State = PeerState::PS_RECVING;

			if (m_Torrent->m_Pieces.count(true) > 0)
			{
				if (!SendBitField(m_Torrent->m_Pieces))
				{
					emit NeedRemove();
					return;
				}
			}

			emit DataTransferReady();			

			m_RequestSizePower += m_MaxRequestSizePower;
		}

		if (m_State == PeerState::PS_RECVING)
		{
			RecivePackets();			


		/*	static quint32 index = 0;
			static quint32 begin = 0;

			while (m_OutcomingRequestList.size() < 2)
			{
				quint32 lengthRem = m_Torrent->PieceLengthAt(index) - begin;
				quint32 length = qMin(lengthRem, PiecePartSize);

				RequestPiece(index,begin,length);

				begin += length;
				if (lengthRem == length)
				{
					begin = 0;
					index += 1;
					if (index == m_Torrent->PieceCount())
					{
						qWarning("Downloaded!!!");
						return;
					}
				}
			
			}*/


		}
	}

	void Peer::OnTimeout()
	{
		auto timer = sender();
		if (timer == &m_LocalKeepAliveTimer)
		{
			emit NeedRemove();
		}
		else
		{
			CHECK( timer == &m_RemoteKeepAliveTimer );
			emit NeedRemove();
		}
	}

	void Peer::ResetTimers()
	{	
		m_LocalKeepAliveTimer.stop();
		m_LocalKeepAliveTimer.start();
		m_RemoteKeepAliveTimer.stop();
		m_RemoteKeepAliveTimer.start();
	}

	void Peer::UpdateSpeeds(quint32 deltaTime)
	{
		//m_PeerInfo->m_DownloadingSpeed = (m_DownloadedBytes - m_PeerInfo->m_DownloadedBytes) * 1000 / deltaTime;
		quint32 prevDownloadingSpeed = m_PeerInfo->m_DownloadingSpeed;
		
		m_PeerInfo->m_DownloadingSpeed = m_DownloadedBytes - m_PeerInfo->m_DownloadedBytes;
		m_PeerInfo->m_DownloadedBytes = m_DownloadedBytes;
		m_DownloadingDeltaSpeed = m_PeerInfo->m_DownloadingSpeed - prevDownloadingSpeed;

	/*	m_PeerInfo->m_UploadingSpeed = (m_UploadedBytes - m_PeerInfo->m_UploadedBytes) * 1000 / deltaTime;
		m_PeerInfo->m_UploadedBytes = m_UploadedBytes;*/
	}

	void Peer::OnError(QAbstractSocket::SocketError error)
	{
		qInfo("Peer %s:%i SocketError %s", qPrintable(m_PeerInfo->GetHostAddress().toString()), m_PeerInfo->GetPort(), qPrintable(m_Socket->errorString()) );
		emit NeedRemove();
	}

	void Peer::OnConnected()
	{		
		connect(&m_LocalKeepAliveTimer,SIGNAL(timeout()), SLOT(OnTimeout()));
		m_LocalKeepAliveTimer.start(KeepAliveInterval);
		connect(&m_RemoteKeepAliveTimer,SIGNAL(timeout()), SLOT(OnTimeout()));
		m_RemoteKeepAliveTimer.start(KeepAliveInterval);
				
		if (!SendHandshake(m_Torrent->GetInfoHash()))
		{
			emit NeedRemove();
			return;
		}
		if (!SendPeerID())
		{
			emit NeedRemove();
			return;
		}
		m_SentHandshake = true;

		m_State = PeerState::PS_CONNECTED;
	}

	void Peer::Download()
	{
		if (m_State != PeerState::PS_RECVING)
		{
			return;
		}


		/*if (m_RequestSizePower == 0 || m_OutcomingRequestIndex == m_OutcomingRequestList.size())
			return;*/

		if (m_OutcomingRequestIndex == m_OutcomingRequestList.size())
			return;

				
		if (m_OutcomingRequestIndex == m_OutcomingRequestList.size())
		{
			NotInteresting();
		//	emit ChooseAlgorithm();
		}
		else
		{
			if (AmInterested())
			{
				if (PeerChoking())
					return;



				for (auto& piecePart : m_OutcomingRequestList)
				{
					SendRequest(piecePart.Index, piecePart.Begin, piecePart.Length);
				}
				m_OutcomingRequestIndex = m_OutcomingRequestList.size();
			}
			else
			{
				Interesting();
			}
		}
	}

	bool Peer::TickTimers(quint32 deltaTime)
	{
		//if (m_RemoteKeepAliveTimer >= deltaTime)
		//{
		//	m_RemoteKeepAliveTimer -= deltaTime;
		//}
		//else
		//{
		//	m_RemoteKeepAliveTimer = 0;
		//}
		//if (m_LocalKeepAliveTimer >= deltaTime)
		//{
		//	m_LocalKeepAliveTimer -= deltaTime;
		//}
		//else
		//{
		//	m_LocalKeepAliveTimer = 0;
		//}

		//m_SpeedTimer += deltaTime;




		//if (m_LocalKeepAliveTimer == 0)
		//{
		//	if (!SendKeepAlive())
		//		return false;
		//	
		//}

		//if (m_RemoteKeepAliveTimer == 0)
		//{
		//	return false;
		//}

		//if (m_SpeedTimer >= SpeedRefreshInterval)
		//{
		//	m_PeerInfo->m_DownloadingSpeed = (m_DownloadedBytes - m_PeerInfo->m_DownloadedBytes) * 1000 / m_SpeedTimer;
		//	m_PeerInfo->m_DownloadedBytes = m_DownloadedBytes;
		//	m_PeerInfo->m_UploadingSpeed = (m_UploadedBytes - m_PeerInfo->m_UploadedBytes) * 1000 / m_SpeedTimer;
		//	m_PeerInfo->m_UploadedBytes = m_UploadedBytes;
		//	m_SpeedTimer = 0;
		//}


		return true;
	}


	bool Peer::SendHandshake(const QByteArray& infohash)
	{
		qint8 buffer[48] = { 0, };
		qint8* pbuffer = buffer;

		pbuffer[0] = ProtocolIdSize;
		pbuffer += sizeof(ProtocolIdSize);

		memcpy(pbuffer, ProtocolId, ProtocolIdSize);
		pbuffer += ProtocolIdSize;

		memset(pbuffer, 0, 8);
		pbuffer += 8;

		memcpy(pbuffer, infohash.data(), infohash.size());
		pbuffer += m_Infohash.size();

		if (m_Socket->write((char*)buffer, sizeof(buffer)) != sizeof(buffer))
			return false;
		
		return true;
	}

	bool Peer::SendPeerID()
	{
		if (m_Socket->write(m_Torrent->m_Server->GetPeerId()) != PeerIdSize)
			return false;
		
		return true;
	}

	bool Peer::SendKeepAlive()
	{
		qint8 buffer[] = { 0, 0, 0, 0 };
		if (m_Socket->write((char*)buffer, sizeof(buffer)) != sizeof(buffer))
			return false;
		
		return true;
	}

	bool Peer::SendChoke()
	{
		qint8 buffer[] = { 0, 0, 0, 1, 0 };
		if (m_Socket->write((char*)buffer, sizeof(buffer)) != sizeof(buffer))
			return false;
		
		return true;
	}

	bool Peer::SendUnchoke()
	{
		qint8 buffer[] = { 0, 0, 0, 1, 1 };
		if (m_Socket->write((char*)buffer, sizeof(buffer)) != sizeof(buffer))
			return false;
		
		return true;
	}

	bool Peer::SendInterested()
	{
		qint8 buffer[] = { 0, 0, 0, 1, 2 };
		if (m_Socket->write((char*)buffer, sizeof(buffer)) != sizeof(buffer))
			return false;
		
		return true;
	}

	bool Peer::SendNotInterested()
	{
		qint8 buffer[] = { 0, 0, 0, 1, 3 };
		if (m_Socket->write((char*)buffer, sizeof(buffer)) != sizeof(buffer))
			return false;
		
		return true;
	}

	bool Peer::SendHave(quint32 index)
	{
		qint8 buffer[4 + 1 + 4] = { 0, 0, 0, 5, 4 };
		*(quint32*)(buffer + 5) = toNetworkData(index);
		if (m_Socket->write((char*)buffer, sizeof(buffer)) != sizeof(buffer))
			return false;
		
		return true;
	}

	bool Peer::SendBitField(const QBitArray& bitArray)
	{
		QByteArray buffer(4 + 1 + m_Torrent->BitFieldByteSize(), 0);

		quint8* pbuffer = (quint8*)buffer.data();
		*(quint32*)pbuffer = toNetworkData(1 + m_Torrent->BitFieldByteSize());
		pbuffer += sizeof(quint32);

		*pbuffer = 5;
		pbuffer += sizeof(quint8);


		quint32 byte = 0;
		quint32 bit = 0;
		// The peer has the following pieces available.
		for (int c = 0, size = bitArray.size(); c < size; c++)
		{
			pbuffer[byte] += (bitArray[c] << (7 - bit));

			if (++bit == 8)
			{
				bit = 0;
				++byte;
			}
		}

		if (m_Socket->write(buffer) != buffer.size())
			return false;
		
		return true;
	}

	bool Peer::SendRequest(quint32 index, quint32 begin, quint32 length)
	{
		qint8 buffer[4 + 1 + sizeof(quint32)* 3] = { 0, 0, 0, 1 + sizeof(quint32)* 3, 6 };
		qint32* pbuffer = (qint32*)&buffer[5];
		pbuffer[0] = toNetworkData(index);
		pbuffer[1] = toNetworkData(begin);
		pbuffer[2] = toNetworkData(length);

		m_Socket->write((char*)buffer, sizeof(buffer));
		return true;
	}

	bool Peer::SendPiece(quint32 index, quint32 begin, quint32 length)
	{
		QByteArray buffer(4 + 9 + length, 0);
		quint8* pbuffer = (quint8*)buffer.data();

		*(quint32*)pbuffer = toNetworkData(9 + length);
		pbuffer += sizeof(quint32);

		*pbuffer = 7;
		pbuffer += 1;


		*(quint32*)pbuffer = toNetworkData(index);
		pbuffer += sizeof(quint32);

		*(quint32*)pbuffer = toNetworkData(begin);
		pbuffer += sizeof(quint32);

		m_Torrent->ReadPiecePart(index, begin, length, pbuffer);

		if (m_Socket->write(buffer) != buffer.size())
			return false;
		
		return true;
	}

	bool Peer::SendCancel(quint32 index, quint32 begin, quint32 length)
	{
		qint8 buffer[4 + 1 + sizeof(quint32)* 3] = { 0, 0, 0, 1 + sizeof(quint32)* 3, 8 };
		qint32* pbuffer = (qint32*)&buffer[5];
		pbuffer[0] = toNetworkData(index);
		pbuffer[1] = toNetworkData(begin);
		pbuffer[2] = toNetworkData(length);

		if (m_Socket->write((char*)buffer, sizeof(buffer)) != sizeof(buffer))
			return false;
		
		return true;
	}

	bool Peer::RecivePackets()
	{
		CHECK(m_Socket->isValid() && m_Socket->state() == QAbstractSocket::ConnectedState);
				
		quint32 sum_length = 0;

		do
		{
			if (!m_NextPacketLength)
			{
				if (m_Socket->bytesAvailable() < sizeof(m_NextPacketLength))
					return false;

				m_NextPacketLength = fromNetworkData((const char *)m_Socket->read(sizeof(m_NextPacketLength)));
			}

			if (m_Socket->bytesAvailable() < m_NextPacketLength)
				return false;

			

			if (m_NextPacketLength == 0)//KeepAlivePacket
			{
				return true;
			}

			QByteArray packet = m_Socket->read(m_NextPacketLength);
			if (packet.size() != m_NextPacketLength)
			{
				emit NeedRemove();
				return false;
			}


			switch ((PacketType)packet.at(0))
			{
				case PacketType::ChokePacket:
				// We have been choked.
				ReciveChoke();

				break;
				case PacketType::UnchokePacket:
				// We have been unchoked.
				ReciveUnchoke();

				break;
				case PacketType::InterestedPacket:
				// The peer is interested in downloading.
				ReciveInterested();

				break;
				case PacketType::NotInterestedPacket:
				// The peer is not interested in downloading.
				ReciveNotInterested();

				break;
				case PacketType::HavePacket:
				{
											   // The peer has a new piece available.
											   quint32 index = fromNetworkData(&packet.data()[1]);
											   if (!ReciveHave(index))
											   {
												   emit NeedRemove();
												   return false;
											   }
				}
				break;
				case PacketType::BitFieldPacket:
				{
												   if (m_NextPacketLength - 1 != m_Torrent->BitFieldByteSize())
												   {
													   return false;
												   }

												   QBitArray pieces(m_Torrent->PieceCount());

												   quint32 bit = 0;
												   quint32 byte = 0;
												   for (quint32 c_piece = 0, size_piece = m_Torrent->PieceCount(); c_piece < size_piece; c_piece++)
												   {
													   if (packet[1 + byte] & (1 << (7 - bit)))
														   pieces[c_piece] = true;
													   else
														   pieces[c_piece] = false;

													   if (++bit == 8)
													   {
														   ++byte;
														   bit = 0;
													   }
												   }

												   if (!ReciveBitField(pieces))
													   return false;
				}
				break;
				case PacketType::RequestPacket:
				{
												  // The peer requests a block.
												  quint32 index = fromNetworkData(&packet.data()[1]);
												  quint32 begin = fromNetworkData(&packet.data()[5]);
												  quint32 length = fromNetworkData(&packet.data()[9]);

												  if (!ReciveRequest(index, begin, length))
													  return false;
				}
				break;
				case PacketType::PiecePacket:
				{
												quint32 index = quint32(fromNetworkData(&packet.data()[1]));
												quint32 begin = quint32(fromNetworkData(&packet.data()[5]));
												quint32 length = m_NextPacketLength - 9;

												if (!RecivePiece(index, begin, length, (quint8*)packet.data() + 9))
													return false;	
											//	sum_length += length;
				
				}
				break;
				case PacketType::CancelPacket:
				{
												 // The peer cancels a block request.
												 quint32 index = fromNetworkData(&packet.data()[1]);
												 quint32 begin = fromNetworkData(&packet.data()[5]);
												 quint32 length = fromNetworkData(&packet.data()[9]);

												 if (!ReciveCancel(index, begin, length))
													 return false;
				}
				break;
				default:
				// Unsupported packet type; just ignore it.
				break;
			}

			m_NextPacketLength = 0;
		} while (m_Socket->bytesAvailable());


		
		//m_RequestSizePower += sum_length;
		//if (sum_length)
		//{
		//	Download();
		//	//emit ChooseAlgorithm();
		//}

		return true;
	}

	bool Peer::ReciveHandshake()
	{
		CHECK(m_Socket->isValid() && m_Socket->state() == QAbstractSocket::ConnectedState);
		
		if (m_Socket->bytesAvailable() < MinimalHeaderSize)
			return false;

		// Sanity check the protocol ID
		QByteArray id = m_Socket->read(ProtocolIdSize + 1);
		if (id.at(0) != ProtocolIdSize || !id.mid(1).startsWith((char*)ProtocolId))
		{
			emit NeedRemove();
			return false;			
		}

		// Discard 8 reserved bytes, then read the info hash and peer ID
		(void)m_Socket->read(8);

		// Read infoHash
		m_Infohash = m_Socket->read(20);

		m_State = PeerState::PS_HANDSHAKED;

		emit InfohashRecived(m_Infohash);

		return true;
	}

	bool Peer::RecivePeerID()
	{
		CHECK(m_Socket->isValid() && m_Socket->state() == QAbstractSocket::ConnectedState);

		if (m_Socket->bytesAvailable() < PeerIdSize)
			return false;

		m_PeerInfo->m_PeerId = m_Socket->read(PeerIdSize);		

		m_State = PeerState::PS_RECV_ID;

		return true;
	}

	void Peer::ReciveChoke()
	{
		if (!m_PeerChoking)
		{
			emit ChooseAlgorithm();
			m_PeerChoking = true;
		}

	}

	void Peer::ReciveUnchoke()
	{
		m_PeerChoking = false;

		//Download();
	}

	void Peer::ReciveInterested()
	{
		m_PeerInterested = true;
		m_PeerChoking = false;
	}

	void Peer::ReciveNotInterested()
	{
		m_PeerInterested = false;
	}

	bool Peer::ReciveHave(quint32 index)
	{
		if (index >= quint32(m_PeerInfo->m_PeerPieces.size()))
			return false;

		m_PeerInfo->m_PeerPieces.setBit(int(index));		
		//m_PeerInfo->m_PieceIndices << index;
		m_Torrent->OnReciveHave(this,index);

	//	emit ChooseAlgorithm();		

		return true;
	}

	bool Peer::ReciveBitField(const QBitArray& pieces)
	{
		if (m_PeerInfo->m_PeerPieces.size() != pieces.size())
		{
			emit NeedRemove();
			return false;
		}

		m_PeerInfo->m_PeerPieces = pieces;

		
		//for (quint32 c = 0, size = m_Torrent->m_PieceCount; c < size; c++)
		//{
		//	if (pieces[c])
		//	{
		//		m_PeerInfo->m_PieceIndices << c;
		//	}
		//}

		m_Torrent->OnReciveBitField(this,pieces);

	//	emit ChooseAlgorithm();

		return true;
	}

	bool Peer::ReciveRequest(quint32 index, quint32 begin, quint32 length)
	{
		if (index >= (quint32)m_PeerInfo->m_PeerPieces.size())
		{
			emit NeedRemove();
			return false;			
		}

		if (length == 0)
		{
			emit NeedRemove();
			return false;			
		}

		if (m_Torrent->PieceOffset(index) + begin + length > m_Torrent->GetTotalBytes())
		{
			emit NeedRemove();
			return false;			
		}

		if (m_IncomingRequestList.contains(PiecePart(index, begin, length)))
			return true;//ignoring

		m_IncomingRequestList.append(PiecePart(index, begin, length));
		return true;
	}


	bool Peer::RecivePiece(quint32 index, quint32 begin, quint32 length, quint8* data)
	{
		//if (m_OutcomingRequestIndex >= m_OutcomingRequestList.size())
		//	return true;//ignoring		
		for (quint32 piece = 0; piece < m_OutcomingRequestIndex; piece++ )
		{
			if (m_OutcomingRequestList[piece].Index == index && m_OutcomingRequestList[piece].Begin == begin && m_OutcomingRequestList[piece].Length == length)
			{
				m_Torrent->WritePiecePart(index, begin, length, data);
				m_DownloadedBytes += length;			
				m_OutcomingRequestList.removeAt(piece);
				--m_OutcomingRequestIndex;
				return true;
			}
		}
		return true;//ignoring
	}

	bool Peer::ReciveCancel(quint32 index, quint32 begin, quint32 length)
	{
		if (index >= (quint32)m_PeerInfo->m_PeerPieces.size())
		{
			emit NeedRemove();
			return false;			
		}

		if (length == 0)
		{
			emit NeedRemove();
			return false;			
		}

		if (m_Torrent->PieceOffset(index) + begin + length >= m_Torrent->GetTotalBytes())
		{
			emit NeedRemove();
			return false;			
		}

		if (!m_IncomingRequestList.contains(PiecePart(index, begin, length)))
			return true;//ignoring

		if (!m_IncomingRequestList.removeOne(PiecePart(index, begin, length)))
			THROW;
		return true;
	}

	void Peer::SetupConnection()
{
		m_Socket.reset(new QTcpSocket);
		connect(m_Socket.data(), SIGNAL(connected()), SLOT(OnConnected()));
		
		connect(m_Socket.data(),SIGNAL(readyRead()), SLOT(OnReadReady()));
		//connect(m_Socket.data(),SIGNAL(disconnected()), SIGNAL(NeedRemove()));
		connect(m_Socket.data(),SIGNAL(error(QAbstractSocket::SocketError)), SLOT(OnError(QAbstractSocket::SocketError)));
				
		
		m_Socket->connectToHost(m_PeerInfo->m_Address, m_PeerInfo->m_Port);
	}


	bool Peer::SendPackets()
	{
		for (auto& part : m_UploadingPieceParts)
		{
			if (!SendPiece(part.Index, part.Begin, part.Length))
				return false;
			m_UploadedBytes += part.Length;
		}
		m_UploadingPieceParts.clear();
		return true;
	}

	bool PiecePart::operator==(const PiecePart& rhs) const
	{
		return Index == rhs.Index && Begin == rhs.Begin && Length == rhs.Length;
	}

	PiecePart::PiecePart()
	{

	}

	PiecePart::PiecePart(quint32 index, quint32 begin, quint32 length) : Index(index), Begin(begin), Length(length)
	{

	}

	DataPiece::DataPiece(quint32 index, quint32 begin, quint32 length, quint8* data) : PiecePart(index, begin, length), Data((char*)data, length)
	{

	}

}
