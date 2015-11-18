#include "server.h"

#include <QTcpSocket>
#include <QFile>
#include <QThread>
#include <QCoreApplication>

#include "torrent.h"
#include "peer.h"
#include "exception.h"
#include "metainfo.h"
#include "torrentloop.h"
#include "protocol.h"

namespace torrent
{
	void Server::Process(quint32 deltaTime)
	{
	/*	if (!m_TcpServer->isListening())
			THROW;

		for (auto& torrent : m_Torrents)
		{
			try
			{
				if (torrent->GetState() == Torrent::State::TS_STOPPED)
					continue;

				if (!torrent->Process(deltaTime))
					m_Torrents.removeOne(torrent);
			}
			catch (Exception& e)
			{
				qWarning("Torrent");
				qWarning("Exception! '%s'", qPrintable(e.m_Message));
				qWarning("'%s'", qPrintable(e.m_File + ":" + QString::number(e.m_Line)));
				qWarning("Function = %s", qPrintable(e.m_Func));
				m_Torrents.removeOne(torrent);
			}
			catch (std::exception& e)
			{
				qWarning("Torrent");
				qWarning("std::exception! %s", e.what());
				m_Torrents.removeOne(torrent);
			}
			catch (...)
			{
				qWarning("Torrent");
				qWarning("Unknown exception!");
				m_Torrents.removeOne(torrent);
			}
		}	

		while (m_TcpServer->hasPendingConnections())
		{
			QTcpSocket* socket = m_TcpServer->nextPendingConnection();			
			m_NewPeers.append(QSharedPointer<Peer>(new Peer(socket)));
		}

		for (auto peer : m_NewPeers)
		{
			try
			{
				if (peer->GetState() == Peer::PeerState::PS_HANDSHAKED)
				{
					QSharedPointer<Torrent> torrent = FindClientByInfohash(peer->m_Infohash);
					if (!torrent.isNull())
					{
						torrent->AddIncomingPeer(peer);
					}
					m_NewPeers.removeOne(peer);
					continue;
				}
				if (!peer->Process(deltaTime))
				{
					m_NewPeers.removeOne(peer);
				}
			}
			catch (Exception& e)
			{
				qWarning("Server peer");
				qWarning("Exception! '%s'", qPrintable(e.m_Message));
				qWarning("'%s'", qPrintable(e.m_File + ":" + QString::number(e.m_Line)));
				qWarning("Function = %s", qPrintable(e.m_Func));
				m_NewPeers.removeOne(peer);
			}
			catch (std::exception& e)
			{
				qWarning("Server peer");
				qWarning("std::exception! %s", e.what());
				m_NewPeers.removeOne(peer);
			}
			catch (...)
			{
				qWarning("Server peer");
				qWarning("Unknown exception!");
				m_NewPeers.removeOne(peer);
			}
		}*/
	}
	

	Server::State Server::GetState() const
	{
		return m_State;
	}

	QString Server::GetStateString() const
	{
		switch (m_State)
		{
			case torrent::Server::State::SS_STOPPED:
				return "Stopped";
			break;
			case torrent::Server::State::SS_RUNNING:
				return "Running";
			break;
			default:
				return "Unknown";
			break;
		}
	}

	void Server::Start()
	{	
		if (m_State == State::SS_RUNNING)
			return;

		while (!m_TcpServer->listen(QHostAddress::Any,m_Port++));			
		m_Port = m_TcpServer->serverPort();

		for (auto& torrent : m_Torrents)
		{
			torrent->Start();
		}
		
		m_SpeedTimer.start();

		m_State = State::SS_RUNNING;
	}

	void Server::Stop()
	{
		if (m_State == State::SS_STOPPED)
			return;

		m_SpeedTimer.stop();

		for (auto& torrent : m_Torrents)
		{
			torrent->Stop();
		}
		m_NewPeers.clear();

		m_TcpServer->close();
		m_Port = 0;
		
		m_State = State::SS_STOPPED;
	}

	bool Server::isListening()
	{
		return m_TcpServer->isListening();
	}

