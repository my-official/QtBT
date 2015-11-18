#include "torrent.h"
#include <algorithm>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QNetworkInterface>

#include "protocol.h"
#include "exception.h"
#include "bencodeparser.h"
#include "filecache.h"
#include "server.h"
#include "verificator.h"

using namespace std;


namespace torrent
{

	Torrent::State Torrent::GetState() const
	{
		return m_State;
	}
	 
	QString Torrent::GetStateString() const
	{
		switch (m_State)
		{
			case torrent::Torrent::State::TS_STOPPED:
			return "Stopped";
			break;
			case torrent::Torrent::State::TS_PREPARING:
			return "Preparing";
			break;
			case torrent::Torrent::State::TS_DOWNLOADING:
			return "Downloading";
			break;
			case torrent::Torrent::State::TS_SEEDING:
			return "Seeding";
			break;
			case torrent::Torrent::State::TS_PAUSED:
			return "Paused";
			break;
			default:
			return "Unknown";
			break;
		}
	}

	void Torrent::SetDestDir(const QString& destDir)
	{
		m_DestDir = (destDir.isEmpty() ? "" : destDir + QDir::separator()) + (name().isEmpty() ? "" : name() + QDir::separator());
	}

	void Torrent::AddTrackerUrl(const QUrl& url)
	{
		QSharedPointer<Tracker> p(new Tracker(m_Server, this, url));
		connect(p.data(), SIGNAL(NewPeerInfo(const QSharedPointer<PeerInfo>&)), SLOT(OnNewPeerInfo(const QSharedPointer<PeerInfo>&)));
		m_Trackers.append( p );
	}

	void Torrent::AddIncomingPeer(const QSharedPointer<Peer>& peer)
	{
		if (!HasPeerInfo(*peer->m_PeerInfo))
		{
			m_PeerInfoList.append(peer->m_PeerInfo);
		}

		if (HasPeerConnection(*peer->m_PeerInfo))
			return;

		if (m_Peers.size() + 1 >= MaxConnectionsPerTorrent)
			return;

		InitializePeer(*peer.data());
		m_Peers.append(peer);
		m_CheckingPeerTimer = 0;
	}

	bool Torrent::AddPeerInfo(const QHostAddress& address, quint16 port)
	{
		for (auto& peerInfo : m_PeerInfoList)
		{			
			if (peerInfo->GetHostAddress() == address && peerInfo->GetPort() == port)
			{
				return false;
			}
		}


		if (m_Server->GetPort() == port)
		{
			for (auto& myaddress : QNetworkInterface::allAddresses())
			{
				if (myaddress == address)
					return false;
			}
		}	

		QSharedPointer<PeerInfo> newPeerInfo(new PeerInfo(address, port));
		m_PeerInfoList.append(newPeerInfo);
		return true;
	}

	bool Torrent::AddPeerInfo(const QSharedPointer<PeerInfo>& peerInfo)
	{
		QString peerId = peerInfo->GetPeerId();
		if (!peerId.isEmpty())
		{
			for (auto& peerInfo : m_PeerInfoList)
			{
				if (peerInfo->GetPeerId() == peerId)
				{
					return false;
				}
			}
		}

		QHostAddress address = peerInfo->GetHostAddress();
		quint16 port = peerInfo->GetPort();

		for (auto& peerInfo : m_PeerInfoList)
		{			
			if (peerInfo->GetHostAddress() == address && peerInfo->GetPort() == port)
			{
				return false;
			}
		}


		if (m_Server->GetPort() == port)
		{
			for (auto& myaddress : QNetworkInterface::allAddresses())
			{
				if (myaddress == address)
					return false;
			}
		}	

		QSharedPointer<PeerInfo> newPeerInfo(new PeerInfo(address, port));
		m_PeerInfoList.append(newPeerInfo);
		return true;

		
	}

	QByteArray Torrent::GetInfoHash() const
	{
		return m_InfoHash;
	}

	quint64 Torrent::GetUploadedBytesPerSession() const
{
		quint64 result = 0;
		for (auto& peer : m_Peers)
		{
			result += peer->m_PeerInfo->m_UploadedBytes;
		}
		return result;
	}

	quint64 Torrent::GetDownloadedBytesPerSession() const
{
		quint64 result = 0;
		for (auto& peer : m_Peers)
		{
			result += peer->m_PeerInfo->m_DownloadedBytes;
		}
		return result;
	}

