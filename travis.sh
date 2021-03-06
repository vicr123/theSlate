if [ $STAGE = "script" ]; then
  if [ $TRAVIS_OS_NAME = "linux" ]; then
    echo "[TRAVIS] Preparing build environment"
    source /opt/qt512/bin/qt512-env.sh
    echo "[TRAVIS] Building and installing the-libs"
    git clone https://github.com/vicr123/the-libs.git
    cd the-libs
    git checkout $TRAVIS_BRANCH
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
    git checkout v5.54.0
    mkdir build
    cd build
    cmake -DCMAKE_INSTALL_PREFIX=/usr -DECM_MKSPECS_INSTALL_DIR=/opt/qt512/mkspecs/modules/ ..
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
    ./linuxdeployqt-continuous-x86_64.AppImage ~/appdir/usr/share/applications/*.desktop -appimage -extra-plugins=iconengines/libqsvgicon.so,imageformats/libqsvg.so
  else
    echo "[TRAVIS] Building for macOS"
    export PATH="/usr/local/opt/qt/bin:$PATH"
    if [ "$TRAVIS_BRANCH" = "blueprint" ]; then
      THESLATE_APPPATH="slate/theSlate Blueprint.app"
      CONFIG_JSON=./node-appdmg-config-bp.json
    else
      THESLATE_APPPATH=slate/theSlate.app
      CONFIG_JSON=./node-appdmg-config.json
    fi
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
    mkdir "$THESLATE_APPPATH/Contents/Libraries"
    cp /usr/local/lib/libthe-libs*.dylib "$THESLATE_APPPATH/Contents/Libraries/"
    install_name_tool -change libthe-libs.1.dylib @executable_path/../Libraries/libthe-libs.1.dylib "$THESLATE_APPPATH/Contents/MacOS/theSlate"*
    install_name_tool -change @rpath/QtWidgets.framework/Versions/5/QtWidgets @executable_path/../Frameworks/QtSvg.framework/Versions/5/QtWidgets "$THESLATE_APPPATH/Libraries/libthe-libs.1.dylib"
    install_name_tool -change @rpath/QtWidgets.framework/Versions/5/QtGui @executable_path/../Frameworks/QtSvg.framework/Versions/5/QtGui "$THESLATE_APPPATH/Libraries/libthe-libs.1.dylib"
    install_name_tool -change @rpath/QtWidgets.framework/Versions/5/QtCore @executable_path/../Frameworks/QtSvg.framework/Versions/5/QtCore "$THESLATE_APPPATH/Libraries/libthe-libs.1.dylib"
    install_name_tool -change libthe-libs.1.dylib @executable_path/../Libraries/libthe-libs.1.dylib "$THESLATE_APPPATH/Contents/filebackends/libLocalFileBackend.dylib"
    install_name_tool -change libthe-libs.1.dylib @executable_path/../Libraries/libthe-libs.1.dylib "$THESLATE_APPPATH/Contents/filebackends/libHttpBackend.dylib"
    install_name_tool -change libthe-libs.1.dylib @executable_path/../Libraries/libthe-libs.1.dylib "$THESLATE_APPPATH/Contents/auxiliarypanes/libHtmlPreview.dylib"
    install_name_tool -change libthe-libs.1.dylib @executable_path/../Libraries/libthe-libs.1.dylib "$THESLATE_APPPATH/Contents/auxiliarypanes/libMdPreview.dylib"
    echo "[TRAVIS] Embedding astyle"
    mkdir "$THESLATE_APPPATH/Contents/bin"
    cp /usr/local/bin/astyle "$THESLATE_APPPATH/Contents/bin/"
    echo "[TRAVIS] Deploying Qt Libraries"
    macdeployqt "$THESLATE_APPPATH"
    echo "[TRAVIS] Preparing Disk Image creator"
    npm install appdmg
    echo "[TRAVIS] Building Disk Image"
    ./node_modules/appdmg/bin/appdmg.js $CONFIG_JSON ~/theSlate-macOS.dmg
  fi
elif [ $STAGE = "before_install" ]; then
  if [ $TRAVIS_OS_NAME = "linux" ]; then
    wget -O ~/vicr12345.gpg.key https://vicr123.com/repo/apt/vicr12345.gpg.key
    sudo apt-key add ~/vicr12345.gpg.key
    sudo add-apt-repository 'deb https://vicr123.com/repo/apt/ubuntu bionic main'
    sudo add-apt-repository -y ppa:beineri/opt-qt-5.12.3-xenial
    sudo apt-get update -qq
    sudo apt-get install qt512-meta-minimal qt512x11extras qt512tools qt512translations qt512svg qt512websockets qt512multimedia qt512webengine xorg-dev libxcb-util0-dev libgl1-mesa-dev cmake
  else
    echo "[TRAVIS] Preparing to build for macOS"
    brew tap kde-mac/kde
    brew update
    brew install qt5 kf5-syntax-highlighting astyle
  fi
elif [ $STAGE = "after_success" ]; then
  if [ $TRAVIS_OS_NAME = "linux" ]; then
    echo "[TRAVIS] Publishing AppImage"
    wget -c https://github.com/probonopd/uploadtool/raw/master/upload.sh
    cp theSlate*.AppImage theSlate-linux.AppImage
    cp theSlate*.AppImage.zsync theSlate-linux.AppImage.zsync
    bash upload.sh theSlate-linux.AppImage*
  else
    # Workaround for uploadtool not working correctly on macOS
#    export RELEASE_NAME="continuous"
#    export RELEASE_TITLE="Continuous build"
#    export is_prerelease="true"
    echo "[TRAVIS] Publishing Disk Image"
    cd ~
    wget -c https://raw.githubusercontent.com/probonopd/uploadtool/76e2330b548a3e1a71f80dd709b89593c4053e2f/upload.sh
    bash upload.sh theSlate-macOS.dmg
  fi
fi