	QSharedPointer<Torrent> Server::CreateNewTorrent(const QString& torrentFile)
	{
		//if (!isListening())
		//	return QSharedPointer<Torrent>(nullptr);	

		QFile file(torrentFile);

		if (!file.open(QIODevice::ReadOnly))
			return QSharedPointer<Torrent>(nullptr);		
		
		return CreateNewTorrent(file.readAll());
	}

	QSharedPointer<Torrent> Server::CreateNewTorrent(const QByteArray& torrentData)
	{
		//if (!isListening())
		//	return QSharedPointer<Torrent>(nullptr);

		if (torrentData.isEmpty())
			return QSharedPointer<Torrent>(nullptr);

		MetaInfo metainfo;
		if (!metainfo.parse(torrentData))
			return QSharedPointer<Torrent>(nullptr);

		QSharedPointer<Torrent> result(new Torrent(this,metainfo));		
		
		m_Torrents.append(result);
		return result;
	}

	QHostAddress Server::GetLocalAddress()
	{
		return m_TcpServer->serverAddress();
	}

	quint16 Server::GetPort()
	{
		return m_Port;
	}

	
	QList< QSharedPointer<Torrent> >& Server::GetTorrents()
{
		return m_Torrents;
	}

    int Server::GetIncomingPeerNum()
	{
		return m_NewPeers.size();
	}

	QByteArray Server::GetPeerId()
	{
		return m_PeerId;
	}

	Server::Server(quint16 port) : m_State(State::SS_STOPPED), m_TcpServer(new QTcpServer), m_Port(port), m_PeerId(GeneratePeerId())
	{	
		connect(m_TcpServer.data(), SIGNAL(newConnection()), SLOT(OnNewConnection()));
		connect(&m_SpeedTimer, SIGNAL(timeout()), SLOT(OnTimeout()));
		m_SpeedTimer.setSingleShot(true);
		m_SpeedTimer.setInterval(SpeedRefreshInterval);
	}

	Server::~Server()
	{
		//if (isListening())
		//	Stop();		
	}

	QSharedPointer<Torrent> Server::FindClientByInfohash(const QByteArray& infohash)
	{
		for (auto& torrent : m_Torrents)
		{
			if (torrent->GetState() == Torrent::State::TS_STOPPED)
				continue;

			if (torrent->GetInfoHash() ==  infohash)
				return torrent;
		}
		return QSharedPointer<Torrent>(NULL);
	}

	QByteArray Server::GeneratePeerId()
	{		
		return (QString("SF-") + QString::number(QCoreApplication::applicationPid()).leftJustified(4,'0',true) + QDateTime::currentDateTime().toString("MMddhhmmsszzz")).toLatin1();	
	}

	void Server::OnNewConnection()
	{
		while (m_TcpServer->hasPendingConnections())
		{
			QTcpSocket* socket = m_TcpServer->nextPendingConnection();			
			m_NewPeers.append(QSharedPointer<Peer>(new Peer(socket)));
		}
	}

	void Server::OnInfohashRecived()
	{
		Peer* peer = qobject_cast<Peer*>(sender());
		CHECK(peer);
		CHECK(peer->GetState() == Peer::PeerState::PS_HANDSHAKED);


		QSharedPointer<Peer> peerPtr;


		for (auto& pointer : m_NewPeers)
		{
			if (pointer.data() == peer)
			{
				peerPtr = pointer;
				break;
			}
		}

		CHECK(peerPtr);

			
		QSharedPointer<Torrent> torrent = FindClientByInfohash(peer->m_Infohash);
		if (!torrent.isNull())
		{
			torrent->AddIncomingPeer(peerPtr);
		}
		m_NewPeers.removeOne(peerPtr);		
	}

	void Server::OnTimeout()
	{		
		CHECK(sender() == &m_SpeedTimer);

		

		for (auto& torrent : m_Torrents)
		{
			torrent->UpdateSpeeds(SpeedRefreshInterval);
		}
		m_SpeedTimer.start();
	}

}