	quint64 Torrent::GetLeftBytes() const
{
		quint64 result = 0;
		for (quint32 c = 0, size = m_PieceCount; c < size; c++)
		{
			if (m_Pieces[c])
				result += PieceLengthAt(c);
		}
		for_each(m_DownloadedParts.begin(), m_DownloadedParts.end(), [&result](const std::pair<quint32, PiecePart>& part)
		{
			result += part.second.Length;
		});

		return totalSize() - result;
	}

	quint64 Torrent::GetTotalBytes() const
{
		return totalSize();
	}

	quint64 Torrent::GetDownloadingSpeed() const
{
		quint32 result = 0;
		for (auto& peer : m_Peers)
		{
			result += peer->m_PeerInfo->GetDownloadingSpeed();
		}
		return result;
	}

	quint64 Torrent::GetUploadingSpeed() const
{
		quint32 result = 0;
		for (auto& peer : m_Peers)
		{
			result += peer->m_PeerInfo->GetUploadingSpeed();
		}
		return result;
	}

	quint32 Torrent::GetActivePeerCount() const
{
		return m_Peers.size();
	}

	quint32 Torrent::GetPeerCount() const
{
		return m_PeerInfoList.size();
	}

	QList< QSharedPointer<PeerInfo> > Torrent::GetPeers() const
{
		return m_PeerInfoList;
	}

	QList< QSharedPointer<PeerInfo> > Torrent::GetActivePeers() const
{
		QList< QSharedPointer<PeerInfo> > activePeers;

		for (auto& peer : m_Peers)
		{
			activePeers.append(peer->m_PeerInfo);
		}		
		return activePeers;
	}

	void Torrent::WritePiecePart(quint32 index, quint32 begin, quint32 length, quint8* data)
	{
		PiecePart newPart(index, begin, length);

		quint32 newBegin = begin;
		quint32 newEnd = begin + length;

		quint32 sum = 0;
		bool merged = false;		

		auto range = m_DownloadedParts.equal_range(index);

		for (auto it = range.first; it != range.second; ++it)
		{			
			quint32 oldBegin = it->second.Begin;
			quint32 oldEnd = it->second.Begin + it->second.Length;			

			if (newEnd + 1 == oldBegin)
			{
				Write(index, begin, length, data);
				it->second.Begin = newBegin;

				merged = true;
				break;
			}
			else
			{
				if (oldEnd + 1 == newBegin)
				{
					Write(index, begin, length, data);
					it->second.Length += length;
					merged = true;
					break;
				}
				else
				{
					CHECK(!(newBegin >= oldBegin && newEnd <= oldEnd));//не поглощение
					//qWarning("Merging piece %i, begin %i, length %i, newBegin %i, newlength %i", index,oldBegin, it->second.Length, newBegin, length);
				}
			}
		}

		if (!merged)
		{
			Write(index, begin, length, data);	
			sum += length;
		}		

		std::for_each(range.first, range.second, [&sum](const std::pair<quint32, PiecePart>& part)
		{
			sum += part.second.Length;
		});

		if (sum == PieceLengthAt(index))
		{
			if (CheckPieceSha(index))
			{
				m_Pieces.setBit(index);
				for (auto peer : m_Peers)
					peer->Have(index);				
			}
			else
			{
				qWarning("Sha error %i",index);
			}
			m_DownloadedParts.erase(index);
		}
		else
		{
			if (!merged)
			{
				m_DownloadedParts.emplace(make_pair(index, newPart));
			}
		}		
	}

	void Torrent::ReadPiecePart(quint32 index, quint32 begin, quint32 length, quint8* data)
	{
		QByteArray result = Read(index, begin, length);
		if (result.isEmpty())
			THROW;
		memcpy(data, result.data(), length);
	}

	void Torrent::Stop()
	{
		if (m_State == State::TS_SEEDING || m_State == State::TS_DOWNLOADING)
		{
			for (auto& tracker : m_Trackers)
			{
				tracker->Stop();
			}
		}

		m_Peers.clear();

		if (fileForm() == MetaInfo::MultiFileForm)
		{
			for (auto& file : multiFiles())
			{
				const QString filename = m_DestDir + file.path;
				FileCache::instance()->CloseFile(filename);
			}
		}
		else
		{
			const QString filename = m_DestDir + singleFile().name;
			FileCache::instance()->CloseFile(filename);
		}		

		m_State = State::TS_STOPPED;		
	}

