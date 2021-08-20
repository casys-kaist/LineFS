sudo postfix stop
sudo make clean; sudo make -j20
# if [ $? -eq 0 ]; then
    # sudo mv libexec/local.mlfs libexec/local
# else
    # exit 1
# fi
if [ $? -eq 0 ]; then
    sudo make install \
    && sudo cp libexec/local /usr/libexec/postfix/local \
    && sudo cp libexec/master /usr/libexec/postfix/master \
    && sudo ls -alh /usr/libexec/postfix/local \
    && sudo ls -alh /usr/libexec/postfix/master \
    && sudo ldd /usr/libexec/postfix/local \
    && sudo postsuper -d ALL \
    && sudo postfix start
fi
