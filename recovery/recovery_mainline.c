#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <string.h>
#include <termios.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#include <linux/input.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "rend_font.h"
#include "cjk_font.h"
#include "py_dict.h"
#include "py_words.h"
#include "libtsm.h"
#include "recovery_drm.h"
/* 控制台搜索需要读取 libtsm 自己维护的 scrollback 行；本项目将 libtsm 源码
 * 静态编进 recovery，故可安全使用同版本的内部结构。 */
#include "libtsm-int.h"
#define SCALE 3
/* 终端字号运行时可变（fontsize 命令）：char_w=16/20/24/32，char_h=char_w*2。
 * 用「宏转变量」技巧：CHAR_W/CHAR_H 变成 char_w/char_h 的别名，
 * 全文引用（dchar/draw_cjk/COLS/布局/PTY winsize）自动跟随。 */
static int char_w=20;   /* 终端字符宽（原 24×48 的 8 成） */
static int char_h=40;   /* 终端字符高 = char_w*2 */
#define CHAR_W char_w
#define CHAR_H char_h
#define COLS (1080/CHAR_W)
#define TROWS (1920/CHAR_H)
#define SBROWS 1
#define SB_H 48          /* 状态栏固定像素高（不随终端字号） */
#define KB_H_PX 768      /* 键盘区固定像素高：候选128 + 4行键帽512 + fn行128 */
#define KBROWS_EXPANDED ((KB_H_PX+char_h-1)/char_h)  /* 展开：768px 键盘 → 行数（向上取整） */
#define KBROWS_COLLAPSED ((96+char_h-1)/char_h)      /* 收起：仅底部展按钮行 96px */
static int kbrows=20;  /* 当前键盘占用行数（默认字号 20x40 下 = ceil(768/40)；运行时 apply_fontsize/kb_toggle 重算） */
#define CROWS_MAX (TROWS-SBROWS-KBROWS_COLLAPSED)  /* 37：旧静态缓冲区用固定最大维度 */
#define CROWS CROWS_MAX  /* 旧代码残留（已废弃），保持编译通过 */
static int crows=27;  /* 当前实际终端行数（默认字号 20x40：48-1-20；收/展/字号变时重算） */
#define KB_Y (1920-kbrows*CHAR_H)
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
static u8 *fb;static u32 *fb32;static int w,h,stride,cur_x,cur_y;
static int cursor_visible=1;   // CSI ?25h/l
static int cursor_style=0;     // CSI q: 0=block, 3/4=underline, 5/6=bar
static int auto_wrap=1;        // CSI ?7h/l (DECAWM)
static int sgr_reverse=0;      // SGR 7/27 reverse video
static int scroll_top=0,scroll_bottom=44;  // DECSTBM scroll region (inclusive)；44=45-1（死代码固定维度）
static int dirty=0;
static int fb_fd=-1;
static int screen_on;   /* 前置声明（定义在电源键区域） */
#define BROWSER_SESSION_FLAG "/run/op3-browser.active"
#define BROWSER_SESSION_READY "/run/op3-browser.recovery-ready"
static struct recovery_drm_display drm_display;

/* The recovery UI and Weston cannot own the DRM device at the same time.
 * browser-session writes its own PID before taking the Wayland path. Keep the
 * recovery process alive so its PTY shell and libtsm state survive, but close
 * recovery's DRM object until the browser session has released the device. */
static int browser_session_active(void){
  char b[32]={0};int fd=open(BROWSER_SESSION_FLAG,O_RDONLY);long pid=0;
  if(fd>=0){int n=read(fd,b,sizeof(b)-1);close(fd);if(n>0)pid=strtol(b,NULL,10);}
  if(pid>1 && (kill((pid_t)pid,0)==0 || errno==EPERM))return 1;
  if(fd>=0)unlink(BROWSER_SESSION_FLAG); /* recover from a killed/stale runner */
  return 0;
}
static void fb_log(const char*msg);
static void do_pan(){
  if(!dirty||fb_fd<0||!fb)return;
  msync(fb,(size_t)stride*h,MS_SYNC);
  asm volatile("dsb sy" ::: "memory");
  dirty=0;
  if(recovery_drm_present(&drm_display)<0)
    fb_log("DRM dirtyfb present failed\n");
}
#define RBUF (fb32)

static void fb_log(const char*msg){
  int lf=open("/tmp/fb.log",O_WRONLY|O_CREAT|O_APPEND,0644);
  if(lf>=0){write(lf,msg,strlen(msg));close(lf);}
}
static void fb_logf(const char*fmt,...){
  char buf[256];va_list ap;va_start(ap,fmt);
  vsnprintf(buf,sizeof(buf),fmt,ap);va_end(ap);
  fb_log(buf);
}

/* Open direct DRM/KMS. The recovery PTY/libtsm renderer writes to the mapped
 * dumb buffer and recovery_drm_present() submits the dirty frontbuffer. */
static int open_framebuffer(void){
  if(recovery_drm_open(&drm_display)<0){fb_log("DRM display open failed\n");return -1;}
  fb_fd=drm_display.fd;fb=drm_display.pixels;fb32=(u32*)fb;
  w=drm_display.width;h=drm_display.height;stride=drm_display.pitch;
  fb_logf("DRM display opened fd=%d connector=%u crtc=%u %ux%u pitch=%u\n",
          fb_fd,drm_display.connector_id,drm_display.crtc_id,w,h,stride);
  return 0;
}

/* Release every recovery-owned DRM handle before Weston attempts to take
 * DRM ownership. The PTY/libtsm state remains alive in this process. */
static void release_framebuffer(void){
  dirty=0;
  recovery_drm_close(&drm_display);
  fb=NULL;fb32=NULL;fb_fd=-1;
  w=0;h=0;stride=0;
  fb_log("DRM display released for browser handoff\n");
}

static int mark_browser_ready(void){
  int fd=open(BROWSER_SESSION_READY,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0644);
  if(fd<0){fb_logf("browser handoff ready open failed: %s\n",strerror(errno));return -1;}
  char b[]="released\n";
  int ok=(write(fd,b,sizeof(b)-1)==(ssize_t)(sizeof(b)-1));
  if(ok)fsync(fd);
  close(fd);
  if(!ok){fb_logf("browser handoff ready write failed: %s\n",strerror(errno));return -1;}
  fb_log("browser handoff ready\n");
  return 0;
}

// Console buffer: avoid re-rendering unchanged rows
// scr stores Unicode code points; wide chars (CJK) occupy 2 cells, 2nd cell = 0
// 注意：以下数组是旧自写解析器的死代码（libtsm 集成后废弃），维度用固定常量
// （COLS/CROWS 已是变量表达式，文件作用域数组不能是 VLA）。删除时机：清理死代码时。
static u32 scr[45][54];
static u32 scr_fg[45][54];
static u32 scr_bg[45][54];
static int scr_dirty[45];

// 主线 fb0 = XRGB8888（内存字节 [B,G,R,X]）→ u32 值 = 0xAARRGGBB（标准 ARGB）。
// 2026-08-21 修正：原宏 0xAABBGGRR（R 在 bit0）内存 [R,G,B,A] 会红蓝交换。
#define RGB(r,g,b) (0xFF000000u|((u32)(r)<<16)|((u32)(g)<<8)|(u32)(b))
static const u32 color16[16]={
  RGB(0,0,0),RGB(205,0,0),RGB(0,205,0),RGB(205,205,0),
  RGB(0,0,238),RGB(205,0,205),RGB(0,205,205),RGB(229,229,229),
  RGB(127,127,127),RGB(255,0,0),RGB(0,255,0),RGB(255,255,0),
  RGB(92,92,255),RGB(255,0,255),RGB(0,255,255),RGB(255,255,255),
};
static u32 color256(int n){
  if(n<16)return color16[n];
  if(n<232){int lv[6]={0,95,135,175,215,255};n-=16;return RGB(lv[n/36],lv[(n/6)%6],lv[n%6]);}
  {int g=8+(n-232)*10;return RGB(g,g,g);}
}

// current SGR color state
static u32 cur_fg=RGB(0,255,0);   // default green
static u32 cur_bg=RGB(0,0,0);     // default black

// scrollback history (view previous output)
#define SB_MAX 2000
static u32 sb[SB_MAX][54];
static u32 sb_fg[SB_MAX][54];
static u32 sb_bg[SB_MAX][54];
static int sb_count=0;
static int sb_view=0;   // 0=live, >0 = scrolled up by N lines

// alternate screen buffer (vim/less fullscreen)
static u32 alt_scr[45][54];
static u32 alt_fg[45][54];
static u32 alt_bg[45][54];
static int alt_x=0,alt_y=0;
static int use_alt=0;

static inline void fill(int x,int y,int rw,int rh,u32 c){
  for(int dy=0;dy<rh;dy++)for(int dx=0;dx<rw;dx++)RBUF[(y+dy)*(stride/4)+(x+dx)]=c;dirty=1;
}
/* iOS 风格圆角矩形（圆角 r，查表近似避免 sqrt/libm） */
static const int corner_off[12]={12,6,4,3,2,2,1,1,0,0,0,0};
static inline void fill_round(int x,int y,int rw,int rh,int r,u32 c){
  if(rw<=0||rh<=0)return;
  if(r<=0||rw<=2*r||rh<=2*r){fill(x,y,rw,rh,c);return;}
  if(r>12)r=12;
  fill(x+r,y,rw-2*r,rh,c);
  fill(x,y+r,rw,rh-2*r,c);
  for(int i=0;i<r;i++){
    int d=corner_off[i];
    fill(x+d,y+i,rw-2*d,1,c);
    fill(x+d,y+rh-1-i,rw-2*d,1,c);
  }
}
static void dchar(int x,int y,char ch,u32 fg,u32 bg){
  int i=(u8)ch;if(i<32||i>126)i=32;
  const u8*s=rend_font+(i-32)*REND_CHAR_BYTES;
  for(int r=0;r<CHAR_H;r++){
    int sr=(r*48)/CHAR_H;        /* 24x48 原图 → CHAR_H 行通用映射（字号可变） */
    const u8*row=s+sr*24;
    u32*d=RBUF+(y+r)*(stride/4)+x;
    for(int c=0;c<CHAR_W;c++){
      d[c]=row[(c*24)/CHAR_W]?fg:bg;   /* 24 列 → CHAR_W 列 */
    }
  }
  dirty=1;  /* 必须置脏：ASCII 不置脏 → 不 msync → 面板读到旧内存 → 字母/汉字重叠残留（2026-08-17 bug） */
}
/* 状态栏专用字（固定 20x40，不随终端字号变）：rend_font(24x48) 通用采样 */
static void draw_status_char(int x,int y,char ch,u32 fg,u32 bg){
  int i=(u8)ch;if(i<32||i>126)i=32;
  const u8*s=rend_font+(i-32)*REND_CHAR_BYTES;
  for(int r=0;r<40;r++){
    const u8*row=s+((r*48)/40)*24;
    u32*d=RBUF+(y+r)*(stride/4)+x;
    for(int c=0;c<20;c++)d[c]=row[(c*24)/20]?fg:bg;
  }
  dirty=1;
}
/* 候选词栏/拼音专用中文（固定 40x40 不随终端字号）：16x16 位图 → 40x40 */
static void draw_cjk_fixed(int x,int y,u32 cp,u32 fg,u32 bg){
  const u8*bm=NULL;
  if(cp>=CJK_FONT_START&&cp<CJK_FONT_START+CJK_FONT_COUNT)bm=cjk_font[cp-CJK_FONT_START];
  else if(cp>=EMOJI_START&&cp<EMOJI_START+EMOJI_COUNT)bm=emoji_font[cp-EMOJI_START];
  if(!bm)return;
  for(int r=0;r<40;r++){
    int br=(r*16)/40;
    u16 row=(u16)((bm[br*2]<<8)|bm[br*2+1]);
    for(int c=0;c<40;c++){
      int bc=(c*16)/40;
      RBUF[(y+r)*(stride/4)+(x+c)]=((row>>(15-bc))&1)?fg:bg;
    }
  }
  dirty=1;
}
/* 候选词栏专用 label（固定 20x40，不随终端字号；中文 40 宽=2 格） */
static void draw_label_fixed(int x,int y,const char*s,u32 fg,u32 bg){
  while(*s){u8 c=(u8)*s;u32 cp;int n;
    if(c<0x80){cp=c;n=1;}
    else if((c&0xE0)==0xC0){cp=c&0x1F;n=2;}
    else if((c&0xF0)==0xE0){cp=c&0x0F;n=3;}
    else if((c&0xF8)==0xF0){cp=c&0x07;n=4;}
    else{cp=32;n=1;}
    for(int i=1;i<n;i++)cp=(cp<<6)|((u8)s[i]&0x3F);
    if(cp<0x80)draw_status_char(x,y,(char)cp,fg,bg);
    else draw_cjk_fixed(x,y,cp,fg,bg);
    x+=(cp<0x80?20:40);s+=n;
  }
}
/* draw a CJK/emoji glyph (16x16 bitmap scaled to CHAR_H x CHAR_H = 40x40, width = 2 cells) */
static void draw_cjk(int x,int y,u32 cp,u32 fg,u32 bg){
  const u8*bm=NULL;
  if(cp>=CJK_FONT_START&&cp<CJK_FONT_START+CJK_FONT_COUNT)bm=cjk_font[cp-CJK_FONT_START];
  else if(cp>=EMOJI_START&&cp<EMOJI_START+EMOJI_COUNT)bm=emoji_font[cp-EMOJI_START];
  if(!bm)return;
  for(int r=0;r<CHAR_H;r++){
    int br=(r*16)/CHAR_H;      /* 源位图行（16 行 → CHAR_H 行） */
    u16 row=(u16)((bm[br*2]<<8)|bm[br*2+1]);
    for(int c=0;c<CHAR_H;c++){ /* 中文占 2 列 = CHAR_H 宽（40px 正方形） */
      int bc=(c*16)/CHAR_H;
      u32 col=(row>>(15-bc))&1?fg:bg;
      RBUF[(y+r)*(stride/4)+(x+c)]=col;
    }
  }
  dirty=1;
}
static void scr_mark_dirty(int row){scr_dirty[row]=1;dirty=1;}
static inline void scr_blank(int r){for(int c=0;c<COLS;c++)scr[r][c]=0x20;}
static void draw_cursor(){
  if(!cursor_visible)return;
  if(cur_y<0||cur_y>=CROWS||cur_x<0||cur_x>=COLS)return;
  int px=cur_x*CHAR_W, py=(cur_y+SBROWS)*CHAR_H;
  u32 cp=scr[cur_y][cur_x];
  if(cursor_style>=3){  /* underline (3/4) or bar (5/6) */
    if(cursor_style>=5)fill(px,py,2,CHAR_H,scr_fg[cur_y][cur_x]);      /* vertical bar */
    else fill(px,py+CHAR_H-3,(cp<0x80?CHAR_W:CHAR_W*2),3,scr_fg[cur_y][cur_x]); /* underline */
  }else{  /* block: invert current cell */
    if(cp<0x80)dchar(px,py,(char)cp,scr_bg[cur_y][cur_x],scr_fg[cur_y][cur_x]);
    else draw_cjk(px,py,cp,scr_bg[cur_y][cur_x],scr_fg[cur_y][cur_x]);
  }
}
static void scr_flush(){
  for(int r=0;r<CROWS;r++)if(scr_dirty[r]){
    for(int c=0;c<COLS;c++){
      u32 cp=scr[r][c];
      if(cp==0)continue;  /* wide-char continuation cell */
      if(cp<0x80)dchar(c*CHAR_W,(r+SBROWS)*CHAR_H,(char)cp,scr_fg[r][c],scr_bg[r][c]);
      else draw_cjk(c*CHAR_W,(r+SBROWS)*CHAR_H,cp,scr_fg[r][c],scr_bg[r][c]);
    }
    scr_dirty[r]=0;
  }
  draw_cursor();
}
static void scr_scroll(){
  int top=scroll_top, bot=scroll_bottom;
  if(sb_count<SB_MAX && top==0){ /* save top row to scrollback (full-screen scroll only) */
    memcpy(sb[sb_count],scr[0],sizeof(scr[0]));
    memcpy(sb_fg[sb_count],scr_fg[0],COLS*sizeof(u32));
    memcpy(sb_bg[sb_count],scr_bg[0],COLS*sizeof(u32));
    sb_count++;
  }
  memmove(scr+top,scr+top+1,(bot-top)*sizeof(scr[0]));
  memmove(scr_fg+top,scr_fg+top+1,(bot-top)*COLS*sizeof(u32));
  memmove(scr_bg+top,scr_bg+top+1,(bot-top)*COLS*sizeof(u32));
  scr_blank(bot);
  for(int c=0;c<COLS;c++){scr_fg[bot][c]=cur_fg;scr_bg[bot][c]=cur_bg;}
  for(int i=top;i<=bot;i++)scr_dirty[i]=1;
  dirty=1;
}
/* insert n lines at cursor (within scroll region) */
static void scr_insert_lines(int n){
  int top=cur_y, bot=scroll_bottom;
  if(top<scroll_top||top>bot)return;
  if(n>bot-top+1)n=bot-top+1;
  for(int r=bot;r>=top+n;r--){
    memcpy(scr[r],scr[r-n],sizeof(scr[0]));
    memcpy(scr_fg[r],scr_fg[r-n],COLS*sizeof(u32));
    memcpy(scr_bg[r],scr_bg[r-n],COLS*sizeof(u32));
  }
  for(int r=top;r<top+n;r++){
    scr_blank(r);
    for(int c=0;c<COLS;c++){scr_fg[r][c]=cur_fg;scr_bg[r][c]=cur_bg;}
  }
  for(int i=scroll_top;i<=bot;i++)scr_dirty[i]=1;
  dirty=1;
}
/* delete n lines at cursor (within scroll region) */
static void scr_delete_lines(int n){
  int top=cur_y, bot=scroll_bottom;
  if(top<scroll_top||top>bot)return;
  if(n>bot-top+1)n=bot-top+1;
  for(int r=top;r+n<=bot;r++){
    memcpy(scr[r],scr[r+n],sizeof(scr[0]));
    memcpy(scr_fg[r],scr_fg[r+n],COLS*sizeof(u32));
    memcpy(scr_bg[r],scr_bg[r+n],COLS*sizeof(u32));
  }
  for(int r=bot-n+1;r<=bot;r++){
    scr_blank(r);
    for(int c=0;c<COLS;c++){scr_fg[r][c]=cur_fg;scr_bg[r][c]=cur_bg;}
  }
  for(int i=scroll_top;i<=bot;i++)scr_dirty[i]=1;
  dirty=1;
}
/* insert n chars at cursor (shift right, truncate at line end) */
static void scr_insert_chars(int n){
  int avail=COLS-cur_x;if(n>avail)n=avail;if(n<=0)return;
  for(int c=COLS-1;c>=cur_x+n;c--){scr[cur_y][c]=scr[cur_y][c-n];scr_fg[cur_y][c]=scr_fg[cur_y][c-n];scr_bg[cur_y][c]=scr_bg[cur_y][c-n];}
  for(int c=cur_x;c<cur_x+n;c++){scr[cur_y][c]=0x20;scr_fg[cur_y][c]=cur_fg;scr_bg[cur_y][c]=cur_bg;}
  scr_mark_dirty(cur_y);
}
/* delete n chars at cursor (shift left) */
static void scr_delete_chars(int n){
  int avail=COLS-cur_x;if(n>avail)n=avail;if(n<=0)return;
  for(int c=cur_x;c<COLS-n;c++){scr[cur_y][c]=scr[cur_y][c+n];scr_fg[cur_y][c]=scr_fg[cur_y][c+n];scr_bg[cur_y][c]=scr_bg[cur_y][c+n];}
  for(int c=COLS-n;c<COLS;c++){scr[cur_y][c]=0x20;scr_fg[cur_y][c]=cur_fg;scr_bg[cur_y][c]=cur_bg;}
  scr_mark_dirty(cur_y);
}
/* erase n chars from cursor (ECH) */
static void scr_erase_chars(int n){
  int avail=COLS-cur_x;if(n>avail)n=avail;if(n<=0)return;
  for(int c=cur_x;c<cur_x+n;c++){scr[cur_y][c]=0x20;scr_fg[cur_y][c]=cur_fg;scr_bg[cur_y][c]=cur_bg;}
  scr_mark_dirty(cur_y);
}
// --- VT100/ANSI escape sequence parser (clear, cursor move, vim/htop) ---
static int esc_state=0;   // 0=normal, 1=got ESC, 2=in CSI, 3=in OSC, 4=OSC-got-ESC
static int esc_p[8];      // CSI params
static int esc_n=0;       // param count
static int esc_cur=0;     // current param value
static int esc_q=0;       // '?' private-sequence prefix
static int saved_x=0,saved_y=0;
static u32 utf8_cp=0;      // UTF-8 decode accumulator
static int utf8_need=0;    // remaining continuation bytes (0 = not in multibyte)

