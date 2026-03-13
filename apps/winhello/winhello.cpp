#include <cstdlib>
#include "../syscall.h"

extern "C" void main(int argc, char **argv)
{
    auto [layer_id, err_openwin] = SyscallOpenWindow(200, 100, 10, 10, "winhello");
    if (err_openwin)
    {
        exit(err_openwin);
    }

    SyscallWinWriteString(layer_id, 7, 24, 0xc0'00'00, "hello world!");
    SyscallWinWriteString(layer_id, 24, 40, 0x00'c0'00, "hello world!");
    SyscallWinWriteString(layer_id, 40, 56, 0x00'00'c0, "hello world!");
    exit(0);
}