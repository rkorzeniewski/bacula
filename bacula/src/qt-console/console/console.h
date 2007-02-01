
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
   void set_text(const QString buf);
   void set_textf(const char *fmt, ...);
   void update_cursor(void);
   void write_dir(const char *buf);
   bool authenticate_director(JCR *jcr, DIRRES *director, CONRES *cons);
   bool is_connected() { return m_sock != NULL; };
   const QFont get_font();
   void writeSettings();
   void readSettings();

public slots:
   void connect(void);
   void read_dir(int fd);
   void status_dir(void);
   void set_font(void);

private:
   QTextEdit *m_textEdit;
   DIRRES *m_dir;
   BSOCK *m_sock;   
   bool m_at_prompt;
   QSocketNotifier *m_notifier;
   QTextCursor *m_cursor;
   QTreeWidgetItem *m_consoleItem;
};

#endif /* _CONSOLE_H_ */