static void scr_clear_screen(){
  for(int r=0;r<CROWS;r++){for(int c=0;c<COLS;c++){scr[r][c]=0x20;scr_bg[r][c]=cur_bg;}scr_dirty[r]=1;}
}
static void scr_clear_to_end(){
  for(int c=cur_x;c<COLS;c++){scr[cur_y][c]=0x20;scr_bg[cur_y][c]=cur_bg;}scr_dirty[cur_y]=1;
  for(int r=cur_y+1;r<CROWS;r++){for(int c=0;c<COLS;c++){scr[r][c]=0x20;scr_bg[r][c]=cur_bg;}scr_dirty[r]=1;}
}
static void scr_clear_to_start(){
  for(int r=0;r<cur_y;r++){for(int c=0;c<COLS;c++){scr[r][c]=0x20;scr_bg[r][c]=cur_bg;}scr_dirty[r]=1;}
  for(int c=0;c<=cur_x;c++){scr[cur_y][c]=0x20;scr_bg[cur_y][c]=cur_bg;}scr_dirty[cur_y]=1;
}
static void scr_clear_line_end(){for(int c=cur_x;c<COLS;c++){scr[cur_y][c]=0x20;scr_bg[cur_y][c]=cur_bg;}scr_dirty[cur_y]=1;}
static void scr_clear_line_start(){for(int c=0;c<=cur_x;c++){scr[cur_y][c]=0x20;scr_bg[cur_y][c]=cur_bg;}scr_dirty[cur_y]=1;}
static void scr_clear_line(){for(int c=0;c<COLS;c++){scr[cur_y][c]=0x20;scr_bg[cur_y][c]=cur_bg;}scr_dirty[cur_y]=1;}

/* SGR: ESC[...m  set foreground/background color */
static void ansi_sgr(){
  if(esc_n==0){cur_fg=RGB(0,255,0);cur_bg=RGB(0,0,0);sgr_reverse=0;return;}  /* ESC[m = reset */
  for(int i=0;i<esc_n;i++){
    int p=esc_p[i];
    if(p==0){cur_fg=RGB(0,255,0);cur_bg=RGB(0,0,0);sgr_reverse=0;}
    else if(p>=30&&p<=37){cur_fg=color16[p-30];}
    else if(p==38){  /* extended fg: 38;5;N or 38;2;R;G;B */
      if(i+1<esc_n&&esc_p[i+1]==5&&i+2<esc_n){cur_fg=color256(esc_p[i+2]);i+=2;}
      else if(i+1<esc_n&&esc_p[i+1]==2&&i+4<esc_n){cur_fg=RGB(esc_p[i+2],esc_p[i+3],esc_p[i+4]);i+=4;}
    }
    else if(p==39){cur_fg=RGB(0,255,0);}
    else if(p>=40&&p<=47){cur_bg=color16[p-40];}
    else if(p==48){  /* extended bg */
      if(i+1<esc_n&&esc_p[i+1]==5&&i+2<esc_n){cur_bg=color256(esc_p[i+2]);i+=2;}
      else if(i+1<esc_n&&esc_p[i+1]==2&&i+4<esc_n){cur_bg=RGB(esc_p[i+2],esc_p[i+3],esc_p[i+4]);i+=4;}
    }
    else if(p==49){cur_bg=RGB(0,0,0);}
    else if(p>=90&&p<=97){cur_fg=color16[p-90+8];}
    else if(p>=100&&p<=107){cur_bg=color16[p-100+8];}
    else if(p==7){sgr_reverse=1;}   /* reverse video */
    else if(p==27){sgr_reverse=0;}
    /* bold/italic/underline etc: not rendered (no glyph variants), ignored */
  }
}

/* alternate screen buffer: enter/leave (ESC[?1049h/l) */
static void alt_enter(){
  if(use_alt)return;
  memcpy(alt_scr,scr,sizeof(scr));
  memcpy(alt_fg,scr_fg,sizeof(scr_fg));
  memcpy(alt_bg,scr_bg,sizeof(scr_bg));
  alt_x=cur_x;alt_y=cur_y;
  use_alt=1;
  scr_clear_screen();cur_x=0;cur_y=0;
}
static void alt_leave(){
  if(!use_alt)return;
  memcpy(scr,alt_scr,sizeof(scr));
  memcpy(scr_fg,alt_fg,sizeof(scr_fg));
  memcpy(scr_bg,alt_bg,sizeof(scr_bg));
  cur_x=alt_x;cur_y=alt_y;
  use_alt=0;
  for(int i=0;i<CROWS;i++)scr_dirty[i]=1;
}

static void ansi_csi(char cmd){
  int p0=esc_n>0?esc_p[0]:1;
  int p1=esc_n>1?esc_p[1]:1;
  switch(cmd){
    case 'H': case 'f': cur_y=(p0>0?p0:1)-1;if(cur_y>=CROWS)cur_y=CROWS-1;
                        cur_x=(p1>0?p1:1)-1;if(cur_x>=COLS)cur_x=COLS-1;break;
    case 'J': {int m=esc_n>0?esc_p[0]:0;if(m==2)scr_clear_screen();else if(m==1)scr_clear_to_start();else scr_clear_to_end();break;}
    case 'K': {int m=esc_n>0?esc_p[0]:0;if(m==2)scr_clear_line();else if(m==1)scr_clear_line_start();else scr_clear_line_end();break;}
    case 'A': cur_y-=(p0>0?p0:1);if(cur_y<0)cur_y=0;break;
    case 'B': cur_y+=(p0>0?p0:1);if(cur_y>=CROWS)cur_y=CROWS-1;break;
    case 'C': cur_x+=(p0>0?p0:1);if(cur_x>=COLS)cur_x=COLS-1;break;
    case 'D': cur_x-=(p0>0?p0:1);if(cur_x<0)cur_x=0;break;
    case 'G': cur_x=(p0>0?p0:1)-1;if(cur_x>=COLS)cur_x=COLS-1;break;
    case 'd': cur_y=(p0>0?p0:1)-1;if(cur_y>=CROWS)cur_y=CROWS-1;break;
    case 's': saved_x=cur_x;saved_y=cur_y;break;
    case 'u': cur_x=saved_x;cur_y=saved_y;break;
    case 'L': {int n=p0>0?p0:1;scr_insert_lines(n);break;}
    case 'M': {int n=p0>0?p0:1;scr_delete_lines(n);break;}
    case '@': {int n=p0>0?p0:1;scr_insert_chars(n);break;}
    case 'P': {int n=p0>0?p0:1;scr_delete_chars(n);break;}
    case 'X': {int n=p0>0?p0:1;scr_erase_chars(n);break;}
    case 'r': {  /* DECSTBM scroll region */
      if(esc_n==0){scroll_top=0;scroll_bottom=CROWS-1;break;}
      int t=(p0>0?p0:1)-1,b=(p1>0?p1:CROWS)-1;
      if(t<0)t=0;if(b>=CROWS)b=CROWS-1;
      if(b>t){scroll_top=t;scroll_bottom=b;}else{scroll_top=0;scroll_bottom=CROWS-1;}
      break;}
    case 'S': {int n=p0>0?p0:1;for(int i=0;i<n;i++)scr_scroll();break;}
    case 'T': {  /* scroll down: content moves down n lines */
      int n=p0>0?p0:1,top=scroll_top,bot=scroll_bottom;
      if(n>bot-top+1)n=bot-top+1;
      for(int r=bot;r>=top+n;r--){memcpy(scr[r],scr[r-n],sizeof(scr[0]));memcpy(scr_fg[r],scr_fg[r-n],COLS*sizeof(u32));memcpy(scr_bg[r],scr_bg[r-n],COLS*sizeof(u32));}
      for(int r=top;r<top+n;r++){scr_blank(r);for(int c=0;c<COLS;c++){scr_fg[r][c]=cur_fg;scr_bg[r][c]=cur_bg;}}
      for(int i=top;i<=bot;i++)scr_dirty[i]=1;dirty=1;
      break;}
    case 'E': cur_y+=(p0>0?p0:1);if(cur_y>=CROWS)cur_y=CROWS-1;cur_x=0;break;
    case 'F': cur_y-=(p0>0?p0:1);if(cur_y<0)cur_y=0;cur_x=0;break;
    case 'm': ansi_sgr();break;
    case 'q': {  /* DECSCUSR cursor style: 0/1/2 block, 3/4 underline, 5/6 bar */
      int s=esc_n>0?esc_p[0]:0;
      if(s>=5)cursor_style=5;else if(s>=3)cursor_style=3;else cursor_style=0;
      break;}
    case 'h': case 'l':
      if(esc_q&&esc_n>=1){
        int mode=esc_p[0];
        if(mode==1049||mode==1047||mode==47){if(cmd=='h')alt_enter();else alt_leave();}
        else if(mode==25){cursor_visible=(cmd=='h');}   /* cursor show/hide */
        else if(mode==7){auto_wrap=(cmd=='h');}         /* auto-wrap */
      }
      break;
    default: break;
  }
}

/* write a single ASCII char (1 cell) to the screen buffer */
static void put_char(char ch){
  {u32 f=cur_fg,g=cur_bg;if(sgr_reverse){u32 t=f;f=g;g=t;}
   scr[cur_y][cur_x]=(u32)(u8)ch;scr_fg[cur_y][cur_x]=f;scr_bg[cur_y][cur_x]=g;scr_mark_dirty(cur_y);}
  cur_x++;
  if(cur_x>=COLS){if(auto_wrap){cur_x=0;cur_y++;}else cur_x=COLS-1;}
  if(cur_y>=CROWS){cur_y=CROWS-1;scr_scroll();}
}
/* write a Unicode code point (CJK occupies 2 cells, 2nd cell marked 0) */
static void put_cp(u32 cp){
  if(cp<0x80){put_char((char)cp);return;}
  {u32 f=cur_fg,g=cur_bg;if(sgr_reverse){u32 t=f;f=g;g=t;}
   if(cur_x>=COLS-1){cur_x=0;cur_y++;if(cur_y>=CROWS){cur_y=CROWS-1;scr_scroll();}}
   scr[cur_y][cur_x]=cp;scr_fg[cur_y][cur_x]=f;scr_bg[cur_y][cur_x]=g;
   if(cur_x+1<COLS){scr[cur_y][cur_x+1]=0;scr_fg[cur_y][cur_x+1]=f;scr_bg[cur_y][cur_x+1]=g;}
   scr_mark_dirty(cur_y);
   cur_x+=2;
   if(cur_x>=COLS){cur_x=0;cur_y++;if(cur_y>=CROWS){cur_y=CROWS-1;scr_scroll();}}
  }
}