	QByteArray Torrent::Read(quint64 pieceIndex, quint64 begin, quint64 length) const
	{
		quint64 offset = PieceOffset(pieceIndex) + begin;

		CHECK(length > 0);			

		if ((offset >= (quint64)totalSize()) || (offset + length > (quint64)totalSize()))
			THROW;

		if (fileForm() == MetaInfo::MultiFileForm)
		{
			QList<MetaInfoMultiFile> files = multiFiles();

			QByteArray result;

			auto file = files.begin();

			{
				while (offset > (quint64)file->length)
				{
					offset -= file->length;
					++file;
					CHECK(file != files.end());					
				}

				const QString filename = m_DestDir + file->path;
				quint64 size = qMin(length, (quint64)file->length - offset);
				QByteArray data;
				if (!FileCache::instance()->ReadFromFile(data, filename, offset, size))
					THROW;

				result.append(data);
				length -= size;
			}

			while (length)
			{
				++file;
				CHECK(file != files.end());				

				const QString filename = m_DestDir + file->path;
				quint64 size = qMin(length, (quint64)file->length);
				QByteArray data;
				if (!FileCache::instance()->ReadFromFile(data, filename, 0, size))
					THROW;

				result.append(data);
				length -= size;
			}

			return result;
		}
		else
		{
			const QString filename = m_DestDir + singleFile().name;
			QByteArray result;
			if (!FileCache::instance()->ReadFromFile(result, filename, offset, length))
				THROW;
			return result;
		}
	}

	void Torrent::Write(quint64 pieceIndex, quint64 begin, quint64 length, quint8* write) const
	{		
		quint64 offset = PieceOffset(pieceIndex) + begin;
		if ((offset >= (quint64)totalSize()) || (offset + length > (quint64)totalSize()))
			THROW;

		if (fileForm() == MetaInfo::MultiFileForm)
		{
			QList<MetaInfoMultiFile> files = multiFiles();
			
			auto file = files.begin();

			{
				while (offset > (quint64)file->length)
				{
					offset -= file->length;
					++file;
					CHECK(file != files.end());					
				}

				const QString filename = m_DestDir + file->path;
				quint64 size = qMin(length, (quint64)file->length - offset);
				if (!FileCache::instance()->WriteToFile(filename, offset, size, write))
					THROW;

				write += size;
				length -= size;
			}

			while (length)
			{
				++file;
				if (file == files.end())
					THROW;

				const QString filename = m_DestDir + file->path;
				quint64 size = qMin(length, (quint64)file->length);
				if (!FileCache::instance()->WriteToFile(filename, 0, size, write))
					THROW;

				write += size;
				length -= size;
			}
		}
		else
		{
			const QString filename = m_DestDir + singleFile().name;
			if (!FileCache::instance()->WriteToFile(filename, offset, length, write))
				THROW;
		}

	}

	bool Torrent::CheckPieceSha(quint32 pieceIndex) const
	{
		if (pieceIndex >= m_PieceCount)
			THROW;

		QByteArray buffer = Read(pieceIndex, 0, PieceLengthAt(pieceIndex));
		if (buffer.isEmpty())
			return false;

		if (fileForm() == MetaInfo::MultiFileForm)
		{
			return sha1Sums()[pieceIndex] == QCryptographicHash::hash(buffer, QCryptographicHash::Sha1);
		}
		else
		{
			return singleFile().sha1Sums[pieceIndex] == QCryptographicHash::hash(buffer, QCryptographicHash::Sha1);
		}
	}

	quint32 Torrent::PieceCount() const
	{
		return m_PieceCount;
	}

	int Torrent::BitFieldByteSize() const
	{
		return m_PieceCount / 8 + (m_PieceCount % 8 ? 1 : 0);
	}

	quint32 Torrent::PieceLengthAt(quint32 pieceIndex) const
	{
		if (pieceIndex >= m_PieceCount)
			THROW;

		if (pieceIndex == m_PieceCount - 1)
		{
			quint32 remainder = totalSize() % pieceLength();
			return remainder ? remainder : pieceLength();
		}
		else
			return pieceLength();
	}

	quint64 Torrent::PieceOffset(quint32 pieceIndex) const
	{
		return (quint64)pieceIndex * (quint64)pieceLength();
	}

