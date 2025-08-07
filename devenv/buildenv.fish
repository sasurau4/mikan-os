# Usage: source buildenv.sh

set BASEDIR "$(path resolve (dirname (status --current-filename)))/x86_64-elf"
set EDK2DIR "$(path resolve $BASEDIR/../../../../tianocore/edk2)"

echo $BASEDIR
echo $EDK2DIR

if [ ! -d $BASEDIR ];
    echo "$BASEDIR does not exist. Please download x86_64-elf toolchain..."
    # wget  https://github.com/uchan-nos/mikanos-build/releases/download/v2.0/x86_64-elf.tar.gz -O - | tar zxvf -
else
    set -x CPPFLAGS "\
    -I$BASEDIR/include/c++/v1 -I$BASEDIR/include -I$BASEDIR/include/freetype2 \
    -I$EDK2DIR/MdePkg/Include -I$EDK2DIR/MdePkg/Include/X64 \
    -nostdlibinc -D__ELF__ -D_LDBL_EQ_DBL -D_GNU_SOURCE -D_POSIX_TIMERS \
    -DEFIAPI='__attribute__((ms_abi))'"
    set -x LDFLAGS "-L$BASEDIR/lib"
end