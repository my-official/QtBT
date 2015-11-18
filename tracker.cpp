#include "tracker.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QEventLoop>

#include "exception.h"
#include "bencodeparser.h"
#include "protocol.h"
#include "peer.h"
#include "torrent.h"
#include "server.h"




namespace torrent
{	
	static QByteArray EncodeRFC1738(const QByteArray& infohash)
	{
		QByteArray result;
		for (int i = 0; i < infohash.size(); ++i)
		{
			auto byte = infohash[i];
            if (('0' <= byte && byte <= '9') ||  ('a' <= byte && byte <= 'z') || ('A' <= byte && byte <= 'Z') || ('.' == byte) || ('-' == byte) || ('_' == byte) || ('~' == byte))
			{
				result += byte;
			}
			else
			{
				result += '%';
				result += QByteArray::number(byte, 16).right(2).rightJustified(2, '0');
			}
		}
		return result;
	}

	Tracker::Tracker(Server* server, Torrent* torrent, const QUrl& url) : QObject(), m_State(State::TS_NULL), m_Torrent(torrent), m_Server(server), m_Url(url)/*, m_Request(url)*/, m_Reply(NULL), m_NetworkAccessManager(new QNetworkAccessManager(nullptr)), m_Interval(TrackerDefaultInterval), m_LastResult(false)
	{		
		connect(&m_RemainingTime,SIGNAL(timeout()), SLOT(OnTimeout()));		
	}


	Tracker::State Tracker::GetState()
	{
		return m_State;
	}
	
	void Tracker::Request(RequestEventType eventType)
	{						
		m_Request.setUrl(MakeRequestUrl(eventType));			
		m_Reply = m_NetworkAccessManager->get(m_Request);		
		connect(m_Reply, SIGNAL(finished()), SLOT(OnFinished()));
		

		
		m_LastResult = false;
		m_State = State::TS_SENT;
		
	}

	void Tracker::Response()
	{
		//	qWarning("State: %s", qPrintable(m_Reply->errorString()));

		

		QByteArray response = m_Reply->readAll();
		m_Reply->abort();
		m_Reply->deleteLater();
		m_Reply = nullptr;

		BencodeParser parser;
		if (!parser.parse(response))
		{
			qWarning("Error parsing bencode response from tracker: %s", qPrintable(parser.errorString()));			
			return;
		}

		QMap<QByteArray, QVariant> dict = parser.dictionary();

		if (dict.contains("failure reason"))
		{
			m_TrackerMessage = dict.value("failure reason").toString();			
			return;
		}

		if (dict.contains("warning message"))
			m_TrackerMessage = dict.value("warning message").toString();


		if (dict.contains("tracker id") && dict.value("tracker id").isValid() && !dict.value("tracker id").isNull())
		{
			m_TrackerId = dict.value("tracker id").toByteArray();
		}

		if (dict.contains("interval"))
		{
			int interval = dict.value("interval").toInt();
			if (interval > 0)
			{
				m_Interval = interval * 1000;
			}
		}

		if (dict.contains("min interval"))
		{
			int interval = dict.value("min interval").toInt();
			if (interval > 0)
			{
				m_Interval = interval * 1000;
			}
		}		
				


		if (dict.contains("peers"))
		{
			QVariant peerEntry = dict.value("peers");
			if (peerEntry.type() == QVariant::List)
			{
				QList<QVariant> peerTmp = peerEntry.toList();
				for (int i = 0; i < peerTmp.size(); ++i)
				{
					
					QMap<QByteArray, QVariant> peer = qvariant_cast<QMap<QByteArray, QVariant>>(peerTmp.at(i));

					QString peerId;
					QHostAddress address;
					quint16 port;
					
					peerId = QString::fromUtf8(peer.value("peer id").toByteArray());
					address.setAddress(QString::fromUtf8(peer.value("ip").toByteArray()));
					port = peer.value("port").toInt();

					AddPeerInfoUnique(address,port,peerId);

					if (AddPeerInfoUnique(address,port,peerId))
					{
						emit NewPeerInfo(m_PeerList.back());						
					}

				}
			}
			else
			{
				static const quint32 SIZEOF_BINARYPEER = 6;

				QByteArray peerTmp = peerEntry.toByteArray();
				for (quint32 c = 0, size = peerTmp.size() / SIZEOF_BINARYPEER; c < size; c++)
				{					
					unsigned char* data = (unsigned char*)peerTmp.constData() + c * SIZEOF_BINARYPEER;
				
					QHostAddress address;
					quint16 port;

					address.setAddress(fromNetworkData(*(quint32*)data));
					port = (quint16(data[4]) << 8) + quint16(data[5]);

					if (AddPeerInfoUnique(address,port))
					{
						emit NewPeerInfo(m_PeerList.back());						
					}
				}
			}

			
		}		
	}