	bool Torrent::HasPeerInfo(const PeerInfo& peerInfo) const
	{
		if (peerInfo.m_PeerId == m_Server->GetPeerId())
			return true;

		//qWarning("peer %s", qPrintable(peerInfo.m_Address.toString()));
		//qWarning("peer port %s", qPrintable(QString::number(peerInfo.m_Port)));
		for (auto& address : QNetworkInterface::allAddresses())
		{
			//qWarning("my %s", qPrintable(address.toString()));
			//qWarning("my port %s", qPrintable(QString::number(m_Server->GetPort())));

			if (address == peerInfo.m_Address && m_Server->GetPort() == peerInfo.m_Port)
				return true;
		}

		for (auto& peer : m_PeerInfoList)
		{
			if (*peer == peerInfo)
				return true;
		}
		return false;
	}

	bool Torrent::HasPeerConnection(const PeerInfo& peerInfo) const
	{
		for (auto& address : QNetworkInterface::allAddresses())
		{
			if (address == peerInfo.m_Address && m_Server->GetPort() == peerInfo.m_Port)
				return true;
		}

		for (auto& actvePeer : m_Peers)
		{
			if (*actvePeer == peerInfo)
				return true;
		}
		return false;
	}

	/*void Torrent::DownloadingAlgorithm()
	{
		if (m_Peers.isEmpty())
			return;

		quint32 c_index = 0;

		for (quint32 c_peer = 0, size_peer = m_Peers.size(); c_peer < size_peer; c_peer++)
		{
			QSharedPointer<Peer> peer = m_Peers[c_peer];
			if (peer.isNull())
				THROW;

			if (peer->GetState() != Peer::PeerState::PS_RECVING)
				continue;


            const int max_num = 100;
	

			for (quint32 size_index = m_PieceCount; c_index < size_index; c_index++)
			{
				if (m_Pieces[c_index])
					continue;

				if (peer->m_OutcomingRequestList.size() >= max_num)
					break;


				if (peer->m_PeerInfo->HasPiece(c_index))
				{
					if (!peer->AmInterested())
					{
						peer->Interesting();
					}

					if (!peer->PeerChoking())
					{
						quint32 begin = 0;
						quint32 remainingLength = PieceLengthAt(c_index);

						while (remainingLength)
						{
							quint32 length = qMin(PiecePartSize, remainingLength);


							if (!peer->m_OutcomingRequestList.contains(PiecePart(c_index, begin, length)) &&
								m_DownloadedParts.end() == std::find_if(m_DownloadedParts.begin(), m_DownloadedParts.end(), [c_index, begin, length,max_num](const std::pair<quint32, DataPiece>& part)->bool
							{
								if (c_index != part.first)
									return false;

								if (part.second.Begin != begin)
									return false;

								if (part.second.Length != length)
									return false;

								return true;
							}))
							{
								peer->RequestPiece(c_index, begin, length);
								if (peer->m_OutcomingRequestList.size() >= max_num)
									break;
							}

							begin += length;
							remainingLength -= length;
						}

					}
				}
			}


		}

	}*/




	void Torrent::InitializePeer(Peer& peer)
	{
		peer.m_PeerInfo->m_PeerPieces.fill(false, m_PieceCount);

		
		peer.m_Torrent = this;		
	}

	QString Torrent::FileAt(quint32 piece)
	{
		if (piece >= m_PieceCount)
			THROW;

		if (fileForm() == MetaInfo::MultiFileForm)
		{
			quint64 offset = PieceOffset(piece);
			if (offset >= (quint64)totalSize())
				THROW;			

			auto files = multiFiles();

			auto file = files.begin();

			while (offset > (quint64)file->length)
			{
				offset -= file->length;
				++file;
				if (file == files.end())
					THROW;
			}
						
			return m_DestDir + file->path;

		}
		else
		{
			return m_DestDir + singleFile().name;
		}		
	}

	quint32 Torrent::LastPieceAtFile(const QString& filename)
	{
		if (fileForm() == MetaInfo::MultiFileForm)
		{
			QList<MetaInfoMultiFile> files = multiFiles();
			
			quint32 piece = 0;

			for (auto& file : files)
			{				
				piece += file.length / pieceLength() + (file.length % pieceLength() > 0 ? 1 : 0);

				if (m_DestDir + file.path == filename)
				{
					return piece;
				}
			}

			THROW;
		}
		else
		{
			if (m_DestDir + singleFile().name != filename)
				THROW;

			return m_PieceCount - 1;
		}
	}