static void cout(char ch){
  if(esc_state==3){  /* OSC 序列中：丢弃直到 BEL 或 ST(ESC \) */
    if(ch==0x07)esc_state=0;
    else if(ch==0x1b)esc_state=4;
    return;
  }
  if(esc_state==4){  /* OSC 里的 ESC：后跟 \ = ST 结束；否则当作新 ESC 序列 */
    if(ch=='\\'){esc_state=0;return;}
    esc_state=1;  /* 不是 ST，fall through 按新 ESC 序列处理 ch */
  }
  if(esc_state==1){
    esc_state=0;
    if(ch=='['){esc_state=2;esc_n=0;esc_cur=0;esc_q=0;}
    else if(ch==']'){esc_state=3;}  /* OSC 序列：超链接(]8;;)/标题(]0;)/进度条(]9;4;3) */
    else if(ch=='7'){saved_x=cur_x;saved_y=cur_y;}  /* DEC save cursor */
    else if(ch=='8'){cur_x=saved_x;cur_y=saved_y;}  /* DEC restore cursor */
    else if(ch=='c'){scr_clear_screen();cur_x=0;cur_y=0;cur_fg=RGB(0,255,0);cur_bg=RGB(0,0,0);}  /* RIS reset */
    return;
  }
  if(esc_state==2){
    if(ch>='0'&&ch<='9'){esc_cur=esc_cur*10+(ch-'0');return;}
    if(ch==';'){if(esc_n<8)esc_p[esc_n++]=esc_cur;esc_cur=0;return;}
    if(ch=='?'){esc_q=1;return;}
    if(ch=='='||ch=='>'||ch=='!')return;  /* other prefixes: ignore */
    if(esc_n<8)esc_p[esc_n++]=esc_cur;
    esc_state=0;ansi_csi(ch);
    return;
  }
  if(ch==0x1b){esc_state=1;return;}
  if(ch=='\n'){cur_x=0;cur_y++;if(cur_y>=CROWS){cur_y=CROWS-1;scr_scroll();}return;}
  if(ch=='\r'){cur_x=0;return;}
  if(ch=='\t'){int ns=8-(cur_x%8);cur_x+=ns;if(cur_x>=COLS)cur_x=COLS-1;return;}
  if(ch=='\x7f'||ch=='\b'){if(cur_x>0)cur_x--;else if(cur_y>0){cur_y--;cur_x=COLS-1;}scr[cur_y][cur_x]=0x20;scr_fg[cur_y][cur_x]=cur_fg;scr_bg[cur_y][cur_x]=cur_bg;scr_mark_dirty(cur_y);return;}
  /* UTF-8 decode */
  if(utf8_need>0){  /* continuation byte expected */
    if((ch&0xC0)!=0x80){utf8_need=0;put_char(ch);return;}  /* invalid: emit as-is */
    utf8_cp=(utf8_cp<<6)|(ch&0x3F);
    if(--utf8_need==0)put_cp(utf8_cp);
    return;
  }
  if(ch<0x80){put_char(ch);return;}
  if((ch&0xE0)==0xC0){utf8_cp=ch&0x1F;utf8_need=1;return;}   /* 2-byte */
  if((ch&0xF0)==0xE0){utf8_cp=ch&0x0F;utf8_need=2;return;}   /* 3-byte (CJK) */
  if((ch&0xF8)==0xF0){utf8_cp=ch&0x07;utf8_need=3;return;}   /* 4-byte (emoji) */
  /* invalid byte: ignore */
}
static void print(const char*s){while(*s)cout(*s++);}
static void println(const char*s){print(s);cout('\n');}
static int rdfs(const char*p,char*b,int m){
  int f=open(p,O_RDONLY);if(f<0)return-1;
  int n=read(f,b,m-1);close(f);if(n>0&&b[n-1]=='\n')n--;b[n]=0;return n;
}

// Vibrator: 主线(6.x)走 input 力反馈（spmi_haptics event，EV_FF/FF_RUMBLE）；
// 3.18 走 timed_output/leds。vibe_init 按序探测。
static int vib_fd=-1;
static int vib_effect=-1;
static void vibe_stop(int ms){  /* 子进程：延时后停止振动（不阻塞主循环） */
  struct input_event ev={0};
  usleep(ms*1000);
  if(vib_fd>=0){ev.type=EV_FF;ev.code=vib_effect;ev.value=0;
    if(write(vib_fd,&ev,sizeof(ev))<0){}}
  _exit(0);
}
static void vibe(int ms){
  if(vib_fd<0)return;
  struct input_event ev={0};
  ev.type=EV_FF;ev.code=vib_effect;ev.value=1;
  if(write(vib_fd,&ev,sizeof(ev))<0)return;
  pid_t p=fork();
  if(p==0)vibe_stop(ms);
}
static void vibe_init(){vib_fd=-1;vib_effect=-1;
  /* 主线：找 input 设备名含 "haptics" 的，上传 FF_RUMBLE 效果 */
  for(int e=0;e<8;e++){
    char path[32];snprintf(path,sizeof(path),"/dev/input/event%d",e);
    int fd=open(path,O_RDWR);if(fd<0)continue;
    char name[64]={0};
    if(ioctl(fd,EVIOCGNAME(sizeof(name)),name)<0||!strstr(name,"haptics")){close(fd);continue;}
    unsigned long evbits[2]={0};
    if(ioctl(fd,EVIOCGBIT(0,sizeof(evbits)),evbits)<0||!(evbits[EV_FF/32]&(1UL<<(EV_FF%32)))){close(fd);continue;}
    struct ff_effect fx={0};
    fx.type=FF_RUMBLE;fx.id=-1;
    fx.u.rumble.strong_magnitude=0x6000;
    fx.u.rumble.weak_magnitude=0x8000;
    fx.replay.length=100;fx.replay.delay=0;
    if(ioctl(fd,EVIOCSFF,&fx)<0){close(fd);continue;}
    vib_fd=fd;vib_effect=fx.id;
    break;
  }
  /* 3.18 回退：timed_output/leds */
  if(vib_fd<0){
    const char* paths[]={"/sys/class/leds/vibrator/activate",
                         "/sys/class/timed_output/vibrator/enable",NULL};
    for(int i=0;paths[i];i++){vib_fd=open(paths[i],O_WRONLY);if(vib_fd>=0)break;}
  }}
static void vibe_close(){if(vib_fd>=0)close(vib_fd);}
static long long now_ms(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000LL+t.tv_nsec/1000000;}

// Keyboard (5 rows: 数字 + QWERTY + ASDF + ZXCV + 功能行，iOS iPad 风格)
#define KBH 128  /* 键帽行高 128px（键帽 116px 高，iOS 高宽比 1.21） */
#define KB_CAND_H 128  /* 候选词行高（键盘上方，拼音候选词预留） */
#define KBM 6
#define KCOL 54   /* 半格宽：全格=108px(2半格)，10全格=1080 */
#define KXOFF 0
#define KB_Y (1920-kbrows*CHAR_H)
/* iOS 浅色主题配色（宏转变量：theme 命令可切换主题色） */
#define KB_BG      kb_bg
#define KB_KEY     kb_key
#define KB_SPEC    kb_spec
#define KB_PRESS   kb_press
#define KB_PRESS_S kb_press_s
#define KB_TXT     kb_txt
static u32 kb_bg=0xFFD1D1D6;      /* 键盘背景浅灰 */
static u32 kb_key=0xFFFFFFFF;     /* 字母/数字键 白 */
static u32 kb_spec=0xFFA8A8AD;    /* 特殊键 浅灰 */
static u32 kb_press=0xFFD1D1D6;   /* 按压(普通键) */
static u32 kb_press_s=0xFF8E8E93; /* 按压(特殊键) */
static u32 kb_txt=0xFF1A1A1A;     /* 文字近黑 */
#define KF_CHAR 0
#define KF_DEL 1
#define KF_RET 2
#define KF_SHIFT 3
#define KF_PAGE 4
#define KF_CTRL 5
#define KF_TAB 6
#define KF_ESC 7
#define KF_LEFT 8
#define KF_RIGHT 9
#define KF_UP 10
#define KF_DOWN 11
#define KF_LANG 12   /* 中英切换键 🌐 */
#define KF_VOICE 13  /* 语音输入键 🎤（暂占位） */
#define KF_ABC 14    /* 回字母键盘 */
#define KF_COLLAPSE 15  /* 键盘缩进/展开 */
#define KF_SEL 16    /* 选择/复制模式 */
#define KF_PASTE 17  /* 粘贴剪贴板 */
#define KF_SLASH 18  /* / 键（功能行，2026-08-18 新增） */
#define KF_DOT 19    /* . 键（功能行，2026-08-18 新增） */
typedef struct{char*lbl;char*out;int row,col,span,func;}KbKey;

static KbKey keys_lo[]={
  /* 行0 QWERTY */
  {"q","q",0,0,2,0},{"w","w",0,2,2,0},{"e","e",0,4,2,0},{"r","r",0,6,2,0},{"t","t",0,8,2,0},
  {"y","y",0,10,2,0},{"u","u",0,12,2,0},{"i","i",0,14,2,0},{"o","o",0,16,2,0},{"p","p",0,18,2,0},
  /* 行1 ASDF 缩进半格 */
  {"a","a",1,1,2,0},{"s","s",1,3,2,0},{"d","d",1,5,2,0},{"f","f",1,7,2,0},{"g","g",1,9,2,0},
  {"h","h",1,11,2,0},{"j","j",1,13,2,0},{"k","k",1,15,2,0},{"l","l",1,17,2,0},
  /* 行2 ZXCV */
  {"⇧","",2,0,3,3},
  {"z","z",2,3,2,0},{"x","x",2,5,2,0},{"c","c",2,7,2,0},{"v","v",2,9,2,0},
  {"b","b",2,11,2,0},{"n","n",2,13,2,0},{"m","m",2,15,2,0},
  {"⌫","\x7f",2,17,3,1},
  /* 行3 功能：123 + 😊 + 空格 + 换行 */
  {"123","",3,0,3,4},{"😊","",3,3,3,0},
  {"空格"," ",3,6,10,0},{"换行","\r",3,16,4,2},
  {NULL,NULL,0,0,0,0}
};
static KbKey keys_up[]={
  /* 行0 QWERTY 大写 */
  {"Q","Q",0,0,2,0},{"W","W",0,2,2,0},{"E","E",0,4,2,0},{"R","R",0,6,2,0},{"T","T",0,8,2,0},
  {"Y","Y",0,10,2,0},{"U","U",0,12,2,0},{"I","I",0,14,2,0},{"O","O",0,16,2,0},{"P","P",0,18,2,0},
  /* 行1 ASDF 大写 */
  {"A","A",1,1,2,0},{"S","S",1,3,2,0},{"D","D",1,5,2,0},{"F","F",1,7,2,0},{"G","G",1,9,2,0},
  {"H","H",1,11,2,0},{"J","J",1,13,2,0},{"K","K",1,15,2,0},{"L","L",1,17,2,0},
  /* 行2 ZXCV 大写 */
  {"⇧","",2,0,3,3},
  {"Z","Z",2,3,2,0},{"X","X",2,5,2,0},{"C","C",2,7,2,0},{"V","V",2,9,2,0},
  {"B","B",2,11,2,0},{"N","N",2,13,2,0},{"M","M",2,15,2,0},
  {"⌫","\x7f",2,17,3,1},
  /* 行3 功能 */
  {"123","",3,0,3,4},{"😊","",3,3,3,0},
  {"空格"," ",3,6,10,0},{"换行","\r",3,16,4,2},
  {NULL,NULL,0,0,0,0}
};
static KbKey keys_num[]={
  /* 行0 数字 */
  {"1","1",0,0,2,0},{"2","2",0,2,2,0},{"3","3",0,4,2,0},{"4","4",0,6,2,0},{"5","5",0,8,2,0},
  {"6","6",0,10,2,0},{"7","7",0,12,2,0},{"8","8",0,14,2,0},{"9","9",0,16,2,0},{"0","0",0,18,2,0},
  /* 行1 符号 */
  {"-","-",1,0,2,0},{"/","/",1,2,2,0},{":",":",1,4,2,0},{";",";",1,6,2,0},{"(","(",1,8,2,0},
  {")",")",1,10,2,0},{"¥","¥",1,12,2,0},{"@","@",1,14,2,0},{"\"","\"",1,16,2,0},{"'","'",1,18,2,0},
  /* 行2 符号+删除（iPhone 数字键盘行2 风格） */
  {"#+=","",2,0,3,4},{".",".",2,3,2,0},{",",",",2,5,2,0},{"?","?",2,7,2,0},{"!","!",2,9,2,0},
  {";",";",2,11,2,0},{"⌫","\x7f",2,17,3,1},
  /* 行3 功能：拼音(→字母) + 😊 + 空格 + 换行 */
  {"拼音","",3,0,3,14},{"😊","",3,3,3,0},
  {"空格"," ",3,6,10,0},{"换行","\r",3,16,4,2},
  {NULL,NULL,0,0,0,0}
};
static KbKey keys_sym[]={
  /* 行0 符号 */
  {"[","[",0,0,2,0},{"]","]",0,2,2,0},{"{","{",0,4,2,0},{"}","}",0,6,2,0},{"#","#",0,8,2,0},
  {"%","%",0,10,2,0},{"^","^",0,12,2,0},{"*","*",0,14,2,0},{"+","+",0,16,2,0},{"=","=",0,18,2,0},
  /* 行1 符号 */
  {"-","-",1,0,2,0},{"_","_",1,2,2,0},{"|","|",1,4,2,0},{"~","~",1,6,2,0},{"《","《",1,8,2,0},
  {"》","》",1,10,2,0},{"$","$",1,12,2,0},{"&","&",1,14,2,0},{"·","·",1,16,2,0},{"¥","¥",1,18,2,0},
  /* 行2 更多符号 + 退格（方向键/Ctrl/Tab/Esc 已移到底部功能键行） */
  {"<","<",2,0,2,0},{">",">",2,2,2,0},{"/","/",2,4,2,0},{"?","?",2,6,2,0},{"!","!",2,8,2,0},
  {";",";",2,10,2,0},{":",":",2,12,2,0},{"⌫","\x7f",2,17,3,1},
  /* 行3 功能：拼音(→字母) + 😊 + 空格 + 换行 */
  {"拼音","",3,0,3,14},{"😊","",3,3,3,0},
  {"空格"," ",3,6,10,0},{"换行","\r",3,16,4,2},
  {NULL,NULL,0,0,0,0}
};
/* 底部功能键行（展开时始终显示）：中英 + Ctrl + Tab + Esc + 方向键 + 选 + 粘 + 收 + 语音 */
#define FN_KEYS 11
static KbKey keys_fn[]={
  {"Ctrl","",0,0,1,KF_CTRL},
  {"Tab","\t",0,1,1,KF_TAB},
  {"Esc","\x1b",0,2,1,KF_ESC},
  {"←","\x1b[D",0,3,1,KF_LEFT},
  {"→","\x1b[C",0,4,1,KF_RIGHT},
  {"/","/",0,5,1,KF_SLASH},
  {".",".",0,6,1,KF_DOT},
  {"选","",0,7,1,KF_SEL},
  {"粘","",0,8,1,KF_PASTE},
  {"收","",0,9,1,KF_COLLAPSE},
  {"🎤","",0,10,1,KF_VOICE},
  {NULL,NULL,0,0,0,0}
};
#define FN_W 98
/* 底部功能键行 x 坐标：11 键 × 98px = 1078（2026-08-18 移除中/↑/↓，加 / .）
   中英切换移到左电容键、上下移动用音量键（任务 75） */
static const int fn_x[FN_KEYS]={
  0,    /* Ctrl */
  98,   /* Tab */
  196,  /* Esc */
  294,  /* <- */
  392,  /* -> */
  490,  /* / */
  588,  /* . */
  686,  /* 选 */
  784,  /* 粘 */
  882,  /* 收 */
  980,  /* 音 */
};

static KbKey* kkeys=keys_lo;
static int kb_page=0;
static int ctrl_active=0;
static int ime_cn=0;  /* 输入法：0=英文，1=中文拼音 */
static int kb_collapsed=0;  /* 键盘缩进：0=展开，1=收起 */
static int kb_hl=-1;static long long kb_deadline=0;
/* 选择/复制/粘贴状态 */
static int select_mode=0;   /* 0=触摸屏滚动历史, 1=选择复制模式（"选"键切换） */
static int selecting=0;     /* 正在拖拽选择中 */
/* 选择拖拽只在一帧触摸事件结束后渲染，且仅跨字符格才重绘。
 * 直接 DRM 的 DIRTYFB 是一次同步提交；若在每个 ABS_X/ABS_Y 事件中提交，
 * 事件队列会积压，选择框就会明显落后手指。 */
