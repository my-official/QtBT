#ifndef FILECACHE_H
#define FILECACHE_H

#include <QFile>
#include <QVector>
#include <QMap>
#include <QByteArray>
#include <QSharedPointer>

namespace torrent
{
	class FileCache
	{
	public:
		static FileCache* instance();		
		bool ReadFromFile(QByteArray& data, const QString& filename, quint64 offset, quint64 length);
		bool WriteToFile(const QString& filename, quint64 offset, quint64 length, quint8* data);
		void CloseFile(const QString& file);
		void Flush();
		quint64 Size();		
		bool IsFileExist(const QString& filename);
	private:
		struct CachedPart
		{
			bool NeedFlush;
			quint64 Offset;
			QByteArray Data;
		};

		class CachedFile
		{
		public:
			QSharedPointer<QFile> File;
			QVector< CachedPart > DataParts;
			void Flush();
			CachedFile();
			~CachedFile();
		
			//QFile& operator = (const QFile& );			
			//bool operator == (const CachedFile& rhs) const;
		};
		QMap<QString, CachedFile> m_Cache;		
		
		
		FileCache();
		~FileCache();
		
	};

}

#endif