	void Torrent::UpdateSpeeds(quint32 deltaTime)
	{
		if (m_State != State::TS_DOWNLOADING && m_State != State::TS_SEEDING)
			return;
		
		for (auto& peer : m_Peers)
		{
			peer->UpdateSpeeds(deltaTime);
			quint32 downloadingSpeed = peer->m_PeerInfo->m_DownloadingSpeed;			
			quint32 maxDownloadingSpeed = peer->m_MaxRequestSizePower;

			if (downloadingSpeed >= maxDownloadingSpeed)
			{
				peer->m_MaxRequestSizePower += 2*PiecePartSize;
				peer->m_RequestSizePower += 2*PiecePartSize;
			}
			else
			{
				if (maxDownloadingSpeed - downloadingSpeed >= 2*PiecePartSize && peer->m_MaxRequestSizePower > 2*PiecePartSize)
				{
					//quint32 value;
					//if (peer->m_MaxRequestSizePower < pieceLength() + 2 * PiecePartSize)
					//{
					//	value = peer->m_MaxRequestSizePower - 2 * PiecePartSize;			
					//}
					//else
					//{
					//	value = pieceLength();
					//}

					peer->m_MaxRequestSizePower -= 2*PiecePartSize;
					peer->m_RequestSizePower -= 2*PiecePartSize;
				}
			}
		}

		

		OnChooseAlgorithm();		
	}

	void Torrent::OnInfohashRecived(const QByteArray& infohash)
	{

	}

	void Torrent::OnPeerIdRecived()
	{

	}

	void Torrent::OnDataTransferReady()
	{

	}

	void Torrent::OnNeedRemove()
	{
		QObject* peer = sender();
		//CHECK(peer);
		for (auto it = m_Peers.begin(); it != m_Peers.end(); ++it)
		{			
			if (sender() == it->data())
			{
				qInfo("Removing peer %s:%i SocketError %s", qPrintable((*it)->m_PeerInfo->GetHostAddress().toString()), (*it)->m_PeerInfo->GetPort(), qPrintable((*it)->m_Socket->errorString()) );
				m_Peers.erase(it);
				return;
			}
		}
		//THROW;
	}

	

	void Torrent::OnNewPeerInfo(const QSharedPointer<PeerInfo>& newPeerInfo)
	{
		if (AddPeerInfo(newPeerInfo))
		{
			OnChooseAlgorithm();
		}
	}
		
	void Torrent::OnVerifyFinished(Torrent* torrent, QBitArray pieces)
	{
		if (this != torrent)
			return;

		disconnect(this, SLOT(OnVerifyFinished(Torrent*, QBitArray)));

		m_Pieces = pieces;

		bool hasAll = true;

		for (quint32 c = 0, size = m_PieceCount; c < size; c++)
		{
			if (!m_Pieces[c])
			{
				hasAll = false;
				break;
			}
		}
				
		if (hasAll)
		{
			m_State = State::TS_SEEDING;
			StartSeeding();
		}
		else
		{
			m_State = State::TS_DOWNLOADING;
			StartDownloading();
		}
	}



	

	void Torrent::OnReciveHave(Peer* peer, quint32 index)
	{
		if (m_Pieces[index])
			return;	
		
		++m_Pieces2Availability[index];

	/*	quint32 prevAvailability = m_Pieces2Availability[index];
		quint32 availability = ++m_Pieces2Availability[index];
		peer->m_PeerInfo->m_PeerAvailability2Index.insert( make_pair(availability, index) );

		for (auto& peerInfo : m_PeerInfoList)
		{
			if (peer->m_PeerInfo == peerInfo)
			{
				continue;
			}

			if (peerInfo->HasPiece(index))
			{
				auto range = peerInfo->m_PeerAvailability2Index.equal_range(prevAvailability);
				auto ret = std::find_if(range.first, range.second, [prevAvailability, index](const pair<quint32, quint32> rhs)->bool
				{
					return prevAvailability == rhs.first && index == rhs.second;
				});

				peerInfo->m_PeerAvailability2Index.erase(ret);
				peerInfo->m_PeerAvailability2Index.insert( make_pair(availability, index) );
			}
		}*/

	/*	auto range = m_Availability2Pieces.equal_range(availability);

		for (auto it = range.first; it != range.second; ++it)		
		{
			if (it->second == index)
			{
				it = m_Availability2Pieces.erase(it);				
				m_Availability2Pieces.emplace(availability + 1, index);
				++m_Pieces2Availability[index];
			}
		}
		THROW;*/

		
	}