static int sel_last_cx=-1,sel_last_cy=-1;
#define CLIP_MAX 4096
static char clipboard[CLIP_MAX];
static int clip_len=0;
/* 拼音输入状态 */
static char py_buf[16];       /* 当前拼音串 */
static int py_len=0;          /* 拼音串长度 */
static char py_cands[40][32]; /* 候选（UTF-8；词库最长 7 字成语=21B，16 字节曾越界崩溃，改 32） */
static int py_ncand=0;        /* 候选总数 */
static int py_page=0;         /* 当前候选页 */
static int py_per_page=8;     /* 每页候选数 */
static int py_cand_x[40];     /* 每个候选的 x 起始（当前页布局用） */
static int py_cand_w[40];     /* 每个候选的像素宽度 */
static int last_kb_bytes=1;   /* 上次键盘输入的字节数（退格删完整 UTF-8 字符） */
static void kb_rect(int*x,int*y,int*w,int row,int col,int span){
  *x=KXOFF+col*KCOL+KBM;*y=KB_Y+KB_CAND_H+row*KBH+KBM;*w=span*KCOL-KBM*2;
}
/* label 显示宽度（格数：ASCII=1，中文=2） */
static int label_w(const char*s){
  int w=0;while(*s){u8 c=(u8)*s;
    if(c<0x80){w+=1;s+=1;}
    else if((c&0xE0)==0xC0){w+=2;s+=2;}
    else if((c&0xF0)==0xE0){w+=2;s+=3;}
    else if((c&0xF8)==0xF0){w+=2;s+=4;}
    else{w+=1;s+=1;}
  }return w;
}
/* 画 UTF-8 label（支持中文） */
static void draw_label(int x,int y,const char*s,u32 fg,u32 bg){
  while(*s){u8 c=(u8)*s;u32 cp;int n;
    if(c<0x80){cp=c;n=1;}
    else if((c&0xE0)==0xC0){cp=c&0x1F;n=2;}
    else if((c&0xF0)==0xE0){cp=c&0x0F;n=3;}
    else if((c&0xF8)==0xF0){cp=c&0x07;n=4;}
    else{cp=32;n=1;}
    for(int i=1;i<n;i++)cp=(cp<<6)|((u8)s[i]&0x3F);
    if(cp<0x80)dchar(x,y,(char)cp,fg,bg);
    else draw_cjk(x,y,cp,fg,bg);
    x+=(cp<0x80?CHAR_W:CHAR_W*2);s+=n;
  }
}
/* --- 键帽大字（键盘文字保持原 24x48，不随终端 8 成缩小） --- */
#define KCAP_W 24
#define KCAP_H 48
static void dchar_big(int x,int y,char ch,u32 fg,u32 bg){  /* 原 dchar：rend_font 24x48 原样 */
  int i=(u8)ch;if(i<32||i>126)i=32;
  const u8*s=rend_font+(i-32)*REND_CHAR_BYTES;
  for(int r=0;r<KCAP_H;r++){u32*d=RBUF+(y+r)*(stride/4)+x;for(int c=0;c<KCAP_W;c++)d[c]=s[c]?fg:bg;s+=KCAP_W;}
}
static void draw_cjk_big(int x,int y,u32 cp,u32 fg,u32 bg){  /* 原 draw_cjk：16x16 位图 ×3 → 48x48 */
  const u8*bm=NULL;
  if(cp>=CJK_FONT_START&&cp<CJK_FONT_START+CJK_FONT_COUNT)bm=cjk_font[cp-CJK_FONT_START];
  else if(cp>=EMOJI_START&&cp<EMOJI_START+EMOJI_COUNT)bm=emoji_font[cp-EMOJI_START];
  if(!bm)return;
  for(int r=0;r<16;r++){
    u16 row=(u16)((bm[r*2]<<8)|bm[r*2+1]);
    for(int c=0;c<16;c++){
      u32 col=(row>>(15-c))&1?fg:bg;
      for(int dy=0;dy<3;dy++)
        for(int dx=0;dx<3;dx++)
          RBUF[(y+r*3+dy)*(stride/4)+(x+c*3+dx)]=col;
    }
  }
}
/* 键帽 label（UTF-8）：ASCII 用 dchar_big(24px)，中文用 draw_cjk_big(48px)，步进 24/48 */
static void draw_kb_label(int x,int y,const char*s,u32 fg,u32 bg){
  while(*s){u8 c=(u8)*s;u32 cp;int n;
    if(c<0x80){cp=c;n=1;}
    else if((c&0xE0)==0xC0){cp=c&0x1F;n=2;}
    else if((c&0xF0)==0xE0){cp=c&0x0F;n=3;}
    else if((c&0xF8)==0xF0){cp=c&0x07;n=4;}
    else{cp=32;n=1;}
    for(int i=1;i<n;i++)cp=(cp<<6)|((u8)s[i]&0x3F);
    if(cp<0x80)dchar_big(x,y,(char)cp,fg,bg);
    else draw_cjk_big(x,y,cp,fg,bg);
    x+=(cp<0x80?KCAP_W:KCAP_H);s+=n;
  }
}
static void draw_kb_key(int idx){
  if(idx<0)return;KbKey*k=&kkeys[idx];int x_,y_,w_;kb_rect(&x_,&y_,&w_,k->row,k->col,k->span);
  int special=(k->func!=KF_CHAR);
  int pressed=(idx==kb_hl&&now_ms()<kb_deadline);
  u32 bg=special?(pressed?KB_PRESS_S:KB_SPEC):(pressed?KB_PRESS:KB_KEY);
  int kh=KBH-2*KBM;
  fill_round(x_,y_,w_,kh,12,bg);
  if(k->lbl[0]){int lpx=label_w(k->lbl)*KCAP_W;int ox=x_+(w_-lpx)/2;int oy=y_+(kh-KCAP_H)/2;draw_kb_label(ox,oy,k->lbl,KB_TXT,bg);}
}
static void kb_clear(){fill(0,KB_Y,1080,kbrows*CHAR_H,0xFF000000);}
/* 屏幕底部独立 🌐/🎤 键（不在 KbKey 数组里） */
#define KB_LANG_Y (KB_Y+KB_CAND_H+4*KBH)
#define KB_LANG_H KBH
/* 根据 ime_cn 动态更新键盘行 3 第一个键 label（中英文切换时调） */
static void ime_update_kb_labels(){
  const char*letter_lbl="123";  /* 字母页行3第一个键：永远「123」（进入数字页），中英文一致 */
  for(int i=0;keys_lo[i].lbl;i++)if(keys_lo[i].row==3&&keys_lo[i].col==0)keys_lo[i].lbl=(char*)letter_lbl;
  for(int i=0;keys_up[i].lbl;i++)if(keys_up[i].row==3&&keys_up[i].col==0)keys_up[i].lbl=(char*)letter_lbl;
  const char*num_lbl=ime_cn?"拼音":"abc";   /* 数字/符号页 KF_ABC 键（回字母） */
  for(int i=0;keys_num[i].lbl;i++)if(keys_num[i].row==3&&keys_num[i].col==0)keys_num[i].lbl=(char*)num_lbl;
  for(int i=0;keys_sym[i].lbl;i++)if(keys_sym[i].row==3&&keys_sym[i].col==0)keys_sym[i].lbl=(char*)num_lbl;
}
/* 底部功能键行绘制（中英/Ctrl/Tab/Esc/方向键/收/语音，10 键按 fn_x 分布） */
static void draw_fnrow(){
  int y=KB_LANG_Y+KBM;int h=KB_LANG_H-2*KBM;
  for(int i=0;i<FN_KEYS;i++){
    KbKey*k=&keys_fn[i];
    int x=fn_x[i];
    const char*lbl=k->lbl;
    u32 bg=KB_SPEC;
    if(k->func==KF_LANG){lbl=ime_cn?"中":"英";if(ime_cn)bg=KB_PRESS_S;}  /* 显示当前语言：中文"中"，英文"英" */
    else if(k->func==KF_COLLAPSE){lbl="收";}
    else if(k->func==KF_SEL){lbl="选";if(select_mode)bg=KB_PRESS_S;}    /* 选择模式：高亮"选"键 */
    else if(k->func==KF_CTRL&&ctrl_active){bg=KB_PRESS_S;}
    fill_round(x,y,FN_W,h,12,bg);
    int lw=label_w(lbl)*KCAP_W;
    draw_kb_label(x+(FN_W-lw)/2,y+(h-KCAP_H)/2,lbl,KB_TXT,bg);
  }
}
/* --- 拼音输入法 --- */
void sh_input(const char*s);  /* 前向声明（定义在后面 PTY 区） */
/* Unicode 码点 → UTF-8 到 out */
static void cp_to_utf8(u32 cp, char* out){
  int n=0;
  if(cp<0x80)out[n++]=(char)cp;
  else if(cp<0x800){out[n++]=(char)(0xC0|(cp>>6));out[n++]=(char)(0x80|(cp&0x3F));}
  else if(cp<0x10000){out[n++]=(char)(0xE0|(cp>>12));out[n++]=(char)(0x80|((cp>>6)&0x3F));out[n++]=(char)(0x80|(cp&0x3F));}
  else{out[n++]=(char)(0xF0|(cp>>18));out[n++]=(char)(0x80|((cp>>12)&0x3F));out[n++]=(char)(0x80|((cp>>6)&0x3F));out[n++]=(char)(0x80|(cp&0x3F));}
  out[n]=0;
}
/* 候选查找：词组完全匹配(最优先) + 单字前缀 + 词组前缀 */
static void py_search(void){
  py_ncand=0;py_page=0;
  if(py_len<=0)return;
  /* 1. 词组完全匹配 */
  for(int i=0;i<WORD_COUNT && py_ncand<40;i++){
    if(strcmp(py_words[i].py, py_buf)==0){
      strcpy(py_cands[py_ncand], py_words[i].word);
      py_ncand++;
    }
  }
  /* 2. 单字前缀匹配 */
  for(int i=0;i<PY_COUNT && py_ncand<40;i++){
    if(strncmp(py_chars[i].py, py_buf, py_len)==0){
      cp_to_utf8(py_chars[i].hanzi, py_cands[py_ncand]);
      py_ncand++;
    }
  }
  /* 3. 词组前缀匹配（非完全匹配） */
  for(int i=0;i<WORD_COUNT && py_ncand<40;i++){
    if(strcmp(py_words[i].py, py_buf)!=0 && strncmp(py_words[i].py, py_buf, py_len)==0){
      strcpy(py_cands[py_ncand], py_words[i].word);
      py_ncand++;
    }
  }
}
/* 上屏候选（UTF-8 字符串），清空拼音串 */
static void py_commit(const char* u8){
  sh_input(u8);
  last_kb_bytes=strlen(u8);  /* 上屏词条字节数（退格删完整词/字） */
  py_len=0;py_buf[0]=0;py_ncand=0;
}
/* 候选词栏绘制：上一页(最左) + 拼音串(绿) + 候选按钮(白底圆角) + 下一页(最右)。
 * 翻页按钮放两端拉开距离，避免误触。 */
static void draw_cand(void){
  fill(0,KB_Y,1080,KB_CAND_H,KB_BG);
  int y=KB_Y+(KB_CAND_H-40)/2;      /* 文字垂直中心（固定 20x40，不随终端字号） */
  if(!ime_cn || py_len<=0)return;
  int bh=KB_CAND_H-20;                  /* 按钮高 108px，上下各留 10px */
  int by=KB_Y+(KB_CAND_H-bh)/2;
  /* 上一页按钮：最左，宽 60 */
  if(py_page>0){fill_round(4,by,60,bh,14,KB_SPEC);draw_status_char(4+(60-20)/2,y,'<',0xFF0000FF,KB_SPEC);}
  /* 拼音串（固定 20x40，步进 22） */
  int x=76;
  for(int i=0;i<py_len;i++){draw_status_char(x,y,py_buf[i],0xFF00AA00,KB_BG);x+=22;}
  x+=8;
  int start=py_page*py_per_page;
  int end=py_ncand<(start+py_per_page)?py_ncand:(start+py_per_page);
  for(int i=start;i<end;i++){
    int tw=label_w(py_cands[i])*20; /* 文字宽（固定 20px/格，中文 40=2 格） */
    int bw=tw+40;                        /* 按钮宽 = 文字 + 左右各 20px 内边距 */
    if(x+bw>1080-72)break;               /* 留下一页按钮空间 */
    py_cand_x[i]=x;py_cand_w[i]=bw;
    fill_round(x,by,bw,bh,14,KB_KEY);
    draw_label_fixed(x+(bw-tw)/2, by+(bh-40)/2, py_cands[i], KB_TXT, KB_KEY);
    x+=bw+14;
  }
  /* 下一页按钮：最右，宽 60 */
  if(end<py_ncand){fill_round(1080-64,by,60,bh,14,KB_SPEC);draw_status_char(1080-64+(60-20)/2,y,'>',0xFF0000FF,KB_SPEC);}
}
/* 候选词栏触摸检测：>=0 候选索引，-2 上一页，-3 下一页，-1 无 */
static int kb_find_cand(int tx,int ty){
  if(!ime_cn || py_ncand<=0)return -1;
  if(ty<KB_Y||ty>=KB_Y+KB_CAND_H)return -1;
  int start=py_page*py_per_page;
  int end=py_ncand<(start+py_per_page)?py_ncand:(start+py_per_page);
  if(py_page>0 && tx>=4 && tx<4+60)return -2;    /* 上一页（最左） */
  if(end<py_ncand && tx>=1080-64)return -3;      /* 下一页（最右） */
  for(int i=start;i<end;i++){
    if(tx>=py_cand_x[i] && tx<py_cand_x[i]+py_cand_w[i])return i;
  }
  return -1;
}
static void draw_kb(){
  if(kb_collapsed){
    /* 收起：整个键盘区变成终端背景（黑底），让控制台内容显示进来 */
    fill(0,KB_Y,1080,kbrows*CHAR_H,0xFF000000);
    /* 只在屏幕底部中央画一个"展"按钮（圆角 + 浅灰） */
    int by=1920-KBH+KBM;const char*lbl="展";int lw=label_w(lbl)*KCAP_W;
    fill_round(492,by,96,KBH-2*KBM,12,KB_SPEC);
    draw_kb_label(492+(96-lw)/2,by+(KBH-2*KBM-KCAP_H)/2,lbl,KB_TXT,KB_SPEC);
  } else {
    /* 展开：键盘背景（浅灰）+ 候选词栏 + 所有键帽 + 底部功能键行 */
    fill(0,KB_Y,1080,kbrows*CHAR_H,KB_BG);
    draw_cand();
    for(int i=0;kkeys[i].lbl;i++)draw_kb_key(i);
    draw_fnrow();
  }
}
static int kb_find_collapse(int tx,int ty){
  int by=kb_collapsed?(1920-KBH+KBM):(KB_LANG_Y+KBM);
  if(ty<by||ty>=by+KBH-2*KBM)return -1;
  if(tx>=492&&tx<492+96)return KF_COLLAPSE;
  return -1;
}
static int kb_find(int tx,int ty){
  if(ty<KB_Y+KB_CAND_H||ty>=KB_Y+kbrows*CHAR_H)return-1;int row=(ty-KB_Y-KB_CAND_H)/KBH;
  for(int i=0;kkeys[i].lbl;i++)if(kkeys[i].row==row){int x_,y_,w_;kb_rect(&x_,&y_,&w_,kkeys[i].row,kkeys[i].col,kkeys[i].span);if(tx>=x_&&tx<=x_+w_)return i;}return-1;
}
/* 底部功能键行触摸检测（返回 keys_fn 索引或 -1） */
static int kb_find_fn(int tx,int ty){
  if(ty<KB_LANG_Y||ty>=KB_LANG_Y+KB_LANG_H)return-1;
  for(int i=0;i<FN_KEYS;i++){
    int x=fn_x[i];
    if(tx>=x&&tx<x+FN_W)return i;
  }
  return -1;
}

// PTY Shell — 多标签（最多 4 个独立 PTY + libtsm 实例）
#define MAX_TABS 4
#define CMD_BLOCK_MAX 32
#define CMD_CAPTURE_MAX 8192
typedef struct{
  int fd;                 /* PTY master，-1=未创建 */
  pid_t pid;
  struct tsm_screen *scr;
  struct tsm_vte *vte;
  /* OSC 133;A is emitted before every interactive shell prompt.  Keep the
   * recent command boundaries as scrollback anchors, plus the latest command
   * transcript for the command palette's copy action. */
  struct line *cmd_anchor[CMD_BLOCK_MAX];
  int cmd_count,cmd_nav;
  char osc_pending[8];int osc_pending_len;
  char cmd_capture[CMD_CAPTURE_MAX];int cmd_capture_len,cmd_capture_live;
  char cmd_last[CMD_CAPTURE_MAX];int cmd_last_len;
}Tab;
static Tab tabs[MAX_TABS];
static int cur_tab=0;     /* 当前标签 0..3 */
/* 宏：让现有代码的变量引用自动指向当前标签（注意不能叫 tsm_vte，会和 libtsm 类型/API 冲突） */
#define cur_scr (tabs[cur_tab].scr)
#define cur_vte (tabs[cur_tab].vte)
#define cur_fd (tabs[cur_tab].fd)
static int render_cell(struct tsm_screen*,uint64_t,const uint32_t*,size_t,unsigned int,unsigned int,unsigned int,const struct tsm_screen_attr*,tsm_age_t,void*);
static void vte_write_cb(struct tsm_vte*,const char*,size_t,void*);
static void draw_statusbar(void);
void sh_input(const char*s);
static void kb_toggle_collapse(void);
static void switch_tab(int i);
static void draw_ui_overlay(void);

