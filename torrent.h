#ifndef TORRENT_H
#define TORRENT_H

#include <QObject>
#include "metainfo.h"

#include <QList>
#include <QString>
#include <QUrl>
#include <QHostAddress>
#include <QBitArray>
#include "tracker.h"
#include "peer.h"

#include <unordered_map> 
#include <map> 
#include <vector> 
#include <random>
#include <time.h>


namespace torrent
{

	class Torrent :  public QObject, public MetaInfo
	{
		Q_OBJECT	
	public:
		enum class State
		{
			TS_STOPPED,
			TS_PREPARING,
			TS_DOWNLOADING,
			TS_SEEDING,
			TS_PAUSED
		};

		Torrent::State GetState() const;
		QString GetStateString() const;
		void SetDestDir(const QString& destDir);

		void AddTrackerUrl(const QUrl& url);		
		bool AddPeerInfo(const QHostAddress& address, quint16 port);
		QByteArray GetInfoHash() const;		
		quint64 GetUploadedBytesPerSession() const;
		quint64 GetDownloadedBytesPerSession() const;
		quint64 GetLeftBytes() const;
		quint64 GetTotalBytes() const;

		quint64 GetDownloadingSpeed() const;
		quint64 GetUploadingSpeed() const;
		quint32 GetActivePeerCount() const;
		quint32 GetPeerCount() const;

		QList< QSharedPointer<PeerInfo> > GetPeers() const;
		QList< QSharedPointer<PeerInfo> > GetActivePeers() const;

		void Start(bool skipPreparing = false);		
		void Stop();
		void SetPause(bool pause);
		bool Pause();
		
		//virtual ~Torrent();


	protected:
		friend class Server;
		friend class Peer;
		friend class Verificator;

		using MetaInfo::clear;
		using MetaInfo::parse;
		using MetaInfo::errorString;

		bool AddPeerInfo(const QSharedPointer<PeerInfo>& peerInfo);
		void AddIncomingPeer(const QSharedPointer<Peer>& peer);
		
		std::multimap<quint32, PiecePart>		m_DownloadedParts;
		void WritePiecePart(quint32 index, quint32 begin, quint32 length, quint8* data);
		void ReadPiecePart(quint32 index, quint32 begin, quint32 length, quint8* data);


		void Prepare();		
		void StartDownloading();
		void StartSeeding();

		Torrent(Server* server, const MetaInfo& metainfo);
	private:
		State m_State;
		bool m_Pause;
		Server* m_Server;
		QString m_DestDir;
		
		QByteArray		m_InfoHash;
		const quint32	m_PieceCount;

		QBitArray	m_Pieces;		
		quint32		m_CheckingPiece;
		quint32		m_CheckingPeerTimer;		

		QList< QSharedPointer<Tracker> > m_Trackers;
		QList< QSharedPointer<PeerInfo> > m_PeerInfoList;
		QList< QSharedPointer<Peer> > m_Peers;
				
		QByteArray Read(quint64 pieceIndex, quint64 begin, quint64 length) const;
		void Write(quint64 pieceIndex, quint64 begin, quint64 length, quint8* write) const;
				
		bool CheckPieceSha(quint32 pieceIndex) const;		

		quint32 PieceCount() const;
        int BitFieldByteSize() const;
		quint32 PieceLengthAt(quint32 pieceIndex) const;
		quint64 PieceOffset(quint32 pieceIndex) const;
		
		bool HasPeerInfo(const PeerInfo& peerInfo) const;	
		//bool HasPeerInfo(const Peer& peer) const;	
		//bool HasPeerConnection(const Peer& peer) const;		
		bool HasPeerConnection(const PeerInfo& peerInfo) const;	
		void SeedingAlgorithm();
		//void DownloadingAlgorithm();		

		void InitializePeer(Peer& peer);
		QString FileAt(quint32 piece);
		quint32 LastPieceAtFile(const QString& filename);

		void UpdateSpeeds(quint32 deltaTime);
	private slots:
		//By Peer	
		void OnInfohashRecived(const QByteArray& infohash);
		void OnPeerIdRecived();	
		void OnDataTransferReady();
		void OnNeedRemove();
		void OnChooseAlgorithm();

		//By Tracker
		void OnNewPeerInfo(const QSharedPointer<PeerInfo>& newPeerInfo);
		//By FileVerificator
		void OnVerifyFinished(Torrent* torrent, QBitArray	pieces);

		////////////////////////////?////////////////////////////////////////////////////////////
	private:
		std::vector< quint32 >	m_Pieces2Availability;
		//std::multimap<quint32, quint32 >		m_Availability2Pieces;		
		QVector<quint32>	m_PiecePartBeginForDownloading;

		void BuildRequestListForPeer(Peer* peer, qint64 requestSize);
		void OnReciveHave(Peer* peer, quint32 index);
		void OnReciveBitField(Peer* peer, const QBitArray& pieces);
		//void RebuildAvailabilityMap();
		bool IsPartDownloaded(quint32 index, quint32 begin, quint32 length);
		bool IsPartRequested(int index, quint32 begin, quint32 size);
		//void AddToAvailabilityMap(quint32 availability, quint32 piece);
		void RebuildPeerAvailabilityMap(PeerInfo* peerInfo);
		bool GetNextRandomRarestPiecePart(Peer* peer, PiecePart& piecePart);
	};

}
#endif // TORRENT_H
