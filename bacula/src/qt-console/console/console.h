
#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <QtGui>

class DIRRES;
class BSOCK;
class JCR;
class CONRES;

class Console : public QWidget
{
   Q_OBJECT 
public:
   Console();
   void set_text(const char *buf);
   void set_textf(const char *fmt, ...);
   void set_statusf(const char *fmt, ...);
   void set_status_ready();
   void set_status(const char *buf);

public slots:
   void connect();

private:
   QTextEdit *m_textEdit;
   DIRRES *m_dir;
   BSOCK *m_sock;   
};

extern int authenticate_director(JCR *jcr, DIRRES *director, CONRES *cons);

#endif /* _CONSOLE_H_ */
