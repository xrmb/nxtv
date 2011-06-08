#include "stdafx.h"
#include "nxlfs.h"



NXLFSFileBuild::NXLFSFileBuild(unsigned int pos, unsigned int steps, NXLFSFile* next)
: NXLFSFile(L"!queued", ::time(NULL), next),
  m_pos(pos),
  m_steps(steps < 1 ? 1 : steps),
  m_substeps(0)
{
}



NXLFSFileBuild::~NXLFSFileBuild()
{
}



int NXLFSFileBuild::open()
{
  return 0;
}



int NXLFSFileBuild::read(void* buffer, size_t blen, size_t* rlen, __int64 offset, int readahead, NXLFSFile* rahint)
{
  return 0;
}



int NXLFSFileBuild::close()
{
  return 0;
}



int NXLFSFileBuild::setStep(unsigned int step, unsigned int substeps)
{
  m_step = step < m_steps ? step : (m_step - 1);
  m_substeps = substeps < 1 ? 1 : substeps;

  return 0;
}



int NXLFSFileBuild::setSubstep(unsigned int substep)
{
  wchar_t n[MAX_PATH];
  NXLFS* nxlfs = NXLFS::i();
  if(!nxlfs) return R(-1, "no nxlfs");
  unsigned int q = nxlfs->queuePos();
  if(q > m_pos)
  {
    if(wsprintf(n, L"!queued %d", q - m_pos) == -1) return R(-2, "buffer error");
  }
  else
  {
    if(wsprintf(n, L"!processing %.0f%%", 100.0*m_step/m_steps + (100.0/m_steps)*(substep/m_substeps)) == -1) return R(-3, "buffer error");
  }
  rename(n);

  return 0;
}