	QString Tracker::GetTrackerId() const
	{
		return m_TrackerId;		
	}

	QString Tracker::GetTrackerMessage() const
	{
		return m_TrackerMessage;
	}



	void Tracker::Start()
	{
		Request(RequestEventType::Start);		
	}

	void Tracker::Stop()
	{
		m_RemainingTime.stop();
		Request(RequestEventType::Stop);
	}

	//bool Tracker::operator==(const Tracker& rhs) const
	//{
	//	return m_Url == rhs.m_Url;
	//}

	QString Tracker::MakeRequestUrl(RequestEventType eventType)
{
		QString result = m_Url.toString();


		result += "?info_hash=" + EncodeRFC1738(m_Torrent->GetInfoHash()) + 
		"&peer_id=" + EncodeRFC1738(m_Server->GetPeerId()) +
		"&port=" + QByteArray::number(m_Server->GetPort()) +
		"&compact=1" +
		"&left=" + QString::number(m_Torrent->GetLeftBytes());

		if (!m_TrackerId.isEmpty())
			result += "&trackerid=" + m_TrackerId;

		switch (eventType)
		{
			case RequestEventType::Start:
			{
				result += "&uploaded=0&downloaded=0&event=started";
			}
			break;
			case RequestEventType::Stop:
			{
			   result += "&event=stopped&downloaded=" + QString::number(m_Torrent->GetDownloadedBytesPerSession()) + "&uploaded=" + QString::number(m_Torrent->GetUploadedBytesPerSession());
			}
			break;
			case RequestEventType::Complete:
			{
			   result += "&event=completed&downloaded=" + QString::number(m_Torrent->GetDownloadedBytesPerSession()) + "&uploaded=" + QString::number(m_Torrent->GetUploadedBytesPerSession());
			}
			break;
			case RequestEventType::Regular:
			{
				result += "&downloaded=" + QString::number(m_Torrent->GetDownloadedBytesPerSession()) + "&uploaded=" + QString::number(m_Torrent->GetUploadedBytesPerSession());
			}			
			break;
		}
		return result;
	}

	void Tracker::OnTimeout()
	{
		m_RemainingTime.stop();
		Request(RequestEventType::Regular);
	}


	void Tracker::OnFinished()
	{
		CHECK(m_Reply->isFinished());
		m_State = State::TS_RECVED;
		if (m_Reply->error() == QNetworkReply::NoError)
		{
			Response();
			m_RemainingTime.start(m_Interval);
		}
		else
		{
			qWarning("Tracker response error %s", qPrintable(m_Reply->errorString()));
			m_RemainingTime.start(TrackerDefaultInterval);
		}
	}

	bool Tracker::AddPeerInfoUnique(const QHostAddress& address, quint16 port, const QString& peerId /*= ""*/)
	{
		for (const auto& peer : m_PeerList)
		{
			if (peer->GetHostAddress() == address && peer->GetPort() == port)
			{
				return false;
			}
		}	
		
		QSharedPointer<PeerInfo> peerInfo(new PeerInfo(address, port, peerId));
		m_PeerList << peerInfo;
		return true;
	}

}
