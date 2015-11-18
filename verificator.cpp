#include "verificator.h"
#include "torrent.h"
#include "filecache.h"
#include "exception.h"

namespace torrent
{


	void Verificator::StartVerification(Torrent* torrent)
	{
		m_Mutex.lock();
		m_Torrents.enqueue(torrent);
		m_Mutex.unlock();
		m_WaitCondition.wakeOne();
	}

	Verificator* Verificator::Instance()
	{
		static Verificator verificator;
		return &verificator;
	}

	void Verificator::run()
	{
		while (!m_Quit)
		{
			Torrent* torrent = nullptr;

			m_Mutex.lock();

			if (m_Torrents.empty())
			{
				if (m_WaitCondition.wait(&m_Mutex, 500))
				{
					CHECK(!m_Torrents.empty());
					torrent = m_Torrents.dequeue();
				}
			}
			else
			{
				torrent = m_Torrents.dequeue();
			}

			m_Mutex.unlock();

			if (!torrent)
				continue;


			QBitArray pieces(torrent->m_PieceCount);

			for (quint32 c_piece = 0, size_piece = torrent->m_PieceCount; c_piece < size_piece; )
			{
				const QString file = torrent->FileAt(c_piece);
				if (FileCache::instance()->IsFileExist(file))
				{
					if (torrent->CheckPieceSha(c_piece))
						pieces.setBit(c_piece);		
					c_piece++;
				}
				else
				{
					c_piece = torrent->LastPieceAtFile(file) + 1;
				}				
			}

			emit VerifyFinished(torrent, pieces);
		}
	}

	Verificator::Verificator() : QThread(), m_Quit(false)
	{
		start();
	}

}
