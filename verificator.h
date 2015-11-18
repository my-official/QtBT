#ifndef VERIFICATOR_H
#define VERIFICATOR_H

#include <QThread>
#include <QWaitCondition>
#include <QMutex>
#include <QQueue>
#include <QSharedPointer>
#include <QTimer>
#include <QBitArray>

namespace torrent
{
	class Torrent;

	class Verificator :  public QThread
	{
		Q_OBJECT
	public:
		void StartVerification(Torrent* torrent);
		static Verificator* Instance();
	signals:
		void VerifyFinished(Torrent* torrent, QBitArray pieces);
	protected:		
		QWaitCondition m_WaitCondition;
		QMutex m_Mutex;
		QQueue< Torrent* >  m_Torrents;
		bool m_Quit;

		virtual void run() override;
		Verificator();
	};

}

#endif 