#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QSharedPointer>
#include <QTcpServer>
#include <QHostAddress>
#include <QList>
#include <QByteArray>
#include <QTimer>

namespace torrent
{	
	class Torrent;
	class Peer;

	class Server : public QObject
	{
		Q_OBJECT
	public:
		enum class State
		{
			SS_STOPPED,
			SS_RUNNING,
		};

		Server::State GetState() const;
		QString GetStateString() const;

		void Start();		
		void Stop();
		bool isListening();
		QSharedPointer<Torrent> CreateNewTorrent(const QString& torrentFile);
		QSharedPointer<Torrent> CreateNewTorrent(const QByteArray& torrentData);
		QHostAddress GetLocalAddress();
		quint16 GetPort();
		QList< QSharedPointer<Torrent> >& GetTorrents();
        int GetIncomingPeerNum();
		QByteArray GetPeerId();

		Server(quint16 port = 0);
		~Server();
	protected:
		friend class TorrentLoop;
		void Process(quint32 deltaTime);		
	private:
		State									m_State;
		QSharedPointer<QTcpServer>				m_TcpServer;
		quint16									m_Port;
		QList< QSharedPointer<Torrent> >		m_Torrents;	
		QList< QSharedPointer<Peer> >			m_NewPeers;
		QByteArray								m_PeerId;
		QTimer									m_SpeedTimer;
		QSharedPointer<Torrent> FindClientByInfohash(const QByteArray& infohash);
		static 	QByteArray GeneratePeerId();	
	private slots:
		void OnNewConnection();
		void OnInfohashRecived();
		void OnTimeout();
	};

}

#endif // TORRENTSERVER_H
