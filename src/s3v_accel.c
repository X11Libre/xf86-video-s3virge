
/*
Copyright (C) 1994-1999 The XFree86 Project, Inc.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the XFree86 Project shall not
be used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the XFree86 Project.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include "s3v.h"
#include "s3v_pciids.h"

#include "miline.h"
	/* fb includes are in s3v.h */
#include "xaarop.h"

#include "servermd.h" /* LOG2_BYTES_PER_SCANLINE_PAD */

static void S3VNopAllCmdSets(ScrnInfoPtr pScrn);

Bool 
S3VAccelInit(ScreenPtr pScreen)
{
    return FALSE;
} 

Bool 
S3VAccelInit32(ScreenPtr pScreen) 
{
   return FALSE; 
}

static void
S3VNopAllCmdSets(ScrnInfoPtr pScrn)
{
  int i;
  int max_it=1000;
  S3VPtr ps3v = S3VPTR(pScrn);

  if (xf86GetVerbosity() > 1) {
     ErrorF("\tTrio3D -- S3VNopAllCmdSets: SubsysStats#1 = 0x%08lx\n",
        (unsigned long)IN_SUBSYS_STAT());
  }

  mem_barrier();
  for(i=0;i<max_it;i++) {
    if( (IN_SUBSYS_STAT() & 0x3f802000 & 0x20002000) == 0x20002000) {
      break;
    }
  }

  if(i!=max_it) {
    if (xf86GetVerbosity() > 1) ErrorF("\tTrio3D -- S3VNopAllCmdSets: state changed after %d iterations\n",i);
  } else {
    if (xf86GetVerbosity() > 1) ErrorF("\tTrio3D -- S3VNopAllCmdSets: state DIDN'T changed after %d iterations\n",max_it);
  }

  WaitQueue(5);

  OUTREG(CMD_SET, CMD_NOP);

  if (xf86GetVerbosity() > 1) {
     ErrorF("\tTrio3D -- S3VNopAllCmdSets: SubsysStats#2 = 0x%08lx\n",
        (unsigned long)IN_SUBSYS_STAT());
  }
}

