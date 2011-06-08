#include "nxlfs.h"



void auto_close::close() 
{ 
  if(m_fh) fclose(m_fh); m_fh = NULL;
  if(m_fd) _close(m_fd); m_fd = 0; 
  if(m_ff) m_ff->close(); m_ff = NULL; 
}
