#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <QException>
#include <QString>

namespace torrent
{

//#define Error(msg)		throw Exception(__FUNCTION__, __FILE__, __LINE__, msg)
#define THROW			throw Exception(__FUNCTION__, __FILE__, __LINE__)
#define CHECK(cond)		if (!(cond))	throw Exception(__FUNCTION__, __FILE__, __LINE__)
//#define Error	++m_Error; throw Exception(__FUNCTION__, __FILE__, __LINE__)



	class Exception : public QException
	{
		/*Q_OBJECT*/
	public:
		QString m_Func;
		QString m_File;
		quint64 m_Line;
		QString m_Message;
		inline Exception(const QString& func, const QString& file, quint64 line, const QString& message = "") : QException(), m_Func(func), m_File(file), m_Line(line), m_Message(message)
		{
		}
	};

}
#endif // EXCEPTION_H
