# EWhere - build all four targets on Kali
#
#   make            -> build all
#   make linux64    -> EWhere_linux_x64
#   make linux32    -> EWhere_linux_x86   (needs gcc-multilib)
#   make win64      -> EWhere_win_x64.exe (needs mingw-w64)
#   make win32      -> EWhere_win_x86.exe (needs mingw-w64)

SRC = BaseAPI.c CMD_Protocol.c SocksBase.c Sock_Tunnel.c \
      Lcx_Base.c ssocksd_pro.c rssocks_pro.c EWmain.c \
      libsock/Socks.c

CFLAGS   = -O2 -Wall -Wno-unused-parameter -s
LPTHREAD = -lpthread

all: linux64 linux32 win64 win32

WINCFLAGS = -DWIN32 -finput-charset=UTF-8 -fexec-charset=GBK

linux64:
	gcc $(CFLAGS) -o EWhere_linux_x64 $(SRC) $(LPTHREAD)

linux32:
	gcc -m32 $(CFLAGS) -o EWhere_linux_x86 $(SRC) $(LPTHREAD)

win64:
	x86_64-w64-mingw32-gcc $(CFLAGS) $(WINCFLAGS) -o EWhere_win_x64.exe $(SRC) -lws2_32

win32:
	i686-w64-mingw32-gcc $(CFLAGS) $(WINCFLAGS) -o EWhere_win_x86.exe $(SRC) -lws2_32

clean:
	rm -f EWhere_linux_x64 EWhere_linux_x86 EWhere_win_x64.exe EWhere_win_x86.exe

.PHONY: all linux64 linux32 win64 win32 clean
