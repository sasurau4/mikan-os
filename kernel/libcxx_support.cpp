#include <new>
#include <cerrno>

int printk(const char *, ...);

std::new_handler std::get_new_handler() noexcept
{
    return []
    {
        printk("not enough memory\n");
        exit(1);
    };
}

extern "C" int posix_memalign(void **, std::size_t, std::size_t)
{
    return ENOMEM;
}