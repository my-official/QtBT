#ifndef TRACKER_H
#define TRACKER_H

#include <QObject>
#include <QUrl>
#include <QHostAddress>
#include <QSharedPointer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
//#include <QAuthenticator>

namespace torrent
{
	class Server;
	class Torrent;
	class PeerInfo;

	class Tracker :  public QObject
	{
		Q_OBJECT
	public:
		enum class State
		{
			TS_NULL,
			TS_WAITING,
			TS_SENT,
			TS_RECVED		
		};

		State GetState();
				
		QString GetTrackerId() const;
		QString GetTrackerMessage() const;		

		//bool operator == (const Tracker& rhs) const;


		void Start();
		void Stop();

		Tracker(Server* server, Torrent* torrent, const QUrl& url);		

	signals:
		void NewPeerInfo(const QSharedPointer<PeerInfo>& peerInfo);
	protected:
		friend class Torrent;
		
		QList< QSharedPointer<PeerInfo> >			m_PeerList;

		enum class RequestEventType
		{
			Regular = 0,
			Start = 1,
			Stop = 2,
			Complete = 3,
		};

		void Request(RequestEventType eventType = RequestEventType::Regular);
		void Response();				
	private:
		State					m_State;
		Torrent*				m_Torrent;
		Server*					m_Server;
		QUrl					m_Url;
		QNetworkRequest			m_Request;
		QNetworkReply*			m_Reply;
		QSharedPointer<QNetworkAccessManager>	m_NetworkAccessManager;		
		
		int						m_Interval;
		QTimer					m_RemainingTime;
		QString					m_TrackerId;
		QString					m_TrackerMessage;
		bool					m_LastResult;		

		QString MakeRequestUrl(RequestEventType eventType);	

		bool AddPeerInfoUnique(const QHostAddress& address, quint16 port, const QString& peerId = "");
	private slots:		
		void OnTimeout();				
		void OnFinished();
		
	};

}

#endif 