#include "torrentloop.h"
#include <QCoreApplication>
#include <QThread>
#include "torrent.h"
#include "server.h"
#include "exception.h"
#include "protocol.h"
#include <QDateTime>

namespace torrent
{	
	//quint8 PeerId[20];


	void TorrentLoop::AddServer(const QSharedPointer<Server>& server)
	{		
		m_Servers.push_back(server);		
	}

	void TorrentLoop::RemoveServer(const QSharedPointer<Server>& server)
	{
		m_Servers.removeOne(server);
	}

	QVector< QSharedPointer<Server> > TorrentLoop::GetServers()
	{
		return m_Servers;
	}

	TorrentLoop* TorrentLoop::instance()
	{
		static TorrentLoop loop(NULL);
		return &loop;
	}

	TorrentLoop::TorrentLoop(QObject* owner) : QObject(owner), m_EventId(QEvent::registerEventType(QEvent::User))
	{		 
		QCoreApplication::postEvent(this,new TorrentLoopEvent((QEvent::Type)m_EventId));
		m_Time.start();
	}

	TorrentLoop::~TorrentLoop()
	{

	}

	bool TorrentLoop::event(QEvent* e)
	{
		if (!e)
			return false;

		const auto type = e->type();

		if (type != m_EventId && type != QEvent::Type::SockAct)
			return false;


		quint32 deltaTime = m_Time.elapsed();

		while (deltaTime == 0)
		{
			QThread::usleep(1);
			deltaTime = m_Time.elapsed();
		}

		m_Time.restart();

		for (auto& server : m_Servers)
		{
			try
			{
				if (!server->isListening())
					continue;
				server->Process(deltaTime);
			}
			catch (Exception& e)
			{
				qWarning("Server");
				qWarning("Exception! '%s'", qPrintable(e.m_Message));
				qWarning("'%s'", qPrintable(e.m_File + ":" + QString::number(e.m_Line)));
				qWarning("Function = %s", qPrintable(e.m_Func));
				m_Servers.removeOne(server);
			}
			catch (std::exception& e)
			{
				qWarning("Server");
				qWarning("std::exception! %s", e.what());
				m_Servers.removeOne(server);
			}
			catch (...)
			{
				qWarning("Server");
				qWarning("Unknown exception!");
				m_Servers.removeOne(server);
			}
		}
		
		QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 1000);

		if (type == m_EventId)
		{
			e->accept();
			QCoreApplication::postEvent(this,new TorrentLoopEvent((QEvent::Type)m_EventId));
			return true;
		}
		else
		{
			e->ignore();
			return false;
		}
	}
	






		/*void Thread::run()
	{
		try
		{	
			for (auto& server : m_Servers)
			{
				server.Start();
			}

			QEventLoop eventLoop(this);
			

			while (eventLoop.processEvents(QEventLoop::WaitForMoreEvents))
			{
				for (auto& server : m_Servers)
				{
					server.Process();
				}
			}

			for (auto& server : m_Servers)
			{
				server.Stop();
			}

		}
		catch (Exception& e)
		{
			qWarning("Exception! '%s'", qPrintable(e.m_Message));
			qWarning("'%s'", qPrintable(e.m_File + ":" + QString::number(e.m_Line)));
			qWarning("Function = %s", qPrintable(e.m_Func));
		}
		catch (std::exception& e)
		{
			qWarning("std::exception! %s", e.what());
		}
		catch (...)
		{
			qWarning("Unknown exception!");
		}
	}*/
	

}