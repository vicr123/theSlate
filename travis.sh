if [ $STAGE = "script" ]; then
  if [ $TRAVIS_OS_NAME = "linux" ]; then
    echo "[TRAVIS] Preparing build environment"
    source /opt/qt510/bin/qt510-env.sh
    echo "[TRAVIS] Building and installing the-libs"
    git clone https://github.com/vicr123/the-libs.git
    cd the-libs
    git checkout blueprint
    qmake
    make
    sudo make install INSTALL_ROOT=/
    cd ..
    
    echo "[TRAVIS] Building and installing extra CMake modules"
    git clone git://anongit.kde.org/extra-cmake-modules
    cd extra-cmake-modules
    mkdir build
    cd build
    cmake -DCMAKE_INSTALL_PREFIX=/usr ..
    make
    sudo make install
    cd ../..
    
    echo "[TRAVIS] Building and installing KDE Syntax Highlighting"
    git clone git://anongit.kde.org/syntax-highlighting.git
    cd syntax-highlighting
    mkdir build
    cd build
    cmake -DCMAKE_INSTALL_PREFIX=/usr -DECM_MKSPECS_INSTALL_DIR=$(qmake -query QMAKE_INSTALL_PLUGINS)/../mkspecs ..
    make
    sudo make install
    cd ../..
    
    echo "[TRAVIS] Running qmake"
    qmake
    echo "[TRAVIS] Building project"
    make
    echo "[TRAVIS] Installing into appdir"
    make install INSTALL_ROOT=~/appdir
    echo "[TRAVIS] Getting linuxdeployqt"
    wget -c -nv "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
    chmod a+x linuxdeployqt-continuous-x86_64.AppImage
    echo "[TRAVIS] Building AppImage"
    ./linuxdeployqt-continuous-x86_64.AppImage ~/appdir/usr/share/applications/*.desktop -bundle-non-qt-libs -extra-plugins=iconengines/libqsvgicon.so,imageformats/libqsvg.so
    ./linuxdeployqt-continuous-x86_64.AppImage ~/appdir/usr/share/applications/*.desktop -appimage -extra-plugins=iconengines/libqsvgicon.so,imageformats/libqsvg.so
  else
    echo "[TRAVIS] Building for macOS"
    export PATH="/usr/local/opt/qt/bin:$PATH"
    cd ..
    echo "[TRAVIS] Building and installing the-libs"
    git clone https://github.com/vicr123/the-libs.git
    cd the-libs
    git checkout blueprint
    qmake
    make
    sudo make install INSTALL_ROOT=/
    cd ..
    mkdir "build-theslate"
    cd "build-theslate"
    echo "[TRAVIS] Running qmake"
    qmake "INCLUDEPATH += /usr/local/opt/qt/include" "LIBS += -L/usr/local/opt/qt/lib" ../theSlate/theSlate.pro
    echo "[TRAVIS] Building project"
    make
    echo "[TRAVIS] Embedding the-libs"
    mkdir slate/theSlate.app/Contents/Libraries
    cp /usr/local/lib/libthe-libs*.dylib slate/theSlate.app/Contents/Libraries/
    install_name_tool -change libthe-libs.1.dylib @executable_path/../Libraries/libthe-libs.1.dylib slate/theSlate.app/Contents/MacOS/theSlate
    install_name_tool -change @rpath/QtWidgets.framework/Versions/5/QtWidgets @executable_path/../Frameworks/QtSvg.framework/Versions/5/QtWidgets slate/theSlate.app/Libraries/libthe-libs.1.dylib
    install_name_tool -change @rpath/QtWidgets.framework/Versions/5/QtGui @executable_path/../Frameworks/QtSvg.framework/Versions/5/QtGui slate/theSlate.app/Libraries/libthe-libs.1.dylib
    install_name_tool -change @rpath/QtWidgets.framework/Versions/5/QtCore @executable_path/../Frameworks/QtSvg.framework/Versions/5/QtCore slate/theSlate.app/Libraries/libthe-libs.1.dylib
    install_name_tool -change libthe-libs.1.dylib @executable_path/../Libraries/libthe-libs.1.dylib slate/theSlate.app/Contents/filebackends/libLocalFileBackend.dylib
    install_name_tool -change libthe-libs.1.dylib @executable_path/../Libraries/libthe-libs.1.dylib slate/theSlate.app/Contents/filebackends/libHttpBackend.dylib
    echo "[TRAVIS] Deploying Qt Libraries"
    macdeployqt slate/theSlate.app
    echo "[TRAVIS] Preparing Disk Image creator"
    npm install appdmg
    echo "[TRAVIS] Building Disk Image"
    ./node_modules/appdmg/bin/appdmg.js ./node-appdmg-config.json ~/theSlate-macOS.dmg
  fi
elif [ $STAGE = "before_install" ]; then
  if [ $TRAVIS_OS_NAME = "linux" ]; then
    wget -O ~/vicr12345.gpg.key https://vicr123.com/repo/apt/vicr12345.gpg.key
    sudo apt-key add ~/vicr12345.gpg.key
    sudo add-apt-repository 'deb https://vicr123.com/repo/apt/ubuntu bionic main'
    sudo add-apt-repository -y ppa:beineri/opt-qt-5.10.1-trusty
    sudo apt-get update -qq
    sudo apt-get install qt510-meta-minimal qt510x11extras qt510tools qt510translations qt510svg qt510websockets xorg-dev libxcb-util0-dev libgl1-mesa-dev
  else
    echo "[TRAVIS] Preparing to build for macOS"
    brew tap kde-mac/kde
    brew update
    brew install qt5 kf5-syntax-highlighting
  fi
elif [ $STAGE = "after_success" ]; then
  if [ $TRAVIS_OS_NAME = "linux" ]; then
    echo "[TRAVIS] Publishing AppImage"
    wget -c https://github.com/probonopd/uploadtool/raw/master/upload.sh
    bash upload.sh theSlate*.AppImage*
  else
    echo "[TRAVIS] Publishing Disk Image"
    cd ~
    wget -c https://github.com/probonopd/uploadtool/raw/master/upload.sh
    bash upload.sh theSlate-macOS.dmg
  fi
fi
