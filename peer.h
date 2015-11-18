#ifndef PEER_H
#define PEER_H

#include <QObject>
#include <QSharedPointer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QBitArray>
#include <QQueue>
#include <QElapsedTimer>
#include <QTimer>
#include <QSet>

#include <unordered_map> 
#include <map> 
#include <vector> 

#include <random>
#include <time.h>

namespace torrent
{
	struct PiecePart
	{
		quint32		Index;
		quint32		Begin;
		quint32		Length;
		bool operator==(const PiecePart& rhs) const;
		PiecePart(quint32 index, quint32 begin, quint32 length);
		PiecePart();
	};

	struct DataPiece : public PiecePart
	{
		QByteArray	Data;
		DataPiece(quint32 index, quint32 begin, quint32 length, quint8* data);
		/*DataPiece();
		DataPiece& operator=(const DataPiece& rhs);*/
	};

	class Peer;

	class PeerInfo
	{
	public:		
		PeerInfo(const QHostAddress& address, quint16 port, const QString& peerId = "");
		bool operator == (const PeerInfo& rhs) const;		

		quint64 GetDownloadingSpeed() const;
		quint64 GetUploadingSpeed() const;

		quint64 GetDownloadedBytes() const;
		quint64 GetUploadedBytes() const;

		QHostAddress GetHostAddress() const;
		quint16 GetPort() const;
		QString GetPeerId() const;

		bool HasPiece(quint32 index);
	protected:
		friend class Torrent;
		friend class Peer;
		QHostAddress	m_Address;
		quint16			m_Port;
		QString			m_PeerId;		
		QString			m_TrackedPeerId;

		QBitArray		m_PeerPieces;
	//	QSet<quint32>	m_PieceIndices;
		std::multimap<quint32, quint32> m_PeerAvailability2Index;
		quint64			m_AvailabilityAmount;

		quint64			m_DownloadedBytes;
		quint64			m_UploadedBytes;

		quint64			m_DownloadingSpeed;
		quint64			m_UploadingSpeed;

		
	};

	class Torrent;

	class Peer : public QObject
	{
		Q_OBJECT
	public:
		enum class PeerState
		{	
			PS_NULL,
			PS_CONNECTED,
			PS_HANDSHAKED,
			PS_RECV_ID,
			PS_RECVING,			
		};
		PeerState GetState();

		bool AmChoking();//€ заблокировал пира?
		bool AmInterested();//€ заинтересован в этом пире?

		bool PeerChoking();//пир заблокировал мен€?
		bool PeerInterested();//€ интересен этому пиру?


		void Choke();//«аблокировать
		void Unchoke();//–азаблокировать
		void Interesting();//Ќачать интересоватьс€ пиром
		void NotInteresting();//ѕерестать интересоватьс€ пиром		

		void RequestPiece(quint32 index, quint32 begin, quint32 length);
		void CancelPiece(quint32 index, quint32 begin, quint32 length);
		void UploadPiece(quint32 index, quint32 begin, quint32 length);
		void Have(quint32 index);

		bool operator == (const PeerInfo& rhs) const;
		bool operator == (const Peer& rhs) const;	

	signals:
		void InfohashRecived(const QByteArray& infohash);
		void PeerIdRecived();
		void DataTransferReady();
		void NeedRemove();

		void ChooseAlgorithm();
	protected:
		friend class Server;
		friend class Torrent;		
		PeerState					m_State;
		Torrent*					m_Torrent;
		QSharedPointer<PeerInfo>	m_PeerInfo;
        bool						m_AmChoking;
        bool						m_AmInterested;
        bool						m_PeerChoking;
        bool						m_PeerInterested;        


		
		QByteArray						m_Infohash;
		QList<PiecePart>				m_UploadingPieceParts;
		QList<PiecePart>				m_IncomingRequestList;
		QList<PiecePart>				m_OutcomingRequestList;		
		quint32							m_OutcomingRequestIndex;
		quint32							m_OutcomingRequestBegin;

		
		//BitTorrent Protocol senders
		bool SendHandshake(const QByteArray& infohash);
		bool SendPeerID();
		bool SendKeepAlive();		
		bool SendChoke();
		bool SendUnchoke();
		bool SendInterested();
		bool SendNotInterested();
		bool SendHave(quint32 index);
		bool SendBitField(const QBitArray& bitArray);		
		bool SendRequest(quint32 index, quint32 begin, quint32 length);
		bool SendPiece(quint32 index, quint32 begin, quint32 length);
		bool SendCancel(quint32 index, quint32 begin, quint32 length);

		//BitTorrent Protocol recivers
		bool ReciveHandshake();
		bool RecivePeerID();
		
		void ReciveChoke();
		void ReciveUnchoke();
		void ReciveInterested();
		void ReciveNotInterested();
		bool ReciveHave(quint32 index);
		bool ReciveBitField(const QBitArray& pieces);
		bool ReciveRequest(quint32 index, quint32 begin, quint32 length);
		bool RecivePiece(quint32 index, quint32 begin, quint32 length, quint8* data);
		bool ReciveCancel(quint32 index, quint32 begin, quint32 length);


		void SetupConnection();
		bool RecivePackets();
		bool SendPackets();		
		bool TickTimers(quint32 deltaTime);
		//bool Process(quint32 deltaTime);
		
		Peer(Torrent* torrent, QSharedPointer<PeerInfo> peerInfo);
		Peer(QTcpSocket* socket);		
	private:
		QSharedPointer<QTcpSocket>	m_Socket;
        int 						m_NextPacketLength;
		//quint32						m_LocalKeepAliveTimer;
		//quint32						m_RemoteKeepAliveTimer;
		QTimer						m_LocalKeepAliveTimer;
		QTimer						m_RemoteKeepAliveTimer;

		quint32						m_SpeedTimer;
		quint64						m_DownloadedBytes;
		quint64						m_UploadedBytes;

		quint64						m_PrevDownloadedBytes;
		quint64						m_PrevUploadedBytes;

		bool						m_SentHandshake;


		void ResetTimers();
		void UpdateSpeeds(quint32 deltaTime);
		void Download();
	private slots:
		void OnReadReady();
		void OnTimeout();		
		void OnError(QAbstractSocket::SocketError error);
		void OnConnected();
		


				////////////////////////////?////////////////////////////////////////////////////////////
	private:
		quint32 m_RequestSizePower;
		quint32 m_MaxRequestSizePower;
		qint32 m_DownloadingDeltaSpeed;
	};

}

#endif