	void Torrent::OnReciveBitField(Peer* peer, const QBitArray& pieces)
	{
		for (quint32 c = 0; c < m_PieceCount; c++)
		{
			if (pieces[c])
			{
				++m_Pieces2Availability[c];				
			}
		}


		//for (quint32 c = 0; c < m_PieceCount; c++)
		//{
		//	if (pieces[c])
		//	{
		//		quint32 availability = ++m_Pieces2Availability[c];		
		//		peer->m_PeerInfo->m_PeerAvailability2Index.insert( make_pair(availability, c) );
		//	}
		//}


		//for (auto& peerInfo : m_PeerInfoList)
		//{
		//	if (peer->m_PeerInfo == peerInfo)
		//	{
		//		continue;
		//	}

		//	RebuildPeerAvailabilityMap(peer->m_PeerInfo.data());
		//}

//		RebuildAvailabilityMap();
	}

	//void Torrent::RebuildAvailabilityMap()
	//{
	//	m_Availability2Pieces.clear();

	//	for (quint32 c = 0; c < m_PieceCount; c++)
	//	{
	//		if (!m_Pieces[c])
	//		{
	//			m_Availability2Pieces.insert(make_pair(m_Pieces2Availability[c], c));
	//			//AddToAvailabilityMap(m_Pieces2Availability[c], c);				
	//	
	//		}
	//	}
	//}

	bool Torrent::IsPartDownloaded(quint32 index, quint32 begin, quint32 length)
	{			
		auto result = std::find_if(m_DownloadedParts.begin(), m_DownloadedParts.end(), [index, begin, length](const std::pair<quint32, PiecePart>& part)->bool
		{
			if (part.first != index)
				return false;

			if ( (part.second.Begin <=  begin)   &&   ( (begin + length) <= (part.second.Begin + part.second.Length) ) ) 
				return true;
			else
				return false;
		});

		return result != m_DownloadedParts.end();
	}

	void Torrent::SeedingAlgorithm()
	{
		if (m_Peers.isEmpty())
			return;

		for (auto& peer : m_Peers)
		{						
			if (peer.isNull())
				THROW;

			if (peer->GetState() != Peer::PeerState::PS_RECVING)
				continue;

			if (peer->PeerChoking())
				continue;

			if (peer->AmChoking())
			{
				peer->Unchoke();
			}
			else
			{
				for (auto piece = peer->m_IncomingRequestList.begin(); piece != peer->m_IncomingRequestList.end(); piece++)
				{
					peer->UploadPiece(piece->Index, piece->Begin, piece->Length);
					peer->m_IncomingRequestList.erase(piece);
				}
			}
		}
	}


	Torrent::Torrent(Server* server, const MetaInfo& metainfo) : MetaInfo(metainfo), m_State(State::TS_STOPPED), m_Pause(false), m_Server(server), m_DestDir(""), m_InfoHash(QCryptographicHash::hash(metainfo.infoValue(), QCryptographicHash::Sha1)),
		m_PieceCount(totalSize() / pieceLength() + (totalSize() % pieceLength() ? 1 : 0)), m_Pieces(m_PieceCount), m_CheckingPiece(0),m_CheckingPeerTimer(0),m_Pieces2Availability(m_PieceCount,0)
	{
		//RebuildAvailabilityMap();
		AddTrackerUrl(announceUrl());
	}

	void Torrent::Prepare()
	{		
		connect(Verificator::Instance(),SIGNAL(VerifyFinished(Torrent*, QBitArray)), SLOT(OnVerifyFinished(Torrent*, QBitArray)));
		Verificator::Instance()->StartVerification(this);		
	}

	void Torrent::StartDownloading()
	{
		m_PiecePartBeginForDownloading.fill(0, m_PieceCount);

		OnChooseAlgorithm();

		for (auto& tracker : m_Trackers)
		{
			tracker->Start();			
		}
	}

	void Torrent::StartSeeding()
	{
		for (auto& tracker : m_Trackers)
		{
			tracker->Start();
		}
	}

	void Torrent::Start(bool skipPreparing /*= false*/)
{
		CHECK(m_State == State::TS_STOPPED);
		if (skipPreparing)
		{			
			StartSeeding();
			m_State = State::TS_SEEDING;
		}
		else
		{			
			Prepare();
			m_State = State::TS_PREPARING;
		}


		//switch (m_State)
		//{
		//	case Torrent::State::TS_STOPPED: Prepare();	break;
		//	case Torrent::State::TS_PREPARING: Prepare();	break;
		//	case Torrent::State::TS_DOWNLOADING: 
		//	case Torrent::State::TS_SEEDING:							
		//	break;
		//	case Torrent::State::TS_PAUSED:
		//		m_State = m_PrevState;				
		//	break;
		//	default:
		//		THROW;
		//}		
	}