/* ============ 控制台二期：Shell 命令块 ============ */
static const char osc_prompt_mark[]="\033]133;A\007";
static int cmd_anchor_is_live(struct tsm_screen*scr,struct line*needle){
  if(!scr||!needle)return 0;
  for(struct line*l=scr->sb_first;l;l=l->next)if(l==needle)return 1;
  return 0;
}
static void cmd_finish(Tab*t){
  if(!t->cmd_capture_live)return;
  while(t->cmd_capture_len>0 && (t->cmd_capture[t->cmd_capture_len-1]=='\r'||t->cmd_capture[t->cmd_capture_len-1]=='\n'))t->cmd_capture_len--;
  if(t->cmd_capture_len>0){
    memcpy(t->cmd_last,t->cmd_capture,(size_t)t->cmd_capture_len);
    t->cmd_last[t->cmd_capture_len]=0;t->cmd_last_len=t->cmd_capture_len;
  }
  t->cmd_capture_len=0;t->cmd_capture_live=1;
  if(t->scr&&t->scr->sb_last){
    if(t->cmd_count<CMD_BLOCK_MAX)t->cmd_anchor[t->cmd_count++]=t->scr->sb_last;
    else{memmove(t->cmd_anchor,t->cmd_anchor+1,(CMD_BLOCK_MAX-1)*sizeof(t->cmd_anchor[0]));t->cmd_anchor[CMD_BLOCK_MAX-1]=t->scr->sb_last;}
    t->cmd_nav=t->cmd_count;
  }
}
static void cmd_capture_char(Tab*t,char c){
  if(t->cmd_capture_live&&t->cmd_capture_len<CMD_CAPTURE_MAX-1){t->cmd_capture[t->cmd_capture_len++]=c;t->cmd_capture[t->cmd_capture_len]=0;}
}
/* Strip the private OSC marker before it reaches libtsm, while retaining all
 * ordinary PTY output byte-for-byte for terminal compatibility. */
static void tab_vte_input(int ti,const char*buf,int len){
  if(ti<0||ti>=MAX_TABS||!tabs[ti].vte)return;
  Tab*t=&tabs[ti];char out[512];int on=0;
  for(int i=0;i<len;i++){
    char c=buf[i];
retry:
    if(c==osc_prompt_mark[t->osc_pending_len]){
      t->osc_pending[t->osc_pending_len++]=c;
      if(t->osc_pending_len==(int)sizeof(osc_prompt_mark)-1){
        if(on){tsm_vte_input(t->vte,out,on);on=0;}
        t->osc_pending_len=0;cmd_finish(t);continue;
      }
      continue;
    }
    if(t->osc_pending_len){
      for(int j=0;j<t->osc_pending_len;j++){out[on++]=t->osc_pending[j];cmd_capture_char(t,t->osc_pending[j]);if(on==(int)sizeof(out)){tsm_vte_input(t->vte,out,on);on=0;}}
      t->osc_pending_len=0;goto retry;
    }
    out[on++]=c;cmd_capture_char(t,c);
    if(on==(int)sizeof(out)){tsm_vte_input(t->vte,out,on);on=0;}
  }
  if(on)tsm_vte_input(t->vte,out,on);
}
static int cmd_jump(int direction){
  Tab*t=&tabs[cur_tab];
  if(!cur_scr||t->cmd_count<=0)return 0;
  int n=t->cmd_nav+direction;if(n<0)n=0;if(n>=t->cmd_count)n=t->cmd_count-1;
  while(n>=0&&n<t->cmd_count&&!cmd_anchor_is_live(cur_scr,t->cmd_anchor[n]))n+=direction;
  if(n<0||n>=t->cmd_count)return 0;
  t->cmd_nav=n;cur_scr->sb_pos=t->cmd_anchor[n];
  tsm_screen_draw(cur_scr,render_cell,NULL);draw_statusbar();draw_kb();draw_ui_overlay();do_pan();return 1;
}
/* Short commands may still be in the live screen rather than scrollback.
 * Copy that visible text as a safe fallback instead of silently doing nothing. */
static int cmd_copy_visible(void){
  if(!cur_scr||!cur_scr->lines)return 0;
  int n=0;
  for(unsigned int r=0;r<cur_scr->size_y&&n<CLIP_MAX-2;r++){
    struct line*l=cur_scr->lines[r];int end=l?(int)l->size:0;
    while(end>0&&l->cells[end-1].ch==' ')end--;
    for(int c=0;c<end&&n<CLIP_MAX-2;c++){
      if(l->cells[c].width==0)continue;
      tsm_symbol_t ch=l->cells[c].ch;clipboard[n++]=(ch>=32&&ch<127)?(char)ch:'?';
    }
    if(end>0)clipboard[n++]='\n';
  }
  clipboard[n]=0;clip_len=n;return n;
}
static int cmd_copy_last(void){
  Tab*t=&tabs[cur_tab];
  if(t->cmd_last_len>0){memcpy(clipboard,t->cmd_last,(size_t)t->cmd_last_len);clipboard[t->cmd_last_len]=0;clip_len=t->cmd_last_len;return clip_len;}
  return cmd_copy_visible();
}

/* ============ 控制台一期：搜索 + 命令面板 ============ */
#define UI_NONE 0
#define UI_SEARCH 1
#define UI_PALETTE 2
#define UI_QUERY_MAX 63
#define UI_MATCH_MAX 128
static int ui_mode=UI_NONE;
static char ui_query[UI_QUERY_MAX+1];
static int ui_qlen=0,ui_match_count=0,ui_match_idx=0;
static struct line*ui_matches[UI_MATCH_MAX];
static char ui_notice[96];
static void draw_ui_overlay(void);

static int ui_line_match(const struct line*line,const char*needle){
  char text[256];int n=0;
  if(!line||!needle[0])return 0;
  for(unsigned int i=0;i<line->size&&n<(int)sizeof(text)-1;i++){
    tsm_symbol_t ch=line->cells[i].ch;
    if(line->cells[i].width==0)continue;
    text[n++]=(ch>=32&&ch<127)?(char)tolower((unsigned char)ch):' ';
  }
  text[n]=0;
  return strstr(text,needle)!=NULL;
}
static void ui_search_collect(void){
  ui_match_count=0;ui_match_idx=0;
  if(!cur_scr||ui_qlen==0)return;
  for(struct line*l=cur_scr->sb_first;l&&ui_match_count<UI_MATCH_MAX;l=l->next)
    if(ui_line_match(l,ui_query))ui_matches[ui_match_count++]=l;
}
static void ui_search_show(void){
  if(cur_scr&&ui_match_count>0){
    cur_scr->sb_pos=ui_matches[ui_match_idx];
    tsm_screen_draw(cur_scr,render_cell,NULL);
  }
  draw_statusbar();draw_kb();draw_ui_overlay();do_pan();
}
static void ui_open_search(void){
  ui_mode=UI_SEARCH;ui_qlen=0;ui_query[0]=0;ui_search_collect();ui_search_show();
}
static void ui_open_palette(void){ui_mode=UI_PALETTE;ui_notice[0]=0;draw_ui_overlay();do_pan();}
static void ui_close(void){
  ui_mode=UI_NONE;
  if(cur_scr)tsm_screen_draw(cur_scr,render_cell,NULL);
  draw_statusbar();draw_kb();do_pan();
}
static void ui_smart_clipboard(void){
  if(clip_len<=0)return;
  if(!strncmp(clipboard,"http://",7)||!strncmp(clipboard,"https://",8)){
    sh_input("links ");sh_input(clipboard);
  }else if(clipboard[0]=='/'){
    sh_input("cd ");sh_input(clipboard);
  }
}
static void ui_palette_action(int action){
  if(action==0){ui_open_search();return;}
  if(action==1){for(int i=0;i<MAX_TABS;i++)if(tabs[i].fd<0){switch_tab(i);break;}}
  else if(action==2)sh_input("\033[2J\033[H");
  else if(action==3)ui_smart_clipboard();
  else if(action==4)kb_toggle_collapse();
  else if(action==5){snprintf(ui_notice,sizeof(ui_notice),"%s",cmd_jump(-1)?"已跳到上一命令块":"没有可跳转的命令块（短输出仍在当前屏幕）");return;}
  else if(action==6){snprintf(ui_notice,sizeof(ui_notice),"%s",cmd_jump(1)?"已跳到下一命令块":"没有可跳转的命令块（短输出仍在当前屏幕）");return;}
  else if(action==7){int n=cmd_copy_last();snprintf(ui_notice,sizeof(ui_notice),n>0?"已复制 %d 字节；关闭面板后点粘贴":"没有可复制的终端内容",n);return;}
  ui_close();
}
static void ui_key(const char*s,int func){
  if(func==KF_ESC){ui_close();return;}
  if(ui_mode==UI_SEARCH){
    if(func==KF_DEL){if(ui_qlen>0)ui_query[--ui_qlen]=0;ui_search_collect();ui_search_show();return;}
    if(func==KF_RET){if(ui_match_count>0){ui_match_idx=(ui_match_idx+1)%ui_match_count;ui_search_show();}return;}
    if(func==KF_CHAR&&s&&s[0]&&ui_qlen<UI_QUERY_MAX&&((unsigned char)s[0])<128){
      ui_query[ui_qlen++]=(char)tolower((unsigned char)s[0]);ui_query[ui_qlen]=0;ui_search_collect();ui_search_show();
    }
  }else if(ui_mode==UI_PALETTE&&func==KF_CHAR&&s&&s[0]){
    char c=(char)tolower((unsigned char)s[0]);
    if(c=='f')ui_palette_action(0);else if(c=='n')ui_palette_action(1);
    else if(c=='c')ui_palette_action(2);else if(c=='o')ui_palette_action(3);
    else if(c=='k')ui_palette_action(4);else if(c=='b')ui_palette_action(5);
    else if(c=='j')ui_palette_action(6);else if(c=='y')ui_palette_action(7);
  }
}
static void draw_ui_overlay(void){
  if(ui_mode==UI_NONE)return;
  int y=SB_H+8;
  if(ui_mode==UI_SEARCH){
    fill_round(12,y,1056,112,16,0xEE20242Au);
    draw_label_fixed(32,y+12,"查找:",0xFFFFFFFF,0xEE20242Au);
    draw_label_fixed(164,y+12,ui_query[0]?ui_query:"(输入关键词)",0xFF66D9EF,0xEE20242Au);
    char info[64];snprintf(info,sizeof(info),"%d 条  回车=下一个  Esc=关闭",ui_match_count);
    draw_label_fixed(32,y+60,info,0xFFD1D1D6,0xEE20242Au);
  }else{
    fill_round(12,y,1056,618,16,0xEE20242Au);
    draw_label_fixed(32,y+12,ui_notice[0]?ui_notice:"命令面板  Esc=关闭",0xFFFFFFFF,0xEE20242Au);
    const char*items[]={"F 查找历史","N 新建标签","C 清屏","O 智能打开剪贴板","K 收起/展开键盘","B 上一命令块","J 下一命令块","Y 复制上一命令输出"};
    for(int i=0;i<8;i++){
      int by=y+64+i*66;fill_round(32,by,1016,54,10,0xFF3A3A3Cu);
      draw_label_fixed(54,by+7,items[i],0xFFFFFFFF,0xFF3A3A3Cu);
    }
  }
}
/* 新建标签 i：PTY + libtsm screen/vte + fork shell */
static uint8_t tpalette[TSM_COLOR_NUM][3]={
  [TSM_COLOR_BLACK]={0,0,0}, [TSM_COLOR_RED]={255,0,0},
  [TSM_COLOR_GREEN]={0,255,0}, [TSM_COLOR_YELLOW]={255,255,0},
  [TSM_COLOR_BLUE]={0,0,255}, [TSM_COLOR_MAGENTA]={255,0,255},
  [TSM_COLOR_CYAN]={0,255,255}, [TSM_COLOR_LIGHT_GREY]={192,192,192},
  [TSM_COLOR_DARK_GREY]={128,128,128}, [TSM_COLOR_LIGHT_RED]={255,128,128},
  [TSM_COLOR_LIGHT_GREEN]={128,255,128}, [TSM_COLOR_LIGHT_YELLOW]={255,255,128},
  [TSM_COLOR_LIGHT_BLUE]={128,128,255}, [TSM_COLOR_LIGHT_MAGENTA]={255,128,255},
  [TSM_COLOR_LIGHT_CYAN]={128,255,255}, [TSM_COLOR_WHITE]={255,255,255},
  [TSM_COLOR_FOREGROUND]={255,255,255}, [TSM_COLOR_BACKGROUND]={0,0,0},
};
static void vte_palette(struct tsm_vte*vte){
  tsm_vte_set_custom_palette(vte, tpalette);
}
static void start_tab(int i){
  if(i<0||i>=MAX_TABS||tabs[i].fd>=0)return;
  struct tsm_screen *scr=NULL;struct tsm_vte *vte=NULL;
  if(tsm_screen_new(&scr,NULL,NULL))return;
  tsm_screen_resize(scr,COLS,crows);
  tsm_screen_set_max_sb(scr,2000);
  if(tsm_vte_new(&vte,scr,vte_write_cb,NULL,NULL,NULL))return;
  vte_palette(vte);
  int ptm=posix_openpt(O_RDWR|O_NOCTTY);if(ptm<0)return;
  grantpt(ptm);unlockpt(ptm);char*sn=ptsname(ptm);
  struct winsize ws={crows,COLS,crows*CHAR_H,COLS*CHAR_W};ioctl(ptm,TIOCSWINSZ,&ws);
  int pid=fork();
  if(pid==0){setsid();int pts=open(sn,O_RDWR);close(ptm);
    ioctl(pts,TIOCSCTTY,0);
    struct termios t;tcgetattr(pts,&t);
    /* Enable ECHO so shell echoes typed chars; VERASE=DEL for keyboard DEL */
    t.c_lflag|=(ICANON|ECHO|ECHOE|ECHOK);
    t.c_lflag&=~ECHOCTL;
    t.c_iflag|=(ICRNL);  /* CR -> NL on input (键盘发 \r, ICANON 下翻译成 \n) */
    t.c_cc[VERASE]=0x7f;
    tcsetattr(pts,TCSANOW,&t);
    setenv("TERM","xterm-256color",1);  /* libtsm 支持 xterm 序列，codebuddy/pi 按此探测终端能力 */
    /* OSC 133;A is a private prompt boundary consumed by tab_vte_input(). */
    setenv("PS1","\033]133;A\007# ",1);
    setenv("SSL_CERT_FILE","/etc/ssl/certs/ca-certificates.crt",1);  /* links 等 HTTPS 用系统 CA（设备无默认 CA 路径） */
    setenv("LANG","C.UTF-8",1);  /* 激活 busybox lineedit 的 UTF-8 退格（UNICODE_USING_LOCALE 检测 LANG；否则退格按字节删，中文删一半） */
    dup2(pts,0);dup2(pts,1);dup2(pts,2);if(pts>2)close(pts);
    /* 主线调试环境(pmOS initramfs/Alpine)无 /sbin/sh，fallback 到 /bin/sh（3.18 Buildroot 两者都有） */
    if(access("/sbin/sh",X_OK)==0)execl("/sbin/sh","sh","-i",NULL);
    execl("/bin/sh","sh","-i",NULL);_exit(1);}
  if(pid<0){close(ptm);return;}
  tabs[i].pid=pid;
  fcntl(ptm,F_SETFL,fcntl(ptm,F_GETFL)|O_NONBLOCK);
  tabs[i].fd=ptm;tabs[i].scr=scr;tabs[i].vte=vte;
}
/* 切换标签：已存在则切换，不存在则新建；内容保留不清空 */
static void switch_tab(int i){
  if(i<0||i>=MAX_TABS)return;
  if(tabs[i].fd<0)start_tab(i);
  if(tabs[i].fd<0)return;
  cur_tab=i;
  tsm_screen_draw(tabs[i].scr,render_cell,NULL);
  draw_statusbar();
  draw_ui_overlay();
  do_pan();
}

