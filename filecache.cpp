#include "filecache.h"
#include <QFileInfo>
#include <QDir>
#include "protocol.h"
#include "exception.h"

namespace torrent
{
	//template <typename TYPE>
	//inline static bool BufferIntersection(TYPE& offsetI, TYPE& lengthI,TYPE offset0, TYPE length0, TYPE offset1, TYPE length1)
	//{
	//	if (offset0 < offset1)
	//	{
	//		if (offset0 + length0 < offset1)
	//		{
	//			return false;
	//		}
	//		else
	//		{				
	//			offsetI = offset0;
	//			lengthI = offset1 - offset0 + length1;
	//			return true;
	//		}
	//	}
	//	else
	//	{
	//		if (offset0 > offset1 + length1)
	//		{
	//			return false;
	//		}
	//		else
	//		{
	//			offsetI = offset1;
	//			lengthI = offset0 - offset1 + length0;
	//			return true;
	//		}
	//	}
	//}

	void FileCache::Flush()
	{
		auto keys = m_Cache.keys();
		for (auto& key : keys)
		{
			m_Cache[key].Flush();
		}
	}

	quint64 FileCache::Size()
	{
		quint64 result = 0;		
		for (auto& key : m_Cache.keys())
		{
			auto& DataParts = m_Cache[key].DataParts;
			for (auto& part : DataParts)
			{				
				result += part.Data.size();
			}
		}
		return result;
	}

	FileCache::FileCache()
	{
	}


	FileCache::~FileCache()
	{
	}

	FileCache* torrent::FileCache::instance()
	{
		static FileCache* cache = new FileCache;
		return cache;
	}

	bool FileCache::ReadFromFile(QByteArray& data, const QString& filename, quint64 offset, quint64 length)
	{	
		if (!m_Cache.contains(filename))		
		{
			CachedFile newCachedFile;		
			
			if (!QFile::exists(filename))
				return false;

			newCachedFile.File.reset(new QFile(filename));

			if (!newCachedFile.File->open(QIODevice::ReadWrite))
				return false;
			
			m_Cache.insert(filename,newCachedFile);
		}

		CachedFile& cachedFile = m_Cache[filename];

		if (length == 0)
			return true;

		quint64 fileSize = cachedFile.File->size();

		if ( (offset >= fileSize) || (offset + length > fileSize) )
			return true;
				
		if (!cachedFile.File->seek(offset))
			return false;

		data = cachedFile.File->read(length);

		if (data.size() != (int)length)
			return false;
		
		return true;
		
		
		//QByteArray result(length,0);

		//
		//bool hasInCache = false;


		//for (auto part = cachedFile.DataParts.begin(); part != cachedFile.DataParts.end(); ++part)
		//{
		//	quint64 new_cached_offset = 0;
		//	quint64 new_cached_length = 0;


		//	if (BufferIntersection(new_cached_offset,new_cached_length, offset, length, part.Offset, (quint64)part.Data.size()))
		//	{		
		//		


		//		if (!cachedFile.File->seek(new_cached_offset))
		//			return QByteArray();
		//		result = cachedFile.File->read(new_cached_length);

		//		hasInCache = true;
		//	}	
		//}


		//if (!hasInCache)
		//{
		//	if (!cachedFile.File->seek(offset))
		//		return QByteArray();
		//	result = cachedFile.File->read(length);

		//	CachedPart part;
		//	part.NeedFlush = false;
		//	part.Offset = offset;
		//	part.Data = result;						

		//	cachedFile.DataParts.append(part);
		//}	

		//return result;
	}

	bool FileCache::WriteToFile(const QString& filename, quint64 offset, quint64 length, quint8* data)
	{
		if (!m_Cache.contains(filename))		
		{
			CachedFile newCachedFile;
			newCachedFile.File.reset(new QFile(filename));

			if (!QDir().mkpath(QFileInfo(filename).path()))
				return false;

			if (!newCachedFile.File->open(QIODevice::ReadWrite))
				return false;
			
			m_Cache.insert(filename,newCachedFile);
		}

		CachedFile& cachedFile = m_Cache[filename];

		if (length == 0)
			return true;

		quint64 fileSize = cachedFile.File->size();
		

		if ( (offset >= fileSize) || (offset + length > fileSize) )
		{
			if (!cachedFile.File->resize(offset + length))
				return false;
		}
				
		if (!cachedFile.File->seek(offset))
			return false;
		

		if (cachedFile.File->write((char*)data,length) != (qint64)length)
			return false;
		
		return true;
	}

	void FileCache::CloseFile(const QString& file)
	{
		if (!m_Cache.contains(file))		
		{
			m_Cache.remove(file);
		}
	}

	bool FileCache::IsFileExist(const QString& filename)
	{
		return QFile::exists(filename);
	}

	void FileCache::CachedFile::Flush()
	{
		for (auto& part : DataParts)
		{
			if (part.NeedFlush)
			{
				File->seek(part.Offset);
				File->write(part.Data);
			}
		}
		File->flush();
	}
	
	FileCache::CachedFile::CachedFile() : File()
	{

	}

	FileCache::CachedFile::~CachedFile()
	{	
		if (!File.isNull())
			File->flush();
	}



	//bool FileCache::CachedFile::operator==(const CachedFile& rhs) const
	//{
	//	return File->fileName() == rhs.File->fileName();
	//}

}