void
S3VGEReset(ScrnInfoPtr pScrn, int from_timeout, int line, const char *file)
{
    unsigned long gs1, gs2;   /* -- debug info for graphics state -- */
    unsigned char tmp, sr1, resetidx=0x66;  /* FIXME */
    int r;
    int ge_was_on = 0;
    CARD32 fifo_control = 0, miu_control = 0;
    CARD32 streams_timeout = 0, misc_timeout = 0;
    vgaHWPtr hwp = VGAHWPTR(pScrn);
	S3VPtr ps3v = S3VPTR(pScrn);
  	int vgaCRIndex, vgaCRReg, vgaIOBase;
  	vgaIOBase = hwp->IOBase;
  	vgaCRIndex = vgaIOBase + 4;
  	vgaCRReg = vgaIOBase + 5;  	  


    if (S3_TRIO_3D_SERIES(ps3v->Chipset)) {
      VGAOUT8(0x3c4,0x01);
      sr1 = VGAIN8(0x3c5);

      if (sr1 & 0x20) {
        if (xf86GetVerbosity() > 1)
          ErrorF("\tTrio3D -- Display is on...turning off\n");
        VGAOUT8(0x3c5,sr1 & ~0x20);
        VerticalRetraceWait();
      }     
    }

    if (from_timeout) {
      if (ps3v->GEResetCnt++ < 10 || xf86GetVerbosity() > 1)
	ErrorF("\tS3VGEReset called from %s line %d\n",file,line);
    }
    else {
      if (S3_TRIO_3D_SERIES(ps3v->Chipset))
        S3VNopAllCmdSets(pScrn);
      WaitIdleEmpty();
    }


    if (from_timeout && (ps3v->Chipset == S3_ViRGE || ps3v->Chipset == S3_ViRGE_VX || ps3v->Chipset == S3_ViRGE_DXGX)) {
      /* reset will trash these registers, so save them */
      fifo_control    = INREG(FIFO_CONTROL_REG);
      miu_control     = INREG(MIU_CONTROL_REG);
      streams_timeout = INREG(STREAMS_TIMEOUT_REG);
      misc_timeout    = INREG(MISC_TIMEOUT_REG);
    }

    if(ps3v->Chipset == S3_ViRGE_VX){
        VGAOUT8(vgaCRIndex, 0x63);
        }
    else {
        VGAOUT8(vgaCRIndex, 0x66);
        }
  if (!S3_TRIO_3D_SERIES(ps3v->Chipset)) {
    tmp = VGAIN8(vgaCRReg);
    
    usleep(10000);
    for (r=1; r<10; r++) {  /* try multiple times to avoid lockup of ViRGE/MX */
      VGAOUT8(vgaCRReg, tmp | 0x02);
      usleep(10000);
      VGAOUT8(vgaCRReg, tmp & ~0x02);
      usleep(10000);

      xf86ErrorFVerb(VERBLEV, "	S3VGEReset sub_stat=%lx \n", 
   	(unsigned long)IN_SUBSYS_STAT()
	);

      if (!from_timeout) 
        WaitIdleEmpty();

      OUTREG(DEST_SRC_STR, ps3v->Bpl << 16 | ps3v->Bpl);
      
      usleep(10000);
      if (((IN_SUBSYS_STAT() & 0x3f00) != 0x3000)) 
	xf86ErrorFVerb(VERBLEV, "restarting S3 graphics engine reset %2d ...\n",r);
      else
	break;
    } 
    } else {
    usleep(10000);

    for (r=1; r<10; r++) {
      VerticalRetraceWait();
      VGAOUT8(vgaCRIndex,resetidx);
      tmp = VGAIN8(vgaCRReg);

      VGAOUT8(0x3c4,0x01);
      sr1 = VGAIN8(0x3c5);

      if(sr1 & 0x20) {
        if(xf86GetVerbosity() > 1) {
          ErrorF("\tTrio3D -- Upps Display is on again ...turning off\n");
        }
        VGAOUT8(0x3c4,0x01);
        VerticalRetraceWait();
        VGAOUT8(0x3c5,sr1 & ~0x20);
      }

      VerticalRetraceWait();
      gs1   = (long) IN_SUBSYS_STAT();

      /* turn off the GE */

      VGAOUT8(vgaCRIndex,resetidx);
      if(tmp & 0x01) {
	/*        tmp &= ~0x01; */
        VGAOUT8(vgaCRReg, tmp);
        ge_was_on = 1;
        usleep(10000);
      }

      gs2   = (long) IN_SUBSYS_STAT();
      VGAOUT8(vgaCRReg, (tmp | 0x02));
      usleep(10000);

      VerticalRetraceWait();
      VGAOUT8(vgaCRIndex,resetidx);
      VGAOUT8(vgaCRReg, (tmp & ~0x02));
      usleep(10000);

      if(ge_was_on) {
        tmp |= 0x01;
        VGAOUT8(vgaCRReg, tmp);
        usleep(10000);
      }

      if (xf86GetVerbosity() > 2) {
          ErrorF("\tTrio3D -- GE was %s ST#1: 0x%08lx ST#2: 0x%08lx\n",
		 (ge_was_on) ? "on" : "off", gs1, gs2);
      }

      VerticalRetraceWait();

      if (!from_timeout) {
	S3VNopAllCmdSets(pScrn);
        WaitIdleEmpty();
      }

      OUTREG(DEST_SRC_STR, ps3v->Bpl << 16 | ps3v->Bpl);
      usleep(10000);

      if((IN_SUBSYS_STAT() & 0x3f802000 & 0x20002000) != 0x20002000) {
        if(xf86GetVerbosity() > 1)
          ErrorF("restarting S3 graphics engine reset %2d ...%lx\n",
		 r, (unsigned long)IN_SUBSYS_STAT());
      }
        else
          break;
    }
    }
    
    if (from_timeout && (ps3v->Chipset == S3_ViRGE || ps3v->Chipset == S3_ViRGE_VX
			 || ps3v->Chipset == S3_ViRGE_DXGX)) {
      /* restore trashed registers */
      OUTREG(FIFO_CONTROL_REG, fifo_control);
      OUTREG(MIU_CONTROL_REG, miu_control);
      OUTREG(STREAMS_TIMEOUT_REG, streams_timeout);
      OUTREG(MISC_TIMEOUT_REG, misc_timeout);
    }

    WAITFIFO(2);
/*      SETB_SRC_BASE(0); */
/*      SETB_DEST_BASE(0);    */
    OUTREG(SRC_BASE, 0);
    OUTREG(DEST_BASE, 0);

  	WAITFIFO(4);
    OUTREG(CLIP_L_R, ((0) << 16) | ps3v->Width);
    OUTREG(CLIP_T_B, ((0) << 16) | ps3v->ScissB);
    OUTREG(MONO_PAT_0, ~0);
    OUTREG(MONO_PAT_1, ~0);

    if (!from_timeout && S3_TRIO_3D_SERIES(ps3v->Chipset))
      S3VNopAllCmdSets(pScrn);
}

/* The sync function for the GE */
void
S3VAccelSync(ScrnInfoPtr pScrn)
{
    S3VPtr ps3v = S3VPTR(pScrn);

    WAITIDLE();
}


void
S3VWaitFifoGX2(S3VPtr ps3v, int slots )
{
  if(ps3v->NoPCIRetry)
    while(((INREG(SUBSYS_STAT_REG) >> 9) & 0x60) < slots){}
}



void
S3VWaitFifoMain(S3VPtr ps3v, int slots )
{
  if(ps3v->NoPCIRetry) 
    while(((INREG(SUBSYS_STAT_REG) >> 8) & 0x1f) < slots){}
}


void
S3VWaitCmdGX2(S3VPtr ps3v)
{
  while(((INREG(ADV_FUNC_CNTR) >> 6) & 0x1f) != 16){}
}


void
S3VWaitDummy(S3VPtr ps3v)
{
  /* do nothing */
}

/*EOF*/