// --- 三段式性能模式（上=性能600 中=均衡601 下=省电602，定义在 draw_statusbar 前供其显示） ---
static int screen_on=1;   /* 息屏状态（apply_tri_mode 依赖，故放前面） */
static int tri_mode=1;    /* 0=性能 performance, 1=均衡 interactive, 2=省电 powersave */
static const char*tri_gov[]={"performance","interactive","powersave"};
static const char*tri_lbl[]={"P","I","S"};  /* 状态栏模式指示 */
static void set_gov(const char*g){
  for(int c=0;c<4;c++){
    char p[64];snprintf(p,sizeof(p),"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor",c);
    int fd=open(p,O_WRONLY);if(fd>=0){write(fd,g,strlen(g));close(fd);}
  }
}
static void apply_tri_mode(int m){
  if(m<0||m>2)m=1;
  tri_mode=m;
  /* 持久化到 /root/tri_mode：开机恢复（无 sysfs 状态接口，用文件记上次档位；拨动滑块立即纠正） */
  int fd=open("/root/tri_mode",O_WRONLY|O_CREAT|O_TRUNC,0644);
  if(fd>=0){char b[8];int n=snprintf(b,sizeof(b),"%d\n",m);write(fd,b,n);close(fd);}
  if(screen_on)set_gov(tri_gov[m]);   /* 息屏保持降载状态，唤醒时恢复 */
}
static void tri_handle(int code){
  if(code==600)apply_tri_mode(0);       /* 上=性能 */
  else if(code==601)apply_tri_mode(1);  /* 中=均衡 */
  else if(code==602)apply_tri_mode(2);  /* 下=省电 */
}
// --- 语音录音（🎤 键）：按一下开始录，再按一下停止并回放 ---
/* 状态栏配色（宏转变量：theme 命令可切换） */
static u32 st_bg=0xFF001122;
static u32 st_fg=0xFFFFFFFF;
static u32 st_tab=0xFFFFAA00;
static u32 st_ip=0xFF00FF00;
static u32 st_time=0xFF00FFFF;
static u32 st_yellow=0xFFFFFF00;
static u32 st_red=0xFFFF3030;
static u32 st_noip=0xFFFF8800;

static int voice_recording=0;
static pid_t voice_pid=-1;
static long long voice_press_start=0;  /* KF_VOICE 按下时间(ms)；=0 表示未按下 */
static int voice_long_done=0;           /* 本次按下已触发 2 秒长按启动（防重复） */
static void voice_toggle(){
  if(!voice_recording){
    voice_pid=fork();
    if(voice_pid==0){
      /* 主线(6.x)：mic 路由（v52 mic-capture 验证链）+ tinycap 录音。
       * -t 60 只是上限（防止无 -t 时的数据异常，8/21 实测无 -t 录全零），
       * 实际靠短按 SIGINT 停止（tinycap handler 写头，数据保留）。 */
      execl("/bin/sh","sh","-c",
        "tinymix -D 0 set 'AIF1_CAP Mixer SLIM TX3' 0;"
        "tinymix -D 0 set 'SLIM TX3 MUX' ZERO;"
        "tinymix -D 0 set 'SLIM TX4 MUX' DEC4;"
        "tinymix -D 0 set 'ADC MUX4' AMIC;"
        "tinymix -D 0 set 'AMIC MUX4' ADC4;"
        "tinymix -D 0 set 'ADC4 Volume' 12;"
        "tinymix -D 0 set 'AIF1_CAP Mixer SLIM TX4' 1;"
        "tinymix -D 0 set 'MultiMedia3 Mixer SLIMBUS_0_TX' 0;"
        "tinymix -D 0 set 'MultiMedia1 Mixer SLIMBUS_0_TX' 1;"
        "exec tinycap /tmp/voice.wav -D 0 -d 0 -c 1 -r 48000 -b 16 -p 480 -n 8 -t 60",
        NULL);
      _exit(1);
    }
    voice_recording=1;vibe(80);
  }else{
    if(voice_pid>0)kill(voice_pid,SIGINT);  /* 短按：SIGINT 让 tinycap 写头退出 */
    voice_pid=-1;voice_recording=0;vibe(80);
    usleep(300000);                          /* 等 tinycap 写完 header */
    pid_t p=fork();                          /* 回放：pcm-wav 设备2 扬声器 */
    if(p==0){execl("/bin/sh","sh","-c",
      "tinymix -D 0 set 'QUAT_MI2S_RX Audio Mixer MultiMedia3' 1;"
      "exec pcm-wav -D 0 -d 2 -v 100 /tmp/voice.wav",NULL);_exit(1);}
  }
  draw_statusbar();
}

/* Only write to PTY; shell ECHO comes back via the read path and is drawn once. */
void sh_input(const char*s){
  /* 打字时自动回到底部（libtsm scrollback reset） */
  if(cur_scr)tsm_screen_sb_reset(cur_scr);
  int n=-1;if(cur_fd>=0)n=write(cur_fd,s,strlen(s));
  fb_log("[sh_input fd=");char buf[16];
  int len=snprintf(buf,sizeof(buf),"%d",cur_fd);fb_log(buf);
  fb_log(" n=");
  len=snprintf(buf,sizeof(buf),"%d",n);fb_log(buf);
  fb_log(" bytes:");
  for(int i=0;s[i]&&i<8;i++){snprintf(buf,sizeof(buf)," %02x",(u8)s[i]);fb_log(buf);}
  fb_log("]\n");
}

// --- libtsm 集成（替代自写解析器） ---

/* vte 需要回写客户端（如响应 DSR/DA 查询），写回 PTY master */
static void vte_write_cb(struct tsm_vte *vte, const char *u8, size_t len, void *data){
  (void)vte;(void)data;
  if(cur_fd>=0){ssize_t w=write(cur_fd,u8,len);(void)w;}
}

static void tsm_setup(void){
  for(int i=0;i<MAX_TABS;i++){tabs[i].fd=-1;tabs[i].pid=-1;tabs[i].scr=NULL;tabs[i].vte=NULL;}
  start_tab(0);  /* 默认开 1 个标签 */
  if(tabs[0].fd<0)return;
  fb_logf("tsm_setup: tab0 fd=%d scr=%p\n", tabs[0].fd, (void*)tabs[0].scr);
  fb_log("tsm_setup: done\n");
}

static int render_cell(struct tsm_screen *con, uint64_t id,
                       const uint32_t *ch, size_t len, unsigned int width,
                       unsigned int posx, unsigned int posy,
                       const struct tsm_screen_attr *attr, tsm_age_t age, void *data){
  (void)con;(void)id;(void)age;(void)data;
  uint8_t fr=attr->fr,fg=attr->fg,fb=attr->fb,br=attr->br,bg=attr->bg,bb=attr->bb;
  if(attr->inverse){uint8_t t;t=fr;fr=br;br=t;t=fg;fg=bg;bg=t;t=fb;fb=bb;bb=t;}
  u32 fgc=0xFF000000|((u32)fr<<16)|((u32)fg<<8)|fb;
  u32 bgc=0xFF000000|((u32)br<<16)|((u32)bg<<8)|bb;
  int x=posx*CHAR_W, y=(posy+SBROWS)*CHAR_H;
  if(!len){fill(x,y,CHAR_W*width,CHAR_H,bgc);return 0;}
  /* 双宽字符越界兜底：width>1 且放不下时只清背景，不画（防 framebuffer 溢出到下一行） */
  if(width>1 && posx+width>COLS){fill(x,y,CHAR_W*width,CHAR_H,bgc);return 0;}
  u32 cp=ch[0];
  if(cp<0x80)dchar(x,y,(char)cp,fgc,bgc);
  else draw_cjk(x,y,cp,fgc,bgc);
  return 0;
}
/* 收/展切换：改 kbrows → crows 变 → resize 所有 tab 的 tsm_screen → 重画当前标签 */
static void kb_toggle_collapse(){
  kb_collapsed=!kb_collapsed;
  kbrows=kb_collapsed?KBROWS_COLLAPSED:KBROWS_EXPANDED;
  crows=TROWS-SBROWS-kbrows;
  for(int i=0;i<MAX_TABS;i++){
    if(!tabs[i].scr)continue;
    tsm_screen_resize(tabs[i].scr,COLS,crows);   /* libtsm 保留内容、重新布局 */
    if(tabs[i].fd>=0){
      struct winsize ws={crows,COLS,crows*char_h,COLS*char_w};
      ioctl(tabs[i].fd,TIOCSWINSZ,&ws);
      if(tabs[i].pid>0)kill(tabs[i].pid,SIGWINCH);
    }
    if(i==cur_tab)tsm_screen_draw(tabs[i].scr,render_cell,NULL);  /* 当前标签整屏重画 */
  }
  draw_kb();  /* 键盘背景+键帽（收：黑底；展：浅灰+键帽） */
  do_pan();   /* DRM 推屏 */
}

// --- Status bar: "Agent OS" + WiFi IP + battery (dynamically refreshed) ---
static int get_wlan_ip(char*buf,int n){
  int fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return -1;
  struct ifreq ifr;memset(&ifr,0,sizeof(ifr));strncpy(ifr.ifr_name,"wlan0",IFNAMSIZ-1);
  int r=ioctl(fd,SIOCGIFADDR,&ifr);close(fd);
  if(r<0)return -1;
  struct sockaddr_in*sin=(struct sockaddr_in*)&ifr.ifr_addr;
  const char*s=inet_ntoa(sin->sin_addr);if(!s)return -1;
  snprintf(buf,n,"%s",s);return 0;
}
/* Wi-Fi 在 recovery 启动后才完成关联。异步校时不能堵塞触摸主循环；
 * 成功后同时更新 sda15 的持久文件，下一次启动可立即恢复而不会回到 1970。 */
static int time_sync_started=0;
static void sync_time_async(void){
  if(time_sync_started)return;
  char ip[32];if(get_wlan_ip(ip,sizeof(ip))<0)return;
  pid_t pid=fork();
  if(pid<0)return;
  time_sync_started=1;
  if(pid==0){
    execl("/bin/sh","sh","-c",
          "busybox rdate -s time.nist.gov && date +%s > /newroot/root/.last_time",
          (char*)NULL);
    _exit(127);
  }
}
static void draw_statusbar(){
  fill(0,0,w,SB_H,st_bg);
  int sy=(SB_H-40)/2;   /* 20x40 状态栏字垂直居中（栏高固定 48px，不随终端字号） */
  int x=6;
  /* 标签指示 [n/4]（主题色，当前标签高亮） */
  char tabl[8];snprintf(tabl,sizeof(tabl),"[%d/%d]",cur_tab+1,MAX_TABS);
  for(char*p=tabl;*p;p++,x+=22)draw_status_char(x,sy,*p,st_tab,st_bg);
  /* 三段式模式指示（黄）：P=性能 I=均衡 S=省电 */
  draw_status_char(x,sy,tri_lbl[tri_mode][0],st_yellow,st_bg);x+=22;
  x+=14;
  const char*title="Agent OS";
  for(const char*p=title;*p;p++,x+=22)draw_status_char(x,sy,*p,st_fg,st_bg);
  x+=12;
  if(voice_recording){const char*rec="*REC";for(const char*p=rec;*p;p++,x+=22)draw_status_char(x,sy,*p,st_red,st_bg);x+=12;}
  char ip[32]={0};
  if(get_wlan_ip(ip,sizeof(ip))==0){for(char*p=ip;*p;p++,x+=22)draw_status_char(x,sy,*p,st_ip,st_bg);}
  else{const char*noip="no-wifi";for(const char*p=noip;*p;p++,x+=22)draw_status_char(x,sy,*p,st_noip,st_bg);}
  /* 右：日期+时间（26-8-17 19:54）+ 电量 */
  char tstr[24]={0};
  {time_t now=time(NULL);struct tm*tm=localtime(&now);
   if(tm)strftime(tstr,sizeof(tstr),"%y-%-m-%-d %H:%M",tm);}
  int tlen=strlen(tstr);

  char bat[16]={0};rdfs("/sys/class/power_supply/battery/capacity",bat,16);
  if(!bat[0])rdfs("/sys/class/power_supply/bq27541-0/capacity",bat,16);  /* 主线电量计 */
  char st[16]={0};rdfs("/sys/class/power_supply/battery/status",st,16);
  if(!st[0])rdfs("/sys/class/power_supply/bq27541-0/status",st,16);
  int charging=(strstr(st,"Charging")||strstr(st,"Full"))?1:0;
  int blen=strlen(bat)+3+(charging?1:0);  /* [xx%] = len+3, [xx%+] = len+4 */
  int rx=1080-blen*22-6;
  int tx=rx-tlen*22-14;   /* 时间在电量左边 */
  for(int i=0;i<tlen;i++)draw_status_char(tx+i*22,sy,tstr[i],st_time,st_bg);
  draw_status_char(rx,sy,'[',st_ip,st_bg);rx+=22;
  if(bat[0]){for(char*p=bat;*p;p++,rx+=22)draw_status_char(rx,sy,*p,st_ip,st_bg);}
  draw_status_char(rx,sy,'%',st_ip,st_bg);rx+=22;
  if(charging){draw_status_char(rx,sy,'+',st_yellow,st_bg);rx+=22;}
  draw_status_char(rx,sy,']',st_ip,st_bg);
}

/* ============ 主题配色 + 终端字号（theme / fontsize 命令，2026-08-17） ============ */
typedef struct{
  const char*name;
  u32 fg,bg;   /* 终端前景/背景（ARGB，写入 palette FOREGROUND/BACKGROUND） */
  u32 kb_bg,kb_key,kb_spec,kb_txt,kb_press,kb_press_s;   /* 键盘 */
  u32 st_bg,st_fg,st_tab,st_ip,st_time,st_yellow,st_red,st_noip;  /* 状态栏 */
}Theme;
static Theme themes[]={
  /* 名字      终端fg      终端bg       键盘bg      键         特殊       文字       按-普通     按-特殊    状态栏... */
  {"default", 0x00FFFFFF,0x00000000, 0xFFD1D1D6,0xFFFFFFFF,0xFFA8A8AD,0xFF1A1A1A,0xFFD1D1D6,0xFF8E8E93, 0xFF001122,0xFFFFFFFF,0xFFFFAA00,0xFF00FF00,0xFF00FFFF,0xFFFFFF00,0xFFFF3030,0xFFFF8800},
  {"green",   0x0000FF00,0x00000000, 0xFFD1D1D6,0xFFFFFFFF,0xFFA8A8AD,0xFF1A1A1A,0xFFD1D1D6,0xFF8E8E93, 0xFF001122,0xFF00FF00,0xFFFFAA00,0xFF00FF00,0xFF00FFFF,0xFFFFFF00,0xFFFF3030,0xFFFF8800},
  {"dark",    0xFFE8E8E8,0xFF141414, 0xFF2C2C2E,0xFF3A3A3C,0xFF48484A,0xFFE8E8E8,0xFF3A3A3C,0xFF5A5A5E, 0xFF1C1C1E,0xFFE8E8E8,0xFFFFB340,0xFF4CD964,0xFF5AC8FA,0xFFFFCC44,0xFFFF5252,0xFFFF8A65},
  {"light",   0xFF1A1A1A,0xFFF2F2F2, 0xFFD1D1D6,0xFFFFFFFF,0xFFA8A8AD,0xFF1A1A1A,0xFFD1D1D6,0xFF8E8E93, 0xFFE0E0E0,0xFF1A1A1A,0xFFB25D00,0xFF007A00,0xFF007A9C,0xFF9A6B00,0xFFCC2222,0xFFB25D00},
  {"amber",   0x00FFB300,0x00000000, 0xFFD1D1D6,0xFFFFFFFF,0xFFA8A8AD,0xFF1A1A1A,0xFFD1D1D6,0xFF8E8E93, 0xFF001122,0xFFFFB300,0xFFFFAA00,0xFF00FF00,0xFF00FFFF,0xFFFFFF00,0xFFFF3030,0xFFFF8800},
};
#define NUM_THEMES ((int)(sizeof(themes)/sizeof(themes[0])))
static int cur_theme=0;

static int read_theme_file(void){
  int fd=open("/root/theme",O_RDONLY);if(fd<0)return cur_theme;
  char b[32]={0};int n=read(fd,b,31);close(fd);
  if(n<=0)return cur_theme;
  for(int t=0;t<NUM_THEMES;t++){ if(strncmp(b,themes[t].name,strlen(themes[t].name))==0)return t; }
  return cur_theme;
}
static int read_fontsize_file(void){
  int fd=open("/root/fontsize",O_RDONLY);if(fd<0)return 0;
  char b[8]={0};int n=read(fd,b,7);close(fd);
  if(n<=0)return 0;
  int v=atoi(b);if(v<14||v>36)return 0;
  return v;
}
/* 主线的 / 是 initramfs，真正持久化的 root 在 sda15 的 /newroot。
 * RTC 不能写入，故 recovery 启动时先从上次 NTP 时间恢复，避免标题栏回到 1970。 */
