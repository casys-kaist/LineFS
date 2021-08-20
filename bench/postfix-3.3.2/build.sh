sudo postfix stop
sudo make clean; sudo make -j20
BUILD_SUCCESS="$?"
if [ $BUILD_SUCCESS -eq 0 ]; then
    sudo cp libexec/local.mlfs libexec/local
    sudo cp libexec/master.mlfs libexec/master
else
    exit 1
fi
if [ $BUILD_SUCCESS -eq 0 ]; then
    sudo make install \
    && sudo cp libexec/local /usr/libexec/postfix/local \
    && sudo cp libexec/master /usr/libexec/postfix/master \
    && sudo ls -alh /usr/libexec/postfix/local \
    && sudo ls -alh /usr/libexec/postfix/master \
    && sudo ldd /usr/libexec/postfix/local \
    && sudo postsuper -d ALL \
    && sudo postfix start
fi