	void Torrent::SetPause(bool pause)
	{
		m_Pause = pause;
	}


	bool Torrent::Pause()
	{
		return m_Pause;
	}

	bool Torrent::IsPartRequested(int index, quint32 begin, quint32 size)
	{
		for (auto& peer : m_Peers)
		{
			if (peer->m_OutcomingRequestList.isEmpty())			
				continue;
			

			if (peer->m_OutcomingRequestList.contains(PiecePart(index,begin,size)))
				return true;
		}
		return false;
	}

	//void Torrent::AddToAvailabilityMap(quint32 availability, quint32 piece)
	//{
	//	
	//}

	void Torrent::RebuildPeerAvailabilityMap(PeerInfo* peerInfo)
	{
		peerInfo->m_PeerAvailability2Index.clear();

		for (quint32 c = 0; c < m_PieceCount; c++)
		{
			if (m_Pieces[c])
				continue;

			quint32 availability = m_Pieces2Availability[c];
			if (availability > 0)
			{
				if (peerInfo->HasPiece(c))
				{
					peerInfo->m_PeerAvailability2Index.insert(make_pair(availability, c));
				}
			}
		}
	}

	void Torrent::BuildRequestListForPeer(Peer* peer, qint64 requestSize)
	{
		//CHECK(!m_Availability2Pieces.empty());

		static std::mt19937 generator((unsigned int)time(NULL));

		while (0 < requestSize)
		{
			/////////////////////
			if (peer->m_PeerInfo->m_PeerAvailability2Index.empty())
				break;

			quint32 availability = peer->m_PeerInfo->m_PeerAvailability2Index.begin()->first;
			auto count = peer->m_PeerInfo->m_PeerAvailability2Index.count(availability);

			std::uniform_int_distribution<quint32> dist(0, count - 1);
			std::multimap<quint32, quint32>::iterator ret;


			quint32 index_offset = dist(generator);
			ret = peer->m_PeerInfo->m_PeerAvailability2Index.lower_bound(availability);
			std::advance(ret, index_offset);

			quint32 index = ret->second;
			quint32 length = PieceLengthAt(index);
			requestSize -= length;

			quint32 begin = 0;

			while (begin < length)
			{
				quint32 size = std::min(length - begin, PiecePartSize);
				peer->m_OutcomingRequestList << PiecePart(index, begin, size);
				begin += size;
			};




			peer->m_PeerInfo->m_PeerAvailability2Index.erase(ret);


			for (auto& peerInfo : m_PeerInfoList)
			{
				if (peer->m_PeerInfo == peerInfo)
				{
					continue;
				}

				if (peerInfo->HasPiece(index))
				{
					auto range = peerInfo->m_PeerAvailability2Index.equal_range(availability);
					auto ret = std::find_if(range.first, range.second, [availability, index](const pair<quint32, quint32> rhs)->bool
					{
						return availability == rhs.first && index == rhs.second;
					});

					peerInfo->m_PeerAvailability2Index.erase(ret);
				}
			}
			//////////
		};
	}

	
	bool Torrent::GetNextRandomRarestPiecePart(Peer* peer, PiecePart& piecePart)
	{
		static std::mt19937 generator((unsigned int)time(NULL));
		
		if (peer->m_PeerInfo->m_PeerAvailability2Index.empty())
			return false;

		quint32 availability = peer->m_PeerInfo->m_PeerAvailability2Index.begin()->first;
		auto count = peer->m_PeerInfo->m_PeerAvailability2Index.count(availability);

		std::uniform_int_distribution<quint32> dist(0, count - 1);
		std::multimap<quint32, quint32>::iterator ret;


		quint32 index_offset = dist(generator);
		ret = peer->m_PeerInfo->m_PeerAvailability2Index.lower_bound(availability);
		std::advance(ret, index_offset);

		quint32 index = ret->second;
		quint32 begin = m_PiecePartBeginForDownloading[index];
		quint32 length = PieceLengthAt(index);		
		
		quint32 size = std::min(length - begin, PiecePartSize);

		piecePart.Index = index;
		piecePart.Begin = begin;
		piecePart.Length = size;		
				
		piecePart.Length = size;
		m_PiecePartBeginForDownloading[index] += size;


		peer->m_PeerInfo->m_PeerAvailability2Index.erase(ret);

		for (auto& peerInfo : m_PeerInfoList)
		{
			if (peer->m_PeerInfo == peerInfo)
			{
				continue;
			}

			if (peerInfo->HasPiece(index))
			{
				auto range = peerInfo->m_PeerAvailability2Index.equal_range(availability);
				auto ret = std::find_if(range.first, range.second, [availability, index](const pair<quint32, quint32> rhs)->bool
				{
					return availability == rhs.first && index == rhs.second;
				});

				peerInfo->m_PeerAvailability2Index.erase(ret);
			}
		}
		
		return true;		
	}
	


