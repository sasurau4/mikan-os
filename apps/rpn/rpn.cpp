#include <cstring>
#include <cstdlib>
#include "../../kernel/graphics.hpp"

auto &printk = *reinterpret_cast<int (*)(const char *, ...)>(0x0000'0000'0010'b000);
auto &fill_rect = *reinterpret_cast<decltype(FillRectangle) *>(0x0000'0000'0010'c0e0);
auto &scrn_writer = *reinterpret_cast<decltype(screen_writer) *>(0x0000'0000'0024'a058);
int stack_ptr;
long stack[100];

long Pop()
{
    long value = stack[stack_ptr];
    --stack_ptr;
    return value;
}

void Push(long value)
{
    ++stack_ptr;
    stack[stack_ptr] = value;
}

extern "C" int main(int argc, char **argv)
{
    stack_ptr = -1;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "+") == 0)
        {
            long b = Pop();
            long a = Pop();
            Push(a + b);
            printk("[%d] <- %ld\n", stack_ptr, a + b);
        }
        else if (strcmp(argv[i], "-") == 0)
        {
            long b = Pop();
            long a = Pop();
            Push(a - b);
            printk("[%d] <- %ld\n", stack_ptr, a - b);
        }
        else
        {
            long a = atol(argv[i]);
            Push(a);
            printk("[%d] <- %ld\n", stack_ptr, a);
        }
    }

    fill_rect(*scrn_writer, Vector2D<int>{100, 10}, Vector2D<int>{200, 200}, ToColor(0x00ff00));
    if (stack_ptr < 0)
    {
        return 0;
    }
    return static_cast<int>(Pop());
}