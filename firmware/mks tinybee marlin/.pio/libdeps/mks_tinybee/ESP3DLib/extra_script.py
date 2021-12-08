import shutil
shutil.rmtree("$TRAVIS_BUILD_DIR/Marlin/.pio/libdeps/esp32/ESP3DLib/src")
shutil.copytree("$TRAVIS_BUILD_DIR/src", "$TRAVIS_BUILD_DIR/Marlin/.pio/libdeps/esp32/ESP3DLib/")
print("Update ESP3DLib files")