	void Torrent::OnChooseAlgorithm()
	{		
	///////////
		int numPeerInfos = m_PeerInfoList.size();

		if (numPeerInfos >= MaxConnectionsPerTorrent)
		{
			for (auto& peerInfo : m_PeerInfoList)
			{
				for (quint32 c_piece = 0; c_piece < m_PieceCount; c_piece++)
				{
					if (peerInfo->m_PeerPieces[c_piece])
					{
						peerInfo->m_AvailabilityAmount += m_Pieces2Availability[c_piece];
					}
				}
			}
			
			sort(m_PeerInfoList.begin(), m_PeerInfoList.end(), [](const QSharedPointer<PeerInfo>& lhs, const QSharedPointer<PeerInfo>& rhs)->bool
			{
				return lhs->m_AvailabilityAmount < rhs->m_AvailabilityAmount;
			});
		}

		int newNumPeerInfos = std::min(numPeerInfos, MaxConnectionsPerTorrent);

		for (auto it = m_Peers.begin(); it != m_Peers.end();)
		{
			auto ret = find_if(m_PeerInfoList.begin(), m_PeerInfoList.begin() + newNumPeerInfos, [&](const QSharedPointer<PeerInfo>& lhs)->bool
			{
				return (*it)->m_PeerInfo == lhs;
			});

			if (ret != m_PeerInfoList.end())
			{
				++it;
			}
			else
			{
				it = m_Peers.erase(it);
			}
		}

		for (int c = 0; c < newNumPeerInfos; c++)
		{
			QSharedPointer<PeerInfo>& peerInfo = m_PeerInfoList[c];
			auto ret = find_if(m_Peers.begin(), m_Peers.end(), [peerInfo](const QSharedPointer<Peer>& lhs)->bool
			{
				return peerInfo == lhs->m_PeerInfo;
			});

			if (ret == m_Peers.end())
			{
				QSharedPointer<Peer> peer(new Peer(this, peerInfo));

				connect(peer.data(),SIGNAL(NeedRemove()), SLOT(OnNeedRemove()), Qt::QueuedConnection);
				connect(peer.data(),SIGNAL(ChooseAlgorithm()), SLOT(OnChooseAlgorithm()), Qt::QueuedConnection);			

				m_Peers.push_back(peer);
			}
		}

		for (auto& peer : m_Peers)
		{
			RebuildPeerAvailabilityMap(peer->m_PeerInfo.data());
		}

	/*	for (auto& peer : m_Peers)
		{
			if (peer->m_RequestSizePower > 0)
			{
				BuildRequestListForPeer(peer.data(), peer->m_RequestSizePower);				
			}			
		}*/


		QList<QSharedPointer<Peer>> tmpPeers = m_Peers;
		for (auto& peer : m_Peers)
		{
			if (peer->m_RequestSizePower)
				tmpPeers << peer;
		}


		while (!tmpPeers.empty())
		{
			for (auto it = tmpPeers.begin(); it != tmpPeers.end(); )
			{
				Peer* peer = it->data();

				PiecePart piecePart;
				bool result = GetNextRandomRarestPiecePart(peer, piecePart);				

				if (result)
				{
					peer->m_OutcomingRequestList << piecePart;					

					if (peer->m_RequestSizePower <= piecePart.Length)
					{
						peer->m_RequestSizePower = 0;
						it = tmpPeers.erase(it);
					}
					else
					{
						peer->m_RequestSizePower -= piecePart.Length;
						++it;
					}

				}
				else
				{
					it = tmpPeers.erase(it);
				}
			}
		}


		for (auto& peer : m_Peers)
		{				
			peer->Download();		
		}

		//QObject* object = sender();
		//if (object)
		//{
		//	Peer* initiator = qobject_cast<Peer*>(object);
		//	if (initiator)
		//		initiator->Download();
		//}
	}

	



	/*Torrent::~Torrent()
	{
		Stop();
	}*/

}