static void restore_saved_time(void){
  const char*paths[]={"/newroot/root/.last_time","/root/.last_time",NULL};
  for(int i=0;paths[i];i++){
    int fd=open(paths[i],O_RDONLY);if(fd<0)continue;
    char b[32]={0};int n=read(fd,b,sizeof(b)-1);close(fd);
    if(n<=0)continue;
    long long saved=strtoll(b,NULL,10);
    if(saved<1577836800LL)continue;  /* 拒绝无效/1970 时间 */
    time_t now=time(NULL);
    if(saved>now){
      struct timespec ts={.tv_sec=(time_t)saved,.tv_nsec=0};
      if(clock_settime(CLOCK_REALTIME,&ts)==0)fb_log("time restored from saved file\n");
    }
    return;
  }
}
/* 应用主题：palette fg/bg + 键盘色 + 状态栏色 + 全量重画 */
static void apply_theme(int t){
  if(t<0||t>=NUM_THEMES)t=0;
  cur_theme=t;Theme*th=&themes[t];
  kb_bg=th->kb_bg;kb_key=th->kb_key;kb_spec=th->kb_spec;kb_txt=th->kb_txt;kb_press=th->kb_press;kb_press_s=th->kb_press_s;
  st_bg=th->st_bg;st_fg=th->st_fg;st_tab=th->st_tab;st_ip=th->st_ip;st_time=th->st_time;st_yellow=th->st_yellow;st_red=th->st_red;st_noip=th->st_noip;
  tpalette[TSM_COLOR_FOREGROUND][0]=(th->fg>>16)&0xFF;tpalette[TSM_COLOR_FOREGROUND][1]=(th->fg>>8)&0xFF;tpalette[TSM_COLOR_FOREGROUND][2]=th->fg&0xFF;
  tpalette[TSM_COLOR_BACKGROUND][0]=(th->bg>>16)&0xFF;tpalette[TSM_COLOR_BACKGROUND][1]=(th->bg>>8)&0xFF;tpalette[TSM_COLOR_BACKGROUND][2]=th->bg&0xFF;
  for(int i=0;i<MAX_TABS;i++)if(tabs[i].vte)vte_palette(tabs[i].vte);
  draw_statusbar();
  if(cur_scr)tsm_screen_draw(cur_scr,render_cell,NULL);
  draw_kb();do_pan();
}
/* 应用字号：改 char_w/char_h → 布局重算 → resize 所有 tab + winsize → 全量重画 */
static void apply_fontsize(int fw){
  if(fw<14)fw=14;if(fw>36)fw=36;
  char_w=fw;char_h=fw*2;
  kbrows=(KB_H_PX+char_h-1)/char_h;if(kbrows<2)kbrows=2;
  crows=TROWS-SBROWS-kbrows;if(crows<4)crows=4;
  for(int i=0;i<MAX_TABS;i++){
    if(!tabs[i].scr)continue;
    tsm_screen_resize(tabs[i].scr,COLS,crows);
    if(tabs[i].fd>=0){
      struct winsize ws={crows,COLS,crows*char_h,COLS*char_w};
      ioctl(tabs[i].fd,TIOCSWINSZ,&ws);
      if(tabs[i].pid>0)kill(tabs[i].pid,SIGWINCH);
    }
  }
  draw_statusbar();
  if(cur_scr)tsm_screen_draw(cur_scr,render_cell,NULL);
  draw_kb();do_pan();
}

/* Reacquire DRM only after the browser supervisor has removed its active flag.
 * This reuses the live PTY/libtsm screens and redraws the recovery UI so the
 * browser session returns to the same console instead of starting a second
 * recovery process. */
static int restore_framebuffer(void){
  if(open_framebuffer()<0)return -1;
  fill(0,0,w,h,0xFF000000);
  draw_statusbar();
  if(cur_scr)tsm_screen_draw(cur_scr,render_cell,NULL);
  draw_kb();draw_ui_overlay();
  for(int i=0;i<30;i++){do_pan();usleep(20000);}
  fb_log("DRM display restored after browser\n");
  return 0;
}

// Touch: auto-detect + ABS range scaling to screen pixels
static int ts_x_min=0,ts_x_max=1080,ts_y_min=0,ts_y_max=1920;
static int scale_x(int raw){
  if(ts_x_max<=ts_x_min)return raw;
  long v=(long)(raw-ts_x_min)*1080/(ts_x_max-ts_x_min);
  if(v<0)v=0;if(v>1079)v=1079;
  return (int)v;
}
static int scale_y(int raw){
  if(ts_y_max<=ts_y_min)return raw;
  long v=(long)(raw-ts_y_min)*1920/(ts_y_max-ts_y_min);
  if(v<0)v=0;if(v>1919)v=1919;
  return (int)v;
}

// --- scrollback view: render history window, swipe to scroll ---
static int touch_swipe=0;
static int swipe_last_y=0;
static int swipe_acc=0;
static void render_scrollback(){
  for(int r=0;r<CROWS;r++){
    int hrow=sb_count-sb_view+r;
    for(int c=0;c<COLS;c++){
      u32 cp=0x20;u32 fg=cur_fg,bg=cur_bg;
      if(hrow>=0&&hrow<sb_count){cp=sb[hrow][c];fg=sb_fg[hrow][c];bg=sb_bg[hrow][c];}
      else if(hrow>=sb_count){int live=hrow-sb_count;if(live<CROWS){cp=scr[live][c];fg=scr_fg[live][c];bg=scr_bg[live][c];}}
      if(cp==0)continue;  /* wide-char continuation cell */
      if(cp<0x80)dchar(c*CHAR_W,(r+SBROWS)*CHAR_H,(char)cp,fg,bg);
      else draw_cjk(c*CHAR_W,(r+SBROWS)*CHAR_H,cp,fg,bg);
    }
  }
}
static void sb_scroll_up(int n){
  if(!cur_scr)return;
  tsm_screen_sb_up(cur_scr,n);
  tsm_screen_draw(cur_scr,render_cell,NULL);do_pan();
}
static void sb_scroll_down(int n){
  if(!cur_scr)return;
  tsm_screen_sb_down(cur_scr,n);
  tsm_screen_draw(cur_scr,render_cell,NULL);do_pan();
}
/* 一次提交多个滚动行。主线 DRM 的 do_pan() 会走 DIRTYFB 原子提交，
 * 因此不能在同一触摸帧里每移动一行就单独重绘。 */
static void sb_scroll_by(int rows){
  if(!cur_scr||rows==0)return;
  if(rows>0)tsm_screen_sb_up(cur_scr,rows);
  else tsm_screen_sb_down(cur_scr,-rows);
  tsm_screen_draw(cur_scr,render_cell,NULL);do_pan();
}

// --- 选择/复制/粘贴（libtsm selection API + 触摸拖拽） ---
/* 触摸像素坐标 → 屏幕 cell 坐标（含状态栏 SBROWS 行偏移） */
static void touch_to_cell(int tx,int ty,int*cellx,int*celly){
  int cx=tx/CHAR_W, cy=(ty-SBROWS*CHAR_H)/CHAR_H;
  if(cx<0)cx=0;if(cx>=COLS)cx=COLS-1;
  if(cy<0)cy=0;if(cy>=crows)cy=crows-1;
  *cellx=cx;*celly=cy;
}
static void sel_start(int cellx,int celly){
  if(!cur_scr)return;
  sel_last_cx=cellx;sel_last_cy=celly;
  tsm_screen_selection_start(cur_scr,(unsigned)cellx,(unsigned)celly);
  tsm_screen_draw(cur_scr,render_cell,NULL);do_pan();
}
static void sel_update(int tx,int ty){
  if(!selecting)return;
  int cx,cy;touch_to_cell(tx,ty,&cx,&cy);
  if(cx==sel_last_cx&&cy==sel_last_cy)return;
  sel_last_cx=cx;sel_last_cy=cy;
  if(cur_scr)tsm_screen_selection_target(cur_scr,(unsigned)cx,(unsigned)cy);
  tsm_screen_draw(cur_scr,render_cell,NULL);do_pan();
}
static void sel_copy_and_clear(){
  if(!cur_scr)return;
  char*sel=NULL;
  /* 注意：tsm_screen_selection_copy 成功返回复制字节数(>=0)，失败返回负数，
   * 不是返回 0 表成功！之前写成 ==0 导致复制非空内容被误判失败清空剪贴板 */
  int r=tsm_screen_selection_copy(cur_scr,&sel);
  if(r>=0 && sel){
    strncpy(clipboard,sel,CLIP_MAX-1);clipboard[CLIP_MAX-1]=0;
    clip_len=strlen(clipboard);
    free(sel);
  }else{clip_len=0;clipboard[0]=0;}
  tsm_screen_selection_reset(cur_scr);
  tsm_screen_draw(cur_scr,render_cell,NULL);do_pan();
  vibe(40);
}

// --- power key: short press toggles screen (backlight on/off) ---
static int bl_fd=-1;
static const char* bl_path=NULL;   /* 实际使用的背光路径 */
static int last_bl=102;   /* 息屏前亮度，唤醒恢复（默认 40%=102，与开机默认一致） */
static int poll_timeout=20;  /* 主循环 poll 超时（ms）；息屏降载时拉长 */
static void bl_open(){
  if(bl_fd>=0)return;
  /* 3.18: leds/lcd-backlight；主线(6.x): DRM backlight（994000.dsi.0）——按序探测 */
  const char* paths[]={"/sys/class/leds/lcd-backlight/brightness",
                       "/sys/class/backlight/994000.dsi.0/brightness",NULL};
  for(int i=0;paths[i];i++){
    bl_fd=open(paths[i],O_RDWR);
    if(bl_fd<0)bl_fd=open(paths[i],O_WRONLY);
    if(bl_fd>=0){bl_path=paths[i];break;}
  }
}
static int bl_read(){
  if(!bl_path)return -1;
  int rfd=open(bl_path,O_RDONLY);
  if(rfd<0)return -1;
  char b[16]={0};int n=read(rfd,b,15);close(rfd);
  if(n>0)return atoi(b);
  return -1;
}
static void screen_toggle(){
  bl_open();
  if(bl_fd<0)return;
  if(screen_on){  /* 息屏：记住亮度 + 背光 0 + 降载（userspace 最低频 + poll 拉长） */
    int v=bl_read();
    if(v>0)last_bl=v;
    const char*zero="0\n";
    write(bl_fd,zero,strlen(zero));
    /* 降载：所有核切 userspace + 最低频，主循环 poll 拉长到 500ms */
    for(int c=0;c<4;c++){
      char p[64],sp[64];
      snprintf(p,sizeof(p),"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor",c);
      snprintf(sp,sizeof(sp),"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed",c);
      int fd=open(p,O_WRONLY);if(fd>=0){write(fd,"userspace",9);close(fd);}
      fd=open(sp,O_WRONLY);if(fd>=0){write(fd,"307200",6);close(fd);}
    }
    poll_timeout=500;
  }else{  /* 唤醒：恢复息屏前亮度 + 恢复档位 governor + poll 恢复 20ms */
    char b[16];snprintf(b,sizeof(b),"%d\n",last_bl);
    write(bl_fd,b,strlen(b));
    set_gov(tri_gov[tri_mode]);
    poll_timeout=20;
  }
  screen_on=!screen_on;
}

/* 按设备名字串匹配 input 设备（主线设备名：pwrkey/gpio-keys/s1302/haptics…） */
static int open_input_by_name(const char* substr){
  for(int i=0;i<10;i++){char p[32];sprintf(p,"/dev/input/event%d",i);
    int f=open(p,O_RDONLY|O_NONBLOCK);if(f<0)continue;
    char n[256]={0};ioctl(f,EVIOCGNAME(sizeof(n)-1),n);
    for(char*c=n;*c;c++)*c=tolower(*c);
    if(strstr(n,substr))return f;
    close(f);}
  return -1;
}
static int open_touch(){
  for(int i=0;i<10;i++){char p[32];sprintf(p,"/dev/input/event%d",i);
    int f=open(p,O_RDONLY);if(f<0)continue;
    char n[256]={0};ioctl(f,EVIOCGNAME(sizeof(n)-1),n);
    for(char*c=n;*c;c++)*c=tolower(*c);
    if(strstr(n,"synaptics")||strstr(n,"touch")||strstr(n,"fts")){
      struct input_absinfo ax,ay;
      if(ioctl(f,EVIOCGABS(ABS_MT_POSITION_X),&ax)==0 && ax.maximum>ax.minimum){ts_x_min=ax.minimum;ts_x_max=ax.maximum;}
      if(ioctl(f,EVIOCGABS(ABS_MT_POSITION_Y),&ay)==0 && ay.maximum>ay.minimum){ts_y_min=ay.minimum;ts_y_max=ay.maximum;}
      fcntl(f,F_SETFL,fcntl(f,F_GETFL)|O_NONBLOCK);return f;
    }
    close(f);}
  return open("/dev/input/event3",O_RDONLY|O_NONBLOCK);
}

