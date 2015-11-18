#ifndef TORRENTLOOP_H
#define TORRENTLOOP_H

#include <QObject>
#include <QSharedPointer>
#include <QEvent>
#include <QVector>
#include <QElapsedTimer>


class QSocket;

namespace torrent
{
	class Server;

	class TorrentLoop : public QObject
	{
		Q_OBJECT
	public:
		void AddServer(const QSharedPointer<Server>& server);
		void RemoveServer(const QSharedPointer<Server>& server);		
		QVector< QSharedPointer<Server> > GetServers();
		static TorrentLoop* instance();
		TorrentLoop(QObject* owner = NULL);
		~TorrentLoop();


		//dispatcher:

		/*void AddSocket(const QSharedPointer<QSocket>& pointer);
		void RemoveSocket(const QSharedPointer<QSocket>& pointer);

		void AddTimer(const QSharedPointer<QTimer>& pointer);
		void RemoveTimer(const QSharedPointer<QTimer>& pointer);

		void AddRequest(const QSharedPointer<QNetworkRequest>& pointer);
		void RemoveRequest(const QSharedPointer<QNetworkRequest>& pointer);

		void AddDiskNotificator(const QSharedPointer<QNetworkRequest>& pointer);
		void RemoveDiskNotificator(const QSharedPointer<QNetworkRequest>& pointer);*/
	protected:		
		bool event(QEvent* e)	Q_DECL_OVERRIDE;
	private:		
		int m_EventId;
		QVector< QSharedPointer<Server> > m_Servers;
		QElapsedTimer	m_Time;
	};


	class TorrentLoopEvent : public QEvent
	{
		Q_GADGET
	public:
		TorrentLoopEvent(Type typeId) : QEvent(typeId)
		{
		}
	};


}

#endif