//################
//# qdebugstream.h  #
//################

#ifndef Q_DEBUG_STREAM_H
#define Q_DEBUG_STREAM_H

#include "mc.h"

#include <iostream>
#include <streambuf>
#include <string>

#include <QtWidgets/QTextEdit>

class QDebugStream : public std::basic_streambuf<char>
{
public:
 QDebugStream(std::ostream &stream, QtClient* wnd) : m_stream(stream)
 {
  log_window = wnd;
  m_old_buf = stream.rdbuf();
  stream.rdbuf(this);
 }
 ~QDebugStream()
 {
  // output anything that is left
  if (!m_string.empty())
   log_window->logOutput(m_string.c_str());

  m_stream.rdbuf(m_old_buf);
 }

protected:
 virtual int_type overflow(int_type v)
 {
  if (v == '\n')
  {
   log_window->logOutput(m_string.c_str());
   //log_window->setHtml(log_window->toHtml() + m_string.c_str() + "\n");
#ifdef MC_OS_WIN
   std::string tmp(m_string);
   tmp.append("\n");
   OutputDebugStringA(tmp.c_str());
#endif
   m_string.erase(m_string.begin(), m_string.end());
  }
  else
   m_string += v;

  return v;
 }

 virtual std::streamsize xsputn(const char *p, std::streamsize n)
 {
  m_string.append(p, p + n);

  size_t pos = 0;
  while (pos != std::string::npos)
  {
   pos = m_string.find('\n');
   if (pos != std::string::npos)
   {
	std::string tmp(m_string.begin(), m_string.begin() + pos);
	log_window->logOutput(tmp.c_str());
	//log_window->setHtml(log_window->toHtml() + tmp.c_str() + "\n");
#ifdef MC_OS_WIN
	tmp.append("\n");
	OutputDebugStringA(tmp.c_str());
#endif
	m_string.erase(m_string.begin(), m_string.begin() + pos + 1);
   }
  }

  return n;
 }

private:
 std::ostream &m_stream;
 std::streambuf *m_old_buf;
 std::string m_string;
 QtClient* log_window;
};

#endif