int main(){
  signal(SIGCHLD,SIG_IGN);  /* auto-reap shell child */
  setenv("TZ","CST-8",1);tzset();  /* 中国时区 UTC+8，状态栏时间用本地时间 */
  restore_saved_time();
  vibe_init();vibe(500);
  unlink(BROWSER_SESSION_READY);
  if(open_framebuffer()<0)return 1;
  /* recovery_drm_open() has already selected DSI-1's preferred mode, created
   * the XRGB8888 dumb buffer, and performed the initial legacy KMS modeset. */
  fill(0,0,w,h,0xFF000000);
  char bat[16]={0};rdfs("/sys/class/power_supply/battery/capacity",bat,16);
  draw_statusbar();
  tsm_setup();
  {  /* 开机恢复上次三段式模式（持久化文件；重启后拨动滑块立即纠正） */
    int m=1,fd=open("/root/tri_mode",O_RDONLY);
    if(fd>=0){char b[8]={0};int n=read(fd,b,7);if(n>0)m=atoi(b);close(fd);}
    apply_tri_mode(m);
  }
  {  /* 开机恢复主题 + 终端字号（/root/theme、/root/fontsize，theme/fontsize 命令持久化） */
    apply_theme(read_theme_file());
    int fw=read_fontsize_file();if(fw>0)apply_fontsize(fw);
  }
  ime_update_kb_labels();draw_kb();
  /* 启动消息通过 libtsm 显示 */
  {char msg[512];int ml;
   ml=snprintf(msg,sizeof(msg),"============================================\r\n  Agent OS Console v0.5\r\n");
   tsm_vte_input(cur_vte,msg,ml);
   char kver[128]={0};rdfs("/proc/version",kver,128);
   ml=snprintf(msg,sizeof(msg),"  Kernel: %s\r\n",kver);tsm_vte_input(cur_vte,msg,ml);
   char tmp[16]={0};rdfs("/sys/class/thermal/thermal_zone0/temp",tmp,16);int tc=tmp[0]?atoi(tmp)/10:0;
   ml=snprintf(msg,sizeof(msg),"  Battery: %s%% | Temp: %dC\r\n",bat,tc);tsm_vte_input(cur_vte,msg,ml);
   tsm_vte_input(cur_vte,"============================================\r\n",46);
   FILE*f=popen("dmesg 2>&1 | tail -n 20","r");
   if(f){char lb[256];while(fgets(lb,256,f))tsm_vte_input(cur_vte,lb,strlen(lb));pclose(f);}
   tsm_vte_input(cur_vte,"--- ready ---\r\n",14);
  }
  tsm_screen_draw(cur_scr,render_cell,NULL);
  /* Submit a few full frames while the DSI command-mode panel settles. */
  for(int i=0;i<3;i++){do_pan();usleep(20000);}

  struct input_event ev;int ts_fd=open_touch(),tx=-1,ty=-1;
  int pending_press=0,pending_release=0;
  int selection_motion=0;  /* 本 SYN_REPORT 帧内有新的选择坐标 */
  int swipe_rows=0;        /* 本 SYN_REPORT 帧内累计的历史滚动行数 */
  /* 主线(6.x) input 设备名与 3.18 完全不同（event 编号也变）——按名字动态匹配：
   *   电源 = pm8941_pwrkey（event2）  三段式+音量 = gpio-keys（event5，同一设备双 fd）
   *   电容键 = op3-capkey-s1302（event3）  haptics=spmi_haptics（event0） */
  int pw_fd=open_input_by_name("pwrkey");
  int tri_fd=open_input_by_name("gpio-keys");  /* 三段式 600/601/602 */
  int vol_fd=open_input_by_name("gpio-keys");  /* 音量 115/114（独立 fd 各自排队） */
  int cap_fd=open_input_by_name("s1302");      /* 左580中英 右158回车 */
  long long cap_press[2]={0,0};   /* 电容键按下时间：0=左 1=右 */
  int cap_done[2]={0,0};
  struct pollfd fds[5+MAX_TABS];
  long long last_status=now_ms();
  int browser_was_active=0;

  while(1){
    int browser_active=browser_session_active();
    if(browser_active&&!browser_was_active){
      /* Do not let Weston race an open DRM client. browser-session waits for
       * this marker before it starts Weston/DRM. */
      release_framebuffer();
      mark_browser_ready();
    }
    if(browser_was_active && !browser_active){
      unlink(BROWSER_SESSION_READY);
      if(restore_framebuffer()<0)fb_log("fb restore failed; recovery UI is unavailable\n");
      /* Discard touch/key events generated for the browser before handing
       * input ownership back to recovery.  Otherwise the browser's final
       * touch release can become a phantom recovery click. */
      struct input_event stale;
      while(read(ts_fd,&stale,sizeof(stale))==sizeof(stale)){}
      while(read(pw_fd,&stale,sizeof(stale))==sizeof(stale)){}
      while(read(tri_fd,&stale,sizeof(stale))==sizeof(stale)){}
      while(read(vol_fd,&stale,sizeof(stale))==sizeof(stale)){}
      while(read(cap_fd,&stale,sizeof(stale))==sizeof(stale)){}
      pending_press=0;pending_release=0;selection_motion=0;swipe_rows=0;
      touch_swipe=0;selecting=0;tx=ty=-1;
      cap_press[0]=cap_press[1]=0;cap_done[0]=cap_done[1]=0;
    }
    browser_was_active=browser_active;
    /* 每次循环重建 poll 表：新建的标签 fd 才能被监听到（多标签关键）
     * 主线内核(6.x)适配：tri/vol/cap 的 input 设备可能不存在(fd=-1)——
     * 仍无条件加入 poll 表（poll 忽略 fd<0 的条目，revents=0），
     * 保持 0=ts 1=pw 2=tri 3=vol 4=cap 5+=tab 的索引固定，处理代码 fds[5+i] 才能对上。 */
    int nfds=0;
    memset(fds,0,sizeof(fds));
    fds[nfds].fd=ts_fd;fds[nfds].events=POLLIN;nfds++;
    fds[nfds].fd=pw_fd;fds[nfds].events=POLLIN;nfds++;
    fds[nfds].fd=tri_fd;fds[nfds].events=POLLIN;nfds++;
    fds[nfds].fd=vol_fd;fds[nfds].events=POLLIN;nfds++;
    fds[nfds].fd=cap_fd;fds[nfds].events=POLLIN;nfds++;
    for(int i=0;i<MAX_TABS;i++){if(tabs[i].fd>=0){fds[nfds].fd=tabs[i].fd;fds[nfds].events=POLLIN;nfds++;}}
    if(poll(fds,nfds,poll_timeout)>0){
      if(!browser_active && fds[0].revents&POLLIN)while(read(ts_fd,&ev,sizeof(ev))==sizeof(ev)){
        if(!screen_on)continue;  /* 息屏：丢弃触摸事件（电源键在 fds[2] 不受影响，仍能唤醒） */
        if(ev.type==EV_ABS){
          if(ev.code==ABS_MT_POSITION_X||ev.code==ABS_X){tx=scale_x(ev.value);if(selecting)selection_motion=1;}
          if(ev.code==ABS_MT_POSITION_Y||ev.code==ABS_Y){
            int new_ty=scale_y(ev.value);
            if(touch_swipe){swipe_acc+=(new_ty-swipe_last_y);swipe_last_y=new_ty;
              while(swipe_acc>=CHAR_H){swipe_acc-=CHAR_H;swipe_rows++;}
              while(swipe_acc<=-CHAR_H){swipe_acc+=CHAR_H;swipe_rows--;}}
            ty=new_ty;
            if(selecting)selection_motion=1;
          }
          if(ev.code==ABS_MT_TRACKING_ID){  /* type B: -1=release, else=press */
            if(ev.value==-1)pending_release=1;
            else pending_press=1;
          }
        }
        if(ev.type==EV_KEY&&(ev.code==BTN_TOUCH||ev.code==BTN_TOOL_FINGER)){
          if(ev.value==1)pending_press=1;
          else pending_release=1;
        }
        if(ev.type==EV_SYN&&ev.code==SYN_REPORT){
          /* 合并同一触摸帧的 X/Y 更新为一次选择渲染和一次 DRM 提交。 */
          if(selection_motion){selection_motion=0;sel_update(tx,ty);}
          if(swipe_rows){sb_scroll_by(swipe_rows);swipe_rows=0;}
          if(pending_press){  /* frame complete: coords are final */
            pending_press=0;vibe(30);
            /* 弹层打开时优先接管触摸：避免搜索/面板操作穿透到终端滚动或键盘。 */
            if(ui_mode){
              if(ui_mode==UI_PALETTE && ty>=SB_H+72 && ty<SB_H+72+8*66){
                int action=(ty-(SB_H+72))/66;ui_palette_action(action);
              }else if(ty>=KB_Y){
                int fi=kb_find_fn(tx,ty),ki=kb_find(tx,ty);
                if(fi>=0)ui_key(keys_fn[fi].out,keys_fn[fi].func);
                else if(ki>=0)ui_key(kkeys[ki].out,kkeys[ki].func);
              }
              draw_kb();draw_ui_overlay();do_pan();
              goto touch_press_done;
            }
            if(ty<KB_Y){  /* 屏幕区：选择模式拖拽选择，否则滑动看历史 */
              if(select_mode){
                int cx,cy;touch_to_cell(tx,ty,&cx,&cy);
                selecting=1;sel_start(cx,cy);
              }else{
                touch_swipe=1;swipe_last_y=ty;swipe_acc=0;
              }
            }else{
              if(kb_collapsed){  /* 缩进态：只响应展开按钮 */
                if(kb_find_collapse(tx,ty)==KF_COLLAPSE)kb_toggle_collapse();
              }else{
              int ci=kb_find_cand(tx,ty);
              if(ci>=0){  /* 候选词栏：点选候选上屏 */
                py_commit(py_cands[ci]);
                draw_kb();do_pan();
              }else if(ci==-2){  /* 上一页 */
                py_page--;draw_cand();do_pan();
              }else if(ci==-3){  /* 下一页 */
                py_page++;draw_cand();do_pan();
              }else{
              int fi=kb_find_fn(tx,ty);
              if(fi>=0){  /* 底部功能键行 */
                KbKey*k=&keys_fn[fi];
                switch(k->func){
                  case KF_LANG: ime_cn=!ime_cn;py_len=0;py_buf[0]=0;py_ncand=0;ime_update_kb_labels();draw_kb();break;
                  case KF_CTRL: ctrl_active=!ctrl_active;draw_kb();break;
                  case KF_TAB: sh_input("\t");break;
                  case KF_ESC: sh_input("\x1b");break;
                  case KF_LEFT: sh_input("\x1b[D");break;
                  case KF_RIGHT: sh_input("\x1b[C");break;
                  case KF_SLASH: sh_input("/");break;   /* / 键（功能行新增） */
                  case KF_DOT: sh_input(".");break;     /* . 键（功能行新增） */
                  case KF_COLLAPSE: kb_toggle_collapse();break;
                  case KF_VOICE:
                    if(voice_recording){voice_toggle();voice_press_start=0;voice_long_done=0;}  /* 录音中：轻按即停止并回放 */
                    else{voice_press_start=now_ms();voice_long_done=0;}  /* 未录音：长按 2 秒启动（防误触） */
                    break;
                  case KF_SEL: select_mode=!select_mode;selecting=0;draw_kb();break;   /* 切换选择模式 */
                  case KF_PASTE: if(clip_len>0)sh_input(clipboard);break;              /* 粘贴剪贴板 */
                }
                do_pan();
              }else{
              int ki=kb_find(tx,ty);int pc=0;
              if(ki>=0){KbKey*k=&kkeys[ki];
                switch(k->func){
                  case KF_CHAR:
                    if(ctrl_active){  /* Ctrl combo: 数字1-4切标签，字母→控制字符 */
                      char c=k->out[0],cc=0;
                      if(c=='f'||c=='F'){ui_open_search();}     /* Ctrl+F：搜索 scrollback */
                      else if(c=='p'||c=='P'){ui_open_palette();} /* Ctrl+P：命令面板 */
                      else if(c>='1'&&c<='4'){switch_tab(c-'1');}   /* Ctrl+1..4 切换/新建标签 */
                      else{
                        if(c>='a'&&c<='z')cc=c-'a'+1;
                        else if(c>='A'&&c<='Z')cc=c-'A'+1;
                        else if(c=='[')cc=0x1b;  /* Ctrl+[ = Esc */
                        if(cc){char b[2]={cc,0};sh_input(b);}
                      }
                      ctrl_active=0;draw_kb();
                    }else if(ime_cn && k->out[0]>='a' && k->out[0]<='z'){  /* 中文模式：字母攒拼音 */
                      if(py_len<15){py_buf[py_len++]=k->out[0];py_buf[py_len]=0;}
                      py_search();draw_cand();
                    }else if(k->out[0]){sh_input(k->out);last_kb_bytes=1;}
                    if(kb_page==1){kb_page=0;kkeys=keys_lo;pc=1;}break;
                  case KF_DEL:
                    if(ime_cn && py_len>0){  /* 拼音模式：退格删拼音字母 */
                      py_len--;py_buf[py_len]=0;py_search();draw_cand();
                    }else{
                      /* 统一发 1 次 \x7f（2026-08-17 最终版）：
                         shell（busybox ash FEATURE_EDITING + LANG=C.UTF-8 → lineedit unicode 级）
                         codebuddy/readline/vim/links（raw + unicode 级）—— 全部按字符删 */
                      sh_input("\x7f");
                    }
                    break;
                  case KF_RET:
                    if(ime_cn && py_len>0){  /* 拼音模式：回车直接上屏拼音串（作为英文） */
                      sh_input(py_buf);last_kb_bytes=py_len;py_len=0;py_buf[0]=0;py_ncand=0;draw_cand();
                    }else sh_input("\r");
                    break;  /* 回车发 CR：ICANON(ICRNL)→NL，raw mode 下 codebuddy 期望 CR */
                  case KF_SHIFT: kb_page=(kb_page==0)?1:0;kkeys=(kb_page==0)?keys_lo:keys_up;pc=1;break;
                  case KF_PAGE: {  /* 智能翻页：字母→数字→符号→字母链式 */
                    if(kb_page==0||kb_page==1){kkeys=keys_num;kb_page=2;}
                    else if(kb_page==2){kkeys=keys_sym;kb_page=3;}
                    else{kkeys=keys_lo;kb_page=0;}
                    pc=1;break;
                  }
                  case KF_ABC: kkeys=keys_lo;kb_page=0;pc=1;break;  /* 任意页回字母 */
                  case KF_CTRL: ctrl_active=!ctrl_active;draw_kb();break;
                  case KF_TAB: sh_input("\t");break;
                  case KF_ESC: sh_input("\x1b");break;
                  case KF_LEFT: sh_input("\x1b[D");break;
                  case KF_RIGHT: sh_input("\x1b[C");break;
                  case KF_UP: sh_input("\x1b[A");break;    /* 方向键发给程序（历史滚动改用触摸滑动） */
                  case KF_DOWN: sh_input("\x1b[B");break;
                }
              }
              if(pc){kb_hl=-1;kb_clear();draw_kb();}
              else if(ki!=kb_hl){int old=kb_hl;kb_hl=ki;kb_deadline=now_ms()+150;if(ki>=0)draw_kb_key(ki);if(old>=0)draw_kb_key(old);}
              do_pan();
              }  /* 闭合内层 else (fi 不是功能键) */
              }  /* 闭合候选词栏 else */
            }   /* 闭合 kb_collapsed else */
          }    /* 闭合 ty<KB_Y else */
touch_press_done:
          ; /* C requires a statement after a label before the closing brace. */
          }    /* 闭合 if(pending_press) */
          if(pending_release){
            pending_release=0;
            /* 松开 KF_VOICE 不停止录音：长按 2 秒启动后松开继续录，再轻按停止+回放 */
            if(voice_press_start>0){
              voice_press_start=0;voice_long_done=0;
            }
            if(selecting){selecting=0;sel_last_cx=sel_last_cy=-1;sel_copy_and_clear();select_mode=0;draw_kb();}  /* 松开：复制选中文本并退出选择模式 */
            touch_swipe=0;kb_hl=-1;draw_kb();do_pan();
          }
        }
      }
      if(!browser_active && fds[1].revents&POLLIN){struct input_event pe;while(read(pw_fd,&pe,sizeof(pe))==sizeof(pe)){if(pe.type==EV_KEY&&pe.code==KEY_POWER&&pe.value==1)screen_toggle();}}
      if(!browser_active && fds[2].revents&POLLIN){struct input_event te;while(read(tri_fd,&te,sizeof(te))==sizeof(te)){if(te.type==EV_KEY&&te.value==1)tri_handle(te.code);}}
      if(!browser_active && vol_fd>=0&&(fds[3].revents&POLLIN)){  /* 音量键 = 光标上/下（息屏禁用，仅电源/三段式可用） */
        struct input_event ve;while(read(vol_fd,&ve,sizeof(ve))==sizeof(ve)){
          if(!screen_on)continue;
          if(ve.type==EV_KEY&&ve.value==1){
            if(ve.code==KEY_VOLUMEUP)sh_input("\x1b[A");      /* 音量+ = 光标上 */
            else if(ve.code==KEY_VOLUMEDOWN)sh_input("\x1b[B"); /* 音量- = 光标下 */
          }
        }
      }
      if(!browser_active && cap_fd>=0&&(fds[4].revents&POLLIN)){  /* 电容键：记按下时间（长按 300ms 触发，防误触；息屏禁用） */
        struct input_event ce;while(read(cap_fd,&ce,sizeof(ce))==sizeof(ce)){
          if(!screen_on)continue;
          if(ce.type!=EV_KEY)continue;
          int idx = (ce.code==580)?0 : (ce.code==KEY_BACK)?1 : -1;  /* 实测：左下巴=APPSWITCH(580)，右下巴=BACK(158) */
          if(idx<0)continue;
          if(ce.value==1){cap_press[idx]=now_ms();cap_done[idx]=0;}
          else if(ce.value==0){cap_press[idx]=0;cap_done[idx]=0;}
        }
      }
      for(int i=0;i<MAX_TABS;i++){  /* 多标签：所有存活 tab 的 PTY 都读入各自 libtsm，仅当前 tab 渲染 */
        if(tabs[i].fd<0)continue;
        if(fds[5+i].revents&POLLIN){
          char buf[512];for(;;){int n=read(tabs[i].fd,buf,sizeof(buf)-1);if(n>0){tab_vte_input(i,buf,n);}else if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))break;else break;}
          if(i==cur_tab && !browser_active){tsm_screen_draw(tabs[i].scr,render_cell,NULL);draw_ui_overlay();do_pan();}
        }
      }
    }
    /* KF_VOICE 长按 2 秒自动启动录音（防误触；松开继续录，再轻按停止+回放；息屏禁用） */
    if(!browser_active && screen_on && voice_press_start>0 && !voice_long_done && !voice_recording && now_ms()-voice_press_start>=2000){
      voice_toggle();
      voice_long_done=1;
    }
    /* 下巴电容键长按 300ms 防误触触发：左 APPSWITCH(580)=中英切换，右 BACK(158)=回车（息屏禁用） */
    for(int ci=0;ci<2;ci++){
      if(browser_active){cap_press[0]=cap_press[1]=0;cap_done[0]=cap_done[1]=0;break;}
      if(!screen_on){cap_press[0]=cap_press[1]=0;cap_done[0]=cap_done[1]=0;break;}
      if(cap_press[ci]>0 && !cap_done[ci] && now_ms()-cap_press[ci]>=300){
        cap_done[ci]=1;
        if(ci==0){  /* 左：中英切换（同 KF_LANG） */
          ime_cn=!ime_cn;py_len=0;py_buf[0]=0;py_ncand=0;ime_update_kb_labels();draw_kb();
        }else{      /* 右：回车 */
          if(ime_cn && py_len>0){sh_input(py_buf);last_kb_bytes=py_len;py_len=0;py_buf[0]=0;py_ncand=0;draw_cand();}
          else sh_input("\r");
        }
        vibe(30);
      }
    }
    if(!browser_active && screen_on&&now_ms()-last_status>5000){last_status=now_ms();
      sync_time_async();  /* WLAN 就绪后后台校时；不阻塞触摸/渲染 */
      int t=read_theme_file();       /* theme 命令切主题：检测 /root/theme 变化 */
      if(t!=cur_theme)apply_theme(t);
      int fw=read_fontsize_file();   /* fontsize 命令调字号：检测 /root/fontsize 变化 */
      if(fw>0&&fw!=char_w)apply_fontsize(fw);
      else draw_statusbar();         /* 无变化仅刷新时间/电量 */
    }  /* 息屏不刷新状态栏（降载） */
    if(!browser_active && kb_hl>=0&&now_ms()>kb_deadline){int old=kb_hl;kb_hl=-1;draw_kb_key(old);do_pan();}
    if(!browser_active)do_pan();
  }
